# 模型训练说明

本文档说明本项目 YOLOv8 检测模型的训练来源、类别顺序、训练命令和最终部署方式。

## 数据集格式

原始数据集采用 YOLOv8 目标检测格式，目录结构为：

```text
electronic-components.v3i.yolov8/
├─ train/images
├─ train/labels
├─ valid/images
├─ valid/labels
├─ test/images
├─ test/labels
└─ data.yaml
```

原始导出说明中记录数据集包含 799 张图片，并对图像做了 640x640 尺寸归一化、水平翻转、随机裁剪、亮度扰动和轻微高斯模糊等增强。GitHub 交付包不包含完整数据集，原因是数据和训练输出体积较大；如需复现训练，可将数据集放置到：

```text
vision/datasets/electronic_data/electronic-components.v3i.yolov8/
```

## 类别顺序

训练时的模型类别顺序为 6 类：

| 类别 ID | 类别名称 | 交付阶段处理 |
| --- | --- | --- |
| 0 | ceramic-capacitor | 忽略 |
| 1 | diode | 1 号框 |
| 2 | electrolytic-capacitor | 2 号框 |
| 3 | polyester-capacitor | 3 号框 |
| 4 | resistor | 4 号框 |
| 5 | transistor | 5 号框 |

这里的 6 类是模型训练层面的类别数；最终分拣任务只有 5 个有效框，因此运行时会过滤 `class_id == 0`。

## 训练命令

先准备基础权重：

```text
vision/weights/yolov8n.pt
```

再执行训练：

```powershell
python -m vision.tools.train_v8 --data vision/data.yaml --weights vision/weights/yolov8n.pt --epochs 100 --imgsz 640 --batch 16 --device 0 --name train
```

训练脚本默认使用离线模式，避免运行时自动下载模型导致环境不可控。如需允许 Ultralytics 自动下载基础权重，可以增加：

```powershell
--allow-download
```

## 训练参数

原始最终训练记录 `train6` 的主要参数为：

| 参数 | 取值 |
| --- | --- |
| model | `./yolov8n.pt` |
| data | `data.yaml` |
| epochs | `100` |
| batch | `16` |
| imgsz | `640` |
| device | `0` |
| optimizer | `auto` |
| pretrained | `true` |
| amp | `false` |

最终训练记录最后一轮的主要验证指标为：

| 指标 | 数值 |
| --- | --- |
| precision(B) | `0.99489` |
| recall(B) | `1.00000` |
| mAP50(B) | `0.99500` |
| mAP50-95(B) | `0.94856` |

## 部署方式

训练完成后，将最优权重复制到：

```text
vision/weights/best.pt
```

上位机默认从该路径加载模型：

```powershell
python -m vision.run_sorter --camera 1 --serial-port COM13
```

如果要临时指定其他权重，可以使用：

```powershell
python -m vision.run_sorter --model C:\path\to\best.pt --camera 1 --serial-port COM13
```
