from __future__ import annotations

MODEL_CLASS_NAMES: dict[int, str] = {
    0: "ceramic-capacitor",
    1: "diode",
    2: "electrolytic-capacitor",
    3: "polyester-capacitor",
    4: "resistor",
    5: "transistor",
}

BOX_LABELS: dict[int, str] = {
    1: "diode",
    2: "electrolytic-capacitor",
    3: "polyester-capacitor",
    4: "resistor",
    5: "transistor",
}

MODEL_CLASS_TO_BOX_ID: dict[int, int] = {
    1: 1,
    2: 2,
    3: 3,
    4: 4,
    5: 5,
}


def model_class_to_box_id(class_id: int) -> int | None:
    return MODEL_CLASS_TO_BOX_ID.get(int(class_id))


def is_deliverable_class(class_id: int) -> bool:
    return model_class_to_box_id(class_id) is not None


def label_for_model_class(class_id: int) -> str:
    class_id = int(class_id)
    box_id = model_class_to_box_id(class_id)
    if box_id is None:
        name = MODEL_CLASS_NAMES.get(class_id, f"unknown-{class_id}")
        return f"ignored: {name}"
    return f"box {box_id}: {BOX_LABELS[box_id]}"
