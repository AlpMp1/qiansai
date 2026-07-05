# Development utility for dataset collection, label conversion, prediction, or training.
import os
from pathlib import Path

os.environ["ULTRALYTICS_OFFLINE"] = "True"  # 这一行是灵魂，强制开启离线模式

SCRIPT_DIR = Path(__file__).resolve().parent

from ultralytics import YOLO

MODEL_PATH = SCRIPT_DIR.parent / "weights" / "yolov8n.pt"
DATA_YAML = SCRIPT_DIR.parent / "data.yaml"
OFFLINE_MODE = os.environ.get("ULTRALYTICS_OFFLINE", "").lower() in {"1", "true", "yes"}

if not DATA_YAML.exists():
    raise FileNotFoundError(f"Dataset config not found: {DATA_YAML}")
if OFFLINE_MODE and not MODEL_PATH.exists():
    raise FileNotFoundError(f"Base weights not found: {MODEL_PATH}")

# 1. 明确加载本地 yolov8n.pt 文件
model = YOLO(MODEL_PATH)

# 2. 开始训练，设置 100 轮
# 我们在这里直接传参，不给它乱跑的机会
model.train(
    data=str(DATA_YAML),
    epochs=100,
    imgsz=640,
    device=0,
    batch=16,
    amp=False,   # <--- 加上这一行，强制关闭混合精度检查
    plots=False
)
