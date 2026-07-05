import pytest

from vision.serial_protocol import SerialPacketConfig, build_sort_packet


def test_builds_default_three_byte_packet():
    assert build_sort_packet(1) == bytes([0xAA, 0x01, 0x55])
    assert build_sort_packet(5) == bytes([0xAA, 0x05, 0x55])


def test_rejects_invalid_box_ids():
    with pytest.raises(ValueError, match="box_id must be in 1..5"):
        build_sort_packet(0)

    with pytest.raises(ValueError, match="box_id must be in 1..5"):
        build_sort_packet(6)


def test_builds_offset_packet_when_enabled():
    cfg = SerialPacketConfig(send_offset=True)
    assert build_sort_packet(3, x_offset=-2, cfg=cfg) == bytes([0xAA, 0x03, 0xFF, 0xFE, 0x55])
