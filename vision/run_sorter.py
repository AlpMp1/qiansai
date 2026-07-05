from __future__ import annotations

import argparse
import math
import queue
import sys
import threading
import time
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

try:
    import cv2
except ImportError:
    cv2 = None

try:
    import serial
except ImportError:
    serial = None

try:
    import yaml
except ImportError:
    yaml = None

try:
    from ultralytics import YOLO
except ImportError:
    YOLO = None

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from vision.component_mapping import BOX_LABELS, label_for_model_class, model_class_to_box_id
from vision.serial_protocol import SerialPacketConfig, build_sort_packet


PROJECT_DIR = Path(__file__).resolve().parent
TRACK_REARM_GRACE_FRAMES = 2
TrackKey = tuple[int, int]


@dataclass(frozen=True)
class RuntimeConfig:
    camera_index: int
    window_name: str
    model_path: Path
    conf_threshold: float
    tracker: str
    zone_left_ratio: float
    zone_right_ratio: float
    cooldown_sec: float
    serial_port: str
    baudrate: int
    timeout: float
    send_offset: bool
    queue_size: int


@dataclass
class TrackSendGate:
    grace_frames: int = TRACK_REARM_GRACE_FRAMES
    sent_keys: set[TrackKey] = field(default_factory=set)
    missed_frames: dict[TrackKey, int] = field(default_factory=dict)

    def can_send(self, key: TrackKey) -> bool:
        return key not in self.sent_keys

    def mark_sent(self, key: TrackKey) -> None:
        self.sent_keys.add(key)
        self.missed_frames.pop(key, None)

    def end_frame(self, current_in_zone_keys: set[TrackKey]) -> None:
        for key in current_in_zone_keys:
            self.missed_frames.pop(key, None)

        for key in list(self.sent_keys):
            if key in current_in_zone_keys:
                continue

            missed_count = self.missed_frames.get(key, 0) + 1
            if missed_count > self.grace_frames:
                self.sent_keys.remove(key)
                self.missed_frames.pop(key, None)
            else:
                self.missed_frames[key] = missed_count


class SerialSender(threading.Thread):
    def __init__(self, serial_port: serial.Serial, queue_size: int) -> None:
        super().__init__(daemon=True)
        self._serial = serial_port
        self._queue: queue.Queue[bytes] = queue.Queue(maxsize=max(1, int(queue_size)))
        self._stop_event = threading.Event()

    def send(self, packet: bytes) -> bool:
        if self._stop_event.is_set():
            return False
        try:
            self._queue.put_nowait(packet)
            return True
        except queue.Full:
            print("Serial send queue is full; dropping packet")
            return False

    def shutdown(self) -> None:
        self._stop_event.set()
        if self.ident is not None:
            self.join(timeout=1.0)
            if self.is_alive():
                print("Warning: serial sender thread did not stop before shutdown timeout")

    def run(self) -> None:
        while not self._stop_event.is_set() or not self._queue.empty():
            try:
                packet = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue

            try:
                self._serial.write(packet)
            except Exception as exc:
                print(f"Serial send failed: {exc}")
            finally:
                self._queue.task_done()


def _require_dependency(dependency: Any, package_name: str) -> Any:
    if dependency is None:
        raise RuntimeError(
            f"{package_name} is required. Install dependencies with "
            "`python -m pip install -r vision/requirements.txt`."
        )
    return dependency


def _cv2() -> Any:
    return _require_dependency(cv2, "opencv-python")


def _resolve_model_path(path: str | Path) -> Path:
    model_path = Path(path)
    if model_path.is_absolute():
        return model_path
    return PROJECT_DIR / model_path


def _config_section(data: Mapping[str, Any], name: str) -> Mapping[str, Any]:
    value = data.get(name, {})
    if not isinstance(value, Mapping):
        raise ValueError(f"config section '{name}' must be a mapping")
    return value


def _parse_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"true", "yes", "1"}:
            return True
        if normalized in {"false", "no", "0"}:
            return False
    raise ValueError("serial.send_offset must be a boolean")


def _parse_int(value: Any, field_name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field_name} must be an integer")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value.strip(), 10)
        except ValueError as exc:
            raise ValueError(f"{field_name} must be an integer") from exc
    raise ValueError(f"{field_name} must be an integer")


def _parse_float(value: Any, field_name: str) -> float:
    if isinstance(value, bool):
        raise ValueError(f"{field_name} must be a number")
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field_name} must be a number") from exc
    if not math.isfinite(parsed):
        raise ValueError(f"{field_name} must be finite")
    return parsed


def _parse_non_empty_str(value: Any, field_name: str) -> str:
    parsed = str(value)
    if not parsed.strip():
        raise ValueError(f"{field_name} must not be empty")
    return parsed


def _require_range(value: int | float, message: str, *, minimum: float | None = None, maximum: float | None = None) -> None:
    if minimum is not None and value < minimum:
        raise ValueError(message)
    if maximum is not None and value > maximum:
        raise ValueError(message)


