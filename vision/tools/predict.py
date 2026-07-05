# Development utility for dataset collection, label conversion, prediction, or training.
from pathlib import Path
from ultralytics import YOLO
import cv2

SCRIPT_DIR = Path(__file__).resolve().parent

# 1. 加载你训练好的模型
MODEL_PATH = SCRIPT_DIR.parent / "weights" / "best.pt"
if not MODEL_PATH.exists():
    raise FileNotFoundError(f"Model not found: {MODEL_PATH}")

model = YOLO(MODEL_PATH)

# 2. 打开摄像头 (0 是默认摄像头)
cap = cv2.VideoCapture(2)

# 检查摄像头是否正常打开
if not cap.isOpened():
    print("错误：无法打开摄像头")
    exit()

print("按下 'q' 键退出实时监测")

while True:
    # 读取摄像头的一帧
    ret, frame = cap.read()
    if not ret:
        break

    # 3. 对这一帧进行预测
    # stream=True 可以让处理更流畅，显存占用更低
    results = model.predict(source=frame, show=False, stream=True, conf=0.5)

    # 4. 在窗口中绘制结果
    for r in results:
        annotated_frame = r.plot() # plot() 会自动画好框和标签
        cv2.imshow("Electronic Components Detection", annotated_frame)

    # 按下 'q' 键退出
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# 释放资源
cap.release()
cv2.destroyAllWindows()
