# Electronic Component Sorting System

This code deliverable implements five-class electronic component sorting with a Python upper host and an STM32F407 lower controller. The upper host runs YOLOv8-based detection/tracking, filters detections into final deliverable boxes, and sends UART sort commands to the firmware. The firmware receives physical box IDs and drives the sorting task, motor/servo timing, LCD count display, and UI state.

## Final Classes

| Box | Component class |
| --- | --- |
| 1 | diode |
| 2 | electrolytic-capacitor |
| 3 | polyester-capacitor |
| 4 | resistor |
| 5 | transistor |

`ceramic-capacitor` is not part of the final deliverable. The runtime filters model class `0` before display as valid, counting, serial transmission, or sorting.

## Repository Layout

- `vision/`: Python upper-host runtime, class mapping, serial packet builder, configuration, tools, and tests.
- `firmware/ComRecCtrl/`: STM32F407 CMake firmware project with HAL/CMSIS, LVGL UI, UART receive handling, and sorting control.
- `docs/`: submission-facing notes for model classes, serial protocol, and build/run steps.

## Quick Start

Install the Python dependencies:

```powershell
python -m pip install -r vision/requirements.txt
```

Run the sorter:

```powershell
python -m vision.run_sorter --camera 1 --serial-port COM13
```

Run tests:

```powershell
pytest -q
python -m pytest -q
```

The model weight file `vision/weights/best.pt` may be present in the local RAR package, but model weights are ignored by Git and are not committed by default.

Third-party and generated foundations are listed in `NOTICE.md`.