def load_config(path: Path) -> RuntimeConfig:
    yaml_module = _require_dependency(yaml, "pyyaml")
    with path.open("r", encoding="utf-8") as config_file:
        data = yaml_module.safe_load(config_file)

    if not isinstance(data, Mapping):
        raise ValueError("config root must be a mapping")

    camera = _config_section(data, "camera")
    model = _config_section(data, "model")
    trigger_zone = _config_section(data, "trigger_zone")
    serial_config = _config_section(data, "serial")
    queue_config = _config_section(data, "queue")

    camera_index = _parse_int(camera.get("index", 0), "camera.index")
    window_name = _parse_non_empty_str(camera.get("window_name", "Component Sorter"), "camera.window_name")
    model_path = _parse_non_empty_str(model.get("path", "weights/best.pt"), "model.path")
    conf_threshold = _parse_float(model.get("confidence", 0.8), "model.confidence")
    tracker = _parse_non_empty_str(model.get("tracker", "bytetrack.yaml"), "model.tracker")
    zone_left_ratio = _parse_float(trigger_zone.get("left_ratio", 0.45), "trigger_zone.left_ratio")
    zone_right_ratio = _parse_float(trigger_zone.get("right_ratio", 0.55), "trigger_zone.right_ratio")
    cooldown_sec = _parse_float(trigger_zone.get("cooldown_sec", 0.5), "trigger_zone.cooldown_sec")
    serial_port = _parse_non_empty_str(serial_config.get("port", "COM13"), "serial.port")
    baudrate = _parse_int(serial_config.get("baudrate", 115200), "serial.baudrate")
    timeout = _parse_float(serial_config.get("timeout", 0.01), "serial.timeout")
    queue_size = _parse_int(queue_config.get("size", 200), "queue.size")

    _require_range(camera_index, "camera.index must be >= 0", minimum=0)
    _require_range(conf_threshold, "model.confidence must be between 0 and 1 inclusive", minimum=0, maximum=1)
    _require_range(
        zone_left_ratio,
        "trigger_zone.left_ratio must be between 0 and 1 inclusive",
        minimum=0,
        maximum=1,
    )
    _require_range(
        zone_right_ratio,
        "trigger_zone.right_ratio must be between 0 and 1 inclusive",
        minimum=0,
        maximum=1,
    )
    if zone_left_ratio >= zone_right_ratio:
        raise ValueError("left_ratio must be less than right_ratio")
    _require_range(cooldown_sec, "trigger_zone.cooldown_sec must be >= 0", minimum=0)
    _require_range(baudrate, "serial.baudrate must be > 0", minimum=1)
    _require_range(timeout, "serial.timeout must be >= 0", minimum=0)
    _require_range(queue_size, "queue.size must be > 0", minimum=1)

    return RuntimeConfig(
        camera_index=camera_index,
        window_name=window_name,
        model_path=_resolve_model_path(model_path),
        conf_threshold=conf_threshold,
        tracker=tracker,
        zone_left_ratio=zone_left_ratio,
        zone_right_ratio=zone_right_ratio,
        cooldown_sec=cooldown_sec,
        serial_port=serial_port,
        baudrate=baudrate,
        timeout=timeout,
        send_offset=_parse_bool(serial_config.get("send_offset", False)),
        queue_size=queue_size,
    )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the component sorter upper-host runtime.")
    parser.add_argument("--config", type=Path, default=PROJECT_DIR / "config.yaml", help="Runtime YAML config path.")
    parser.add_argument("--camera", type=int, default=None, help="Camera index override.")
    parser.add_argument("--serial-port", default=None, help="Serial port override.")
    parser.add_argument("--model", default=None, help="YOLO model path override.")
    return parser.parse_args(argv)


def apply_overrides(config: RuntimeConfig, args: argparse.Namespace) -> RuntimeConfig:
    values = {field: getattr(config, field) for field in RuntimeConfig.__dataclass_fields__}

    if args.camera is not None:
        values["camera_index"] = int(args.camera)
    if args.serial_port is not None:
        values["serial_port"] = str(args.serial_port)
    if args.model is not None:
        values["model_path"] = _resolve_model_path(args.model)

    return RuntimeConfig(**values)


def open_camera(camera_index: int) -> cv2.VideoCapture:
    cv2_module = _cv2()
    camera = cv2_module.VideoCapture(camera_index)
    if not camera.isOpened():
        camera.release()
        raise RuntimeError(f"Could not open camera index {camera_index}")
    return camera


def open_serial(config: RuntimeConfig) -> serial.Serial:
    serial_module = _require_dependency(serial, "pyserial")
    return serial_module.Serial(
        port=config.serial_port,
        baudrate=config.baudrate,
        timeout=config.timeout,
    )


def _scalar_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        if hasattr(value, "numel") and value.numel() == 0:
            return None
        if hasattr(value, "item"):
            return int(value.item())
        if isinstance(value, (list, tuple)):
            if not value:
                return None
            return int(value[0])
        return int(value)
    except (TypeError, ValueError):
        return None


