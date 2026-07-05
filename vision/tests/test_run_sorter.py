import pytest

from vision.run_sorter import PROJECT_DIR, SerialSender, load_config


class FakeSerial:
    def __init__(self):
        self.writes = []

    def write(self, packet: bytes):
        self.writes.append(packet)


def write_config(
    tmp_path,
    *,
    camera_index=1,
    confidence=0.8,
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
  window_name: "Component Sorter"

model:
  path: "weights/best.pt"
  confidence: {confidence}
  tracker: "bytetrack.yaml"

trigger_zone:
  left_ratio: {left_ratio}
  right_ratio: {right_ratio}
  cooldown_sec: {cooldown_sec}

serial:
  port: "COM13"
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
