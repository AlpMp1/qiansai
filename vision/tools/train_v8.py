# Development utility for dataset collection, label conversion, prediction, or training.
import os
from pathlib import Path

os.environ["ULTRALYTICS_OFFLINE"] = "True"  # 这一行是灵魂，强制开启离线模式

SCRIPT_DIR = Path(__file__).resolve().parent

from ultralytics import YOLO

# 1. 明确加载你刚才下载好的本地 yolov8n.pt 文件
model = YOLO(SCRIPT_DIR / "yolov8n.pt")

# 2. 开始训练，设置 100 轮
# 我们在这里直接传参，不给它乱跑的机会
model.train(
    data=str(SCRIPT_DIR / "data.yaml"),
    epochs=100,
    imgsz=640,
    device=0,
    batch=16,
    amp=False,   # <--- 加上这一行，强制关闭混合精度检查
    plots=False
)
