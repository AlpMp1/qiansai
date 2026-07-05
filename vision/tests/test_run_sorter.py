import pytest

import vision.run_sorter as run_sorter
from vision.run_sorter import (
    PROJECT_DIR,
    SerialSender,
    TrackSendGate,
    apply_overrides,
    load_config,
    parse_args,
)


class FakeSerial:
    def __init__(self):
        self.writes = []

    def write(self, packet: bytes):
        self.writes.append(packet)


def write_config(
    tmp_path,
    *,
    camera_index=1,
    window_name='"Component Sorter"',
    model_path='"weights/best.pt"',
    confidence=0.8,
    tracker='"bytetrack.yaml"',
    serial_port='"COM13"',
    send_offset="false",
    left_ratio=0.45,
    right_ratio=0.55,
    cooldown_sec=0.5,
    baudrate=115200,
    timeout=0.01,
    queue_size=200,
):
    config_path = tmp_path / "config.yaml"
    config_path.write_text(
        f"""
camera:
  index: {camera_index}
  window_name: {window_name}

model:
  path: {model_path}
  confidence: {confidence}
  tracker: {tracker}

trigger_zone:
  left_ratio: {left_ratio}
  right_ratio: {right_ratio}
  cooldown_sec: {cooldown_sec}

serial:
  port: {serial_port}
  baudrate: {baudrate}
  timeout: {timeout}
  send_offset: {send_offset}

queue:
  size: {queue_size}
""",
        encoding="utf-8",
    )
    return config_path


def test_load_config_parses_values_and_false_string(tmp_path):
    config = load_config(write_config(tmp_path, send_offset='"false"'))

    assert config.send_offset is False
    assert config.model_path == PROJECT_DIR / "weights" / "best.pt"
    assert config.camera_index == 1
    assert config.conf_threshold == 0.8
    assert config.zone_left_ratio == 0.45
    assert config.zone_right_ratio == 0.55
    assert config.cooldown_sec == 0.5
    assert config.baudrate == 115200
    assert config.timeout == 0.01
    assert config.queue_size == 200


def test_load_config_rejects_invalid_trigger_ratios(tmp_path):
    with pytest.raises(ValueError, match="left_ratio must be less than right_ratio"):
        load_config(write_config(tmp_path, left_ratio=0.55, right_ratio=0.55))


def test_load_config_rejects_invalid_boolean_string(tmp_path):
    with pytest.raises(ValueError, match="serial.send_offset must be a boolean"):
        load_config(write_config(tmp_path, send_offset="maybe"))


def test_load_config_rejects_non_integer_camera_index(tmp_path):
    with pytest.raises(ValueError, match="camera.index must be an integer"):
        load_config(write_config(tmp_path, camera_index="abc"))


def test_load_config_rejects_fractional_queue_size(tmp_path):
    with pytest.raises(ValueError, match="queue.size must be an integer"):
        load_config(write_config(tmp_path, queue_size=1.9))


def test_load_config_rejects_non_finite_confidence(tmp_path):
    with pytest.raises(ValueError, match="model.confidence must be finite"):
        load_config(write_config(tmp_path, confidence=".nan"))


def test_load_config_rejects_empty_serial_port(tmp_path):
    with pytest.raises(ValueError, match="serial.port must not be empty"):
        load_config(write_config(tmp_path, serial_port='"   "'))


def test_load_config_rejects_non_string_model_path(tmp_path):
    with pytest.raises(ValueError, match="model.path must be a string"):
        load_config(write_config(tmp_path, model_path=123))


def test_apply_overrides_rejects_negative_camera_override(tmp_path):
    base_config = load_config(write_config(tmp_path))
    args = parse_args(["--camera", "-1"])

    with pytest.raises(ValueError, match="camera.index must be >= 0"):
        apply_overrides(base_config, args)


def test_apply_overrides_rejects_empty_serial_port_override(tmp_path):
    base_config = load_config(write_config(tmp_path))
    args = parse_args(["--serial-port", ""])

    with pytest.raises(ValueError, match="serial.port must not be empty"):
        apply_overrides(base_config, args)


def test_apply_overrides_rejects_empty_model_override(tmp_path):
    base_config = load_config(write_config(tmp_path))
    args = parse_args(["--model", ""])

    with pytest.raises(ValueError, match="model.path must not be empty"):
        apply_overrides(base_config, args)


def test_load_config_rejects_non_mapping_root(tmp_path):
    config_path = tmp_path / "config.yaml"
    config_path.write_text("- not\n- a mapping\n", encoding="utf-8")

    with pytest.raises(ValueError, match="config root must be a mapping"):
        load_config(config_path)


def test_load_config_rejects_non_mapping_section(tmp_path):
    config_path = tmp_path / "config.yaml"
    config_path.write_text("camera: 1\n", encoding="utf-8")

    with pytest.raises(ValueError, match="config section 'camera' must be a mapping"):
        load_config(config_path)


