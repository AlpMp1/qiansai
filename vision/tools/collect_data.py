# Development utility for dataset collection, label conversion, prediction, or training.
import cv2
import os
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# 1. 创建保存图片的文件夹
save_path = str(SCRIPT_DIR.parent / "captured_data")
if not os.path.exists(save_path):
    os.makedirs(save_path)

cap = cv2.VideoCapture(2)

print("--- 抓拍模式已启动 ---")
print("操作指南：")
print("  - 按 's' 键：保存当前画面")
print("  - 按 'q' 键：退出")

count = 0
while True:
    ret, frame = cap.read()
    if not ret:
        break

    # 在屏幕上显示提示信息
    display_frame = frame.copy()
    cv2.putText(display_frame, f"Saved: {count}", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.imshow("Data Collector", display_frame)

    key = cv2.waitKey(1)
    if key & 0xFF == ord('s'):
        # 以时间戳命名，防止覆盖
        img_name = os.path.join(save_path, f"component_{int(time.time())}.jpg")
        cv2.imwrite(img_name, frame)
        count += 1
        print(f"已保存第 {count} 张图片: {img_name}")

    elif key & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
