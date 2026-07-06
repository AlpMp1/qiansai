# Build and Run

## Upper Host

Install dependencies:

```powershell
python -m pip install -r vision/requirements.txt
```

Run the sorter:

```powershell
python -m vision.run_sorter --camera 1 --serial-port COM13
```

Runtime defaults live in `vision/config.yaml`. The command line can override the config path, camera index, serial port, and model path:

```powershell
python -m vision.run_sorter --config vision/config.yaml --camera 1 --serial-port COM13 --model vision/weights/best.pt
```

Run tests:

```powershell
pytest -q
python -m pytest -q
```

The expected model weight location is `vision/weights/best.pt`. This file may be present in the local RAR package, but model weights and exported inference artifacts are ignored by Git and are not committed by default.

## Firmware

Configure the STM32 firmware build:

```powershell
cmake -S firmware\ComRecCtrl -B firmware\ComRecCtrl\build\Debug -DCMAKE_BUILD_TYPE=Debug
```

Build it:

```powershell
cmake --build firmware\ComRecCtrl\build\Debug
```

If CMake or the ARM embedded toolchain is missing, install them before building.

Files such as `.elf`, `.hex`, and `.bin` are build artifacts. They are ignored by Git and are not committed.