@pytest.mark.parametrize(
    ("field", "value", "message"),
    [
        ("camera_index", -1, "camera.index must be >= 0"),
        ("confidence", 1.01, "model.confidence must be between 0 and 1"),
        ("left_ratio", -0.01, "trigger_zone.left_ratio must be between 0 and 1"),
        ("right_ratio", 1.01, "trigger_zone.right_ratio must be between 0 and 1"),
        ("cooldown_sec", -0.01, "trigger_zone.cooldown_sec must be >= 0"),
        ("baudrate", 0, "serial.baudrate must be > 0"),
        ("timeout", -0.01, "serial.timeout must be >= 0"),
        ("queue_size", 0, "queue.size must be > 0"),
    ],
)
def test_load_config_rejects_invalid_numeric_values(tmp_path, field, value, message):
    with pytest.raises(ValueError, match=message):
        load_config(write_config(tmp_path, **{field: value}))


def test_serial_sender_shutdown_before_start_does_not_raise():
    sender = SerialSender(FakeSerial(), 1)

    sender.shutdown()


def test_serial_sender_shutdown_with_full_queue_returns_cleanly():
    sender = SerialSender(FakeSerial(), 1)
    assert sender.send(b"a") is True

    sender.start()
    sender.shutdown()

    assert sender.is_alive() is False


def test_track_send_gate_suppresses_while_visible_in_zone():
    gate = TrackSendGate(grace_frames=2)
    track_id = 7

    assert gate.can_send(track_id) is True
    gate.mark_sent(track_id)
    assert gate.can_send(track_id) is False

    gate.end_frame({track_id})
    assert gate.can_send(track_id) is False


def test_track_send_gate_gracefully_ignores_single_frame_dropout():
    gate = TrackSendGate(grace_frames=2)
    track_id = 7

    gate.mark_sent(track_id)
    gate.end_frame(set())

    assert gate.can_send(track_id) is False

    gate.end_frame({track_id})
    assert gate.can_send(track_id) is False


def test_track_send_gate_rearms_after_grace_frames():
    gate = TrackSendGate(grace_frames=2)
    track_id = 7

    gate.mark_sent(track_id)
    gate.end_frame(set())
    gate.end_frame(set())
    assert gate.can_send(track_id) is False

    gate.end_frame(set())
    assert gate.can_send(track_id) is True


def test_track_send_gate_suppresses_class_flicker_while_in_zone():
    gate = TrackSendGate(grace_frames=2)
    track_id = 7

    gate.mark_sent(track_id)
    gate.end_frame({track_id})

    assert gate.can_send(track_id) is False


def test_main_suppresses_duplicate_send_when_class_flickers_in_zone(tmp_path, monkeypatch):
    config_path = write_config(tmp_path)
    fake_sender_instances = []

    class FakeFrame:
        shape = (100, 100, 3)

    class FakeCamera:
        def __init__(self):
            self.frames = [FakeFrame(), FakeFrame()]

        def read(self):
            if not self.frames:
                return False, None
            return True, self.frames.pop(0)

        def release(self):
            pass

    class FakeSerialPort:
        def close(self):
            pass

    class FakeSerialSender:
        def __init__(self, serial_port, queue_size):
            self.sent_packets = []
            fake_sender_instances.append(self)

        def start(self):
            pass

        def send(self, packet):
            self.sent_packets.append(packet)
            return True

        def shutdown(self):
            pass

    class FakeBox:
        def __init__(self, class_id):
            self.cls = class_id
            self.id = 7
            self.xyxy = [[45, 10, 55, 20]]

    class FakeResult:
        def __init__(self, class_id):
            self.boxes = [FakeBox(class_id)]

    class FakeYOLO:
        def __init__(self, model_path):
            self.class_ids = [3, 4]

        def track(self, *args, **kwargs):
            return [FakeResult(self.class_ids.pop(0))]

    class FakeCV2:
        FONT_HERSHEY_SIMPLEX = 0

        def __init__(self):
            self.wait_keys = [0, ord("q")]

        def line(self, *args, **kwargs):
            pass

        def rectangle(self, *args, **kwargs):
            pass

        def putText(self, *args, **kwargs):
            pass

        def imshow(self, *args, **kwargs):
            pass

        def waitKey(self, delay):
            return self.wait_keys.pop(0)

        def destroyAllWindows(self):
            pass

    monkeypatch.setattr(run_sorter, "YOLO", FakeYOLO)
    monkeypatch.setattr(run_sorter, "cv2", FakeCV2())
    monkeypatch.setattr(run_sorter, "open_camera", lambda camera_index: FakeCamera())
    monkeypatch.setattr(run_sorter, "open_serial", lambda config: FakeSerialPort())
    monkeypatch.setattr(run_sorter, "SerialSender", FakeSerialSender)

    assert run_sorter.main(["--config", str(config_path)]) == 0
    assert len(fake_sender_instances) == 1
    assert len(fake_sender_instances[0].sent_packets) == 1
