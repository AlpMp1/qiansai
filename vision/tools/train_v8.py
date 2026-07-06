"""Train the YOLOv8 detector used by the sorting runtime."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
VISION_DIR = SCRIPT_DIR.parent
DEFAULT_DATA = VISION_DIR / "data.yaml"
DEFAULT_WEIGHTS = VISION_DIR / "weights" / "yolov8n.pt"
DEFAULT_PROJECT = VISION_DIR / "runs" / "detect"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=DEFAULT_DATA, help="YOLOv8 data.yaml path.")
    parser.add_argument("--weights", type=Path, default=DEFAULT_WEIGHTS, help="Base YOLO weights.")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--batch", type=int, default=16)
    parser.add_argument("--device", default="0", help="CUDA device id or 'cpu'.")
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--name", default="train", help="Ultralytics run name.")
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--amp", action="store_true", help="Enable mixed precision training.")
    parser.add_argument("--plots", action="store_true", help="Save training plots.")
    parser.add_argument(
        "--allow-download",
        action="store_true",
        help="Allow Ultralytics to download missing base weights.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.allow_download:
        os.environ["ULTRALYTICS_OFFLINE"] = "True"

    if not args.data.exists():
        raise FileNotFoundError(f"Dataset config not found: {args.data}")
    if not args.weights.exists() and not args.allow_download:
        raise FileNotFoundError(
            f"Base weights not found: {args.weights}. "
            "Place yolov8n.pt there or rerun with --allow-download."
        )

    from ultralytics import YOLO

    model = YOLO(args.weights)
    model.train(
        data=str(args.data),
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device,
        workers=args.workers,
        project=str(args.project),
        name=args.name,
        pretrained=True,
        amp=args.amp,
        plots=args.plots,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
