# Serial Protocol

The default UART packet sent from the Python upper host to the STM32 firmware is:

```text
0xAA box_id 0x55
```

`box_id` is the physical box ID, not the raw YOLO model class.

| Box ID | Component class |
| --- | --- |
| 1 | diode |
| 2 | electrolytic-capacitor |
| 3 | polyester-capacitor |
| 4 | resistor |
| 5 | transistor |

The firmware ignores invalid IDs outside `1..5`. The upper host also filters model class `0` before sending, so `ceramic-capacitor` does not produce a packet.

The Python packet builder also implements an optional offset packet format, disabled by default in `vision/config.yaml`:

```text
0xAA box_id x_offset_hi x_offset_lo 0x55
```

`x_offset_hi` and `x_offset_lo` encode `x_offset` as a signed int16 in big-endian byte order. The submitted default configuration uses the three-byte packet.
