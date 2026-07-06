# Build and Run

Unless specified otherwise, commands are intended to be run from the repository root.

## Upper Host

Install dependencies:

```powershell
python -m pip install -r vision/requirements.txt
```

Running inference requires a local model weight file at `vision/weights/best.pt`. GitHub ignores `.pt` weights; this file may be present in the local RAR package, or it must be provided separately before running the sorter.

Run the sorter:

```powershell
python -m vision.run_sorter --camera 1 --serial-port COM13
```

Runtime defaults live in `vision/config.yaml`. The command line can override the config path, camera index, serial port, and model path:

```powershell
python -m vision.run_sorter --config vision/config.yaml --camera 1 --serial-port COM13 --model weights/best.pt
```

Relative model paths passed to `--model` are resolved under the `vision` directory, so `weights/best.pt` resolves to `vision/weights/best.pt`. To use a model outside the repository, pass an absolute path.

Run tests:

```powershell
pytest -q
python -m pytest -q
```

Raw training data and captured datasets are intentionally not included in GitHub and are ignored by `.gitignore`. Development tools for training or collection expect local data files when those workflows are needed.

## Firmware

Configure the STM32 firmware build:

```powershell
cmake -S firmware\ComRecCtrl -B firmware\ComRecCtrl\build\Debug -DCMAKE_BUILD_TYPE=Debug
```

Build it:

```powershell
cmake --build firmware\ComRecCtrl\build\Debug
```

The firmware CMake build was not verified in this cleanup environment because `cmake` was not available on `PATH`. Install CMake and ARM GCC before building.

Files such as `.elf`, `.hex`, and `.bin` are build artifacts. They are ignored by Git and are not committed.
