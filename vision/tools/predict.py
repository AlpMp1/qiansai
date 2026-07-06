"""Preview the trained detector while applying the final five-box policy."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import cv2
from ultralytics import YOLO

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from vision.component_mapping import model_class_to_box_id

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_MODEL = SCRIPT_DIR.parent / "weights" / "best.pt"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--camera", type=int, default=2, help="OpenCV camera index.")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL, help="YOLO model path.")
    parser.add_argument("--conf", type=float, default=0.5, help="Confidence threshold.")
    return parser.parse_args()


def draw_valid_detections(frame, result, model) -> None:
    names = getattr(model, "names", {}) or {}

    for box in result.boxes:
        class_id = int(box.cls[0])
        box_id = model_class_to_box_id(class_id)
        if box_id is None:
            continue

        x1, y1, x2, y2 = [int(value) for value in box.xyxy[0]]
        score = float(box.conf[0])
        class_name = names.get(class_id, str(class_id))
        label = f"{class_name} -> box {box_id} {score:.2f}"

        cv2.rectangle(frame, (x1, y1), (x2, y2), (64, 180, 90), 2)
        cv2.putText(
            frame,
            label,
            (x1, max(20, y1 - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            (64, 180, 90),
            2,
            cv2.LINE_AA,
        )


def main() -> int:
    args = parse_args()
    if not args.model.exists():
        raise FileNotFoundError(f"Model not found: {args.model}")

    model = YOLO(args.model)
    cap = cv2.VideoCapture(args.camera)
    if not cap.isOpened():
        raise RuntimeError(f"Unable to open camera index {args.camera}")

    print("Press 'q' to quit. Class 0 ceramic-capacitor is ignored in this preview.")

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break

            results = model.predict(source=frame, show=False, stream=True, conf=args.conf, verbose=False)
            for result in results:
                preview = frame.copy()
                draw_valid_detections(preview, result, model)
                cv2.imshow("Electronic Components Detection", preview)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
