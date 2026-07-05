from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class SerialPacketConfig:
    frame_head: int = 0xAA
    frame_tail: int = 0x55
    send_offset: bool = False


def build_sort_packet(box_id: int, *, x_offset: int = 0, cfg: SerialPacketConfig | None = None) -> bytes:
    cfg = cfg or SerialPacketConfig()
    box_id = int(box_id)
    if box_id < 1 or box_id > 5:
        raise ValueError("box_id must be in 1..5")

    if not cfg.send_offset:
        return bytes([cfg.frame_head, box_id, cfg.frame_tail])

    x_offset = max(-32768, min(32767, int(x_offset)))
    offset_bytes = x_offset.to_bytes(2, byteorder="big", signed=True)
    return bytes([cfg.frame_head, box_id]) + offset_bytes + bytes([cfg.frame_tail])
