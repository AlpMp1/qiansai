# 电子元件视觉识别与分拣系统

本项目是一个面向电子元件分拣任务的完整代码交付包，包含 Python 上位机视觉识别程序和 STM32F407 下位机控制程序。上位机使用 YOLOv8 完成目标检测与跟踪，根据最终比赛要求把有效元件映射到 5 个分拣框，并通过串口向下位机发送分拣指令；下位机负责串口接收、分拣任务调度、电机/舵机控制、LCD 计数显示和 UI 状态更新。

## 最终分拣类别

模型权重保留 6 个类别索引，但最终作品只启用 5 个分拣框。`ceramic-capacitor` 作为兼容旧模型权重的忽略类保留在模型索引中，不参与计数、发包和分拣。

| 模型类别 ID | 元件类别 | 最终处理 |
| --- | --- | --- |
| 0 | ceramic-capacitor / 陶瓷电容 | 忽略，不进入任何分拣框 |
| 1 | diode / 二极管 | 1 号框 |
| 2 | electrolytic-capacitor / 电解电容 | 2 号框 |
| 3 | polyester-capacitor / 涤纶电容 | 3 号框 |
| 4 | resistor / 电阻 | 4 号框 |
| 5 | transistor / 三极管 | 5 号框 |

也就是说，识别模型本身能够输出 6 类，但比赛交付逻辑只承认 1 到 5 号框。陶瓷电容即使被模型检测到，也会在上位机过滤阶段被丢弃，不会发送给 STM32。

## 模型训练说明

检测模型基于 Ultralytics YOLOv8n 训练。原始工程中的数据集为 YOLOv8 格式，包含 `train/images`、`valid/images` 和 `test/images`，类别顺序与 `vision/data.yaml` 保持一致：

```text
0 ceramic-capacitor
1 diode
2 electrolytic-capacitor
3 polyester-capacitor
4 resistor
5 transistor
```

原始训练记录中使用的主要参数如下：

| 参数 | 取值 |
| --- | --- |
| 基础模型 | `yolov8n.pt` |
| 训练轮数 | `100` |
| 输入尺寸 | `640` |
| batch size | `16` |
| 设备 | `device=0` |
| 混合精度 | `amp=False` |
| 最终权重 | `runs/detect/train6/weights/best.pt` |

本交付包保留了可复现训练入口：

```powershell
python -m vision.tools.train_v8 --data vision/data.yaml --weights vision/weights/yolov8n.pt --epochs 100 --imgsz 640 --batch 16 --device 0 --name train
```

训练完成后，将生成的 `runs/detect/<name>/weights/best.pt` 放到：

```text
vision/weights/best.pt
```

即可供上位机运行时加载。完整训练流程、数据集放置方式和指标说明见 [docs/training.md](docs/training.md)。

说明：GitHub 开源仓库默认不提交 `.pt` 权重、训练数据集和训练输出目录；比赛 RAR 包可以按要求包含本地的 `vision/weights/best.pt`。

## 快速运行

以下命令默认在仓库根目录执行。

安装 Python 依赖：

```powershell
python -m pip install -r vision/requirements.txt
```

运行上位机分拣程序：

```powershell
python -m vision.run_sorter --camera 1 --serial-port COM13
```

运行测试：

```powershell
pytest -q
python -m pytest -q
```

上位机运行需要本地存在模型文件 `vision/weights/best.pt`。该文件已被 `.gitignore` 忽略，适合放入比赛 RAR 包，但不建议提交到 GitHub。

## 工程结构

```text
competition-code/
├─ vision/                  Python 上位机代码
│  ├─ run_sorter.py          视觉检测、触发区域判断、串口发送主程序
│  ├─ component_mapping.py   模型类别到分拣框的映射
│  ├─ serial_protocol.py     串口协议打包与校验
│  ├─ config.yaml            运行配置
│  ├─ data.yaml              YOLOv8 数据集配置
│  ├─ tools/                 数据采集、标签转换、训练和预览工具
│  └─ tests/                 上位机单元测试
├─ firmware/ComRecCtrl/      STM32F407 下位机工程
├─ docs/                     模型类别、串口协议、训练和构建说明
├─ NOTICE.md                 第三方库与生成代码说明
└─ README.md                 项目说明
```

## 相对原始工程的整理

本目录不是简单复制原始工程，而是面向比赛提交重新整理后的代码包，主要改动如下：

- 明确最终只有 5 个分拣框，模型类别 `0` 陶瓷电容统一作为忽略类处理。
- 将上位机从零散脚本整理为可导入、可测试的 `vision` 模块，拆分出类别映射、串口协议、运行配置和主程序。
- 增加测试用例，覆盖类别映射、串口打包、重复触发抑制和忽略类过滤。
- 清理训练、采集、预测脚本中的乱码和硬编码路径，保留正式的命令行入口。
- 将 STM32 固件中的分拣 ID 口径整理为 `Box 1..5`，无效 ID 和陶瓷电容不会更新计数。
- 删除构建产物、缓存、运行输出、IDE 配置、临时图片和无关素材，降低 RAR/GitHub 提交噪声。
- 补充中文 README、训练说明、串口协议说明、构建运行说明和第三方声明，方便评审和讲解视频使用。

## 交付说明

- GitHub：建议提交本目录的代码、配置、文档和测试，不提交 `.pt` 权重和数据集。
- RAR：按比赛要求可以包含 `vision/weights/best.pt`，这样评审机器安装依赖后可直接运行上位机。
- 固件：本机当前可找到 ARM GCC，但没有检测到 CMake/Ninja，因此固件构建命令已写入文档，但未在本机完成实际构建验证。