def _box_xyxy(box: Any) -> tuple[int, int, int, int]:
    coords = box.xyxy[0]
    if hasattr(coords, "detach"):
        coords = coords.detach().cpu()
    if hasattr(coords, "tolist"):
        coords = coords.tolist()
    x1, y1, x2, y2 = coords[:4]
    return int(x1), int(y1), int(x2), int(y2)


def _draw_detection(frame: Any, xyxy: tuple[int, int, int, int], label: str, color: tuple[int, int, int]) -> None:
    cv2_module = _cv2()
    x1, y1, x2, y2 = xyxy
    cv2_module.rectangle(frame, (x1, y1), (x2, y2), color, 2)
    cv2_module.putText(frame, label, (x1, max(20, y1 - 8)), cv2_module.FONT_HERSHEY_SIMPLEX, 0.55, color, 2)


def _draw_trigger_zone(frame: Any, left_x: int, right_x: int) -> None:
    cv2_module = _cv2()
    height = frame.shape[0]
    color = (0, 255, 255)
    cv2_module.line(frame, (left_x, 0), (left_x, height), color, 2)
    cv2_module.line(frame, (right_x, 0), (right_x, height), color, 2)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    config = apply_overrides(load_config(args.config), args)
    packet_cfg = SerialPacketConfig(send_offset=config.send_offset)

    camera: cv2.VideoCapture | None = None
    serial_port: serial.Serial | None = None
    sender: SerialSender | None = None

    send_gate = TrackSendGate()
    last_trackless_send_at = 0.0

    try:
        yolo_class = _require_dependency(YOLO, "ultralytics")
        cv2_module = _cv2()

        model = yolo_class(str(config.model_path))
        camera = open_camera(config.camera_index)
        serial_port = open_serial(config)
        sender = SerialSender(serial_port, config.queue_size)
        sender.start()

        while True:
            ok, frame = camera.read()
            if not ok:
                print("Camera frame read failed")
                break

            current_in_zone_keys: set[TrackKey] = set()
            width = frame.shape[1]
            image_center_x = width // 2
            left_x = int(width * config.zone_left_ratio)
            right_x = int(width * config.zone_right_ratio)
            _draw_trigger_zone(frame, left_x, right_x)

            results = model.track(
                frame,
                conf=config.conf_threshold,
                tracker=config.tracker,
                persist=True,
                verbose=False,
            )

            now = time.monotonic()
            for result in results or []:
                boxes = getattr(result, "boxes", None)
                if boxes is None:
                    continue

                for box in boxes:
                    class_id = _scalar_int(box.cls)
                    if class_id is None:
                        continue

                    xyxy = _box_xyxy(box)
                    x1, _, x2, _ = xyxy
                    x_center = (x1 + x2) // 2
                    track_id = _scalar_int(getattr(box, "id", None))

                    box_id = model_class_to_box_id(class_id)
                    if box_id is None:
                        _draw_detection(frame, xyxy, label_for_model_class(class_id), (128, 128, 128))
                        continue

                    in_zone = left_x <= x_center <= right_x
                    label = f"box {box_id}: {BOX_LABELS[box_id]}"
                    if track_id is not None:
                        label = f"{label} id:{track_id}"

                    track_key: TrackKey | None = None
                    if track_id is not None and in_zone:
                        track_key = (track_id, box_id)
                        current_in_zone_keys.add(track_key)

                    sent_packet = False
                    if in_zone and sender is not None:
                        should_send = False
                        if track_key is not None:
                            should_send = send_gate.can_send(track_key)
                        elif now - last_trackless_send_at >= config.cooldown_sec:
                            should_send = True

                        if should_send:
                            packet = build_sort_packet(
                                box_id,
                                x_offset=x_center - image_center_x,
                                cfg=packet_cfg,
                            )
                            if sender.send(packet):
                                if track_key is not None:
                                    send_gate.mark_sent(track_key)
                                else:
                                    last_trackless_send_at = now
                                sent_packet = True

                    if sent_packet:
                        color = (0, 0, 255)
                    elif in_zone:
                        color = (0, 255, 0)
                    else:
                        color = (255, 160, 0)
                    _draw_detection(frame, xyxy, label, color)

            send_gate.end_frame(current_in_zone_keys)

            cv2_module.imshow(config.window_name, frame)
            key = cv2_module.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break
    except KeyboardInterrupt:
        pass
    finally:
        if sender is not None:
            try:
                sender.shutdown()
            except Exception as exc:
                print(f"Warning: sender shutdown failed: {exc}")
        if serial_port is not None:
            try:
                serial_port.close()
            except Exception as exc:
                print(f"Warning: serial port close failed: {exc}")
        if camera is not None:
            try:
                camera.release()
            except Exception as exc:
                print(f"Warning: camera release failed: {exc}")
        if cv2 is not None:
            try:
                cv2.destroyAllWindows()
            except Exception as exc:
                print(f"Warning: cv2 window cleanup failed: {exc}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
