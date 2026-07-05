# Development utility for dataset collection, label conversion, prediction, or training.
import os
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# 定义映射关系：{旧ID: 新ID}
# 旧(classes.txt) -> 新(data.yaml)
# 0(resistor) -> 4
# 1(diode) -> 1
# 2(transistor) -> 5
# 3(polyester-capacitor) -> 3
# 4(electrolytic-capacitor) -> 2
mapping = {
    '0': '4',
    '1': '1',
    '2': '5',
    '3': '3',
    '4': '2'
}

label_dir = str(SCRIPT_DIR / "captured_data") # 你的标注文件所在目录

for file_name in os.listdir(label_dir):
    if file_name.endswith(".txt") and file_name != "classes.txt":
        file_path = os.path.join(label_dir, file_name)
        
        with open(file_path, 'r') as f:
            lines = f.readlines()
            
        new_lines = []
        for line in lines:
            parts = line.split()
            if parts:
                old_id = parts[0]
                if old_id in mapping:
                    parts[0] = mapping[old_id] # 替换成正确ID
                    new_lines.append(" ".join(parts) + "\n")
                else:
                    new_lines.append(line) # 如果不在映射里，保持原样
        
        with open(file_path, 'w') as f:
            f.writelines(new_lines)

print("✅ 修复完成！所有标签 ID 已按 data.yaml 顺序重排。")
