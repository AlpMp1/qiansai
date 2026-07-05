from vision.component_mapping import (
    BOX_LABELS,
    MODEL_CLASS_NAMES,
    is_deliverable_class,
    label_for_model_class,
    model_class_to_box_id,
)


def test_ceramic_capacitor_is_not_deliverable():
    assert MODEL_CLASS_NAMES[0] == "ceramic-capacitor"
    assert model_class_to_box_id(0) is None
    assert is_deliverable_class(0) is False
    assert label_for_model_class(0) == "ignored: ceramic-capacitor"


def test_five_deliverable_classes_map_to_box_ids():
    assert model_class_to_box_id(1) == 1
    assert model_class_to_box_id(2) == 2
    assert model_class_to_box_id(3) == 3
    assert model_class_to_box_id(4) == 4
    assert model_class_to_box_id(5) == 5
    assert BOX_LABELS[1] == "diode"
    assert BOX_LABELS[2] == "electrolytic-capacitor"
    assert BOX_LABELS[3] == "polyester-capacitor"
    assert BOX_LABELS[4] == "resistor"
    assert BOX_LABELS[5] == "transistor"


def test_unknown_model_class_is_not_deliverable():
    assert model_class_to_box_id(-1) is None
    assert model_class_to_box_id(6) is None
    assert is_deliverable_class(6) is False
    assert label_for_model_class(6) == "ignored: unknown-6"
