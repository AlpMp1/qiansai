# Model Classes

The final deliverable uses five physical sorting boxes. `ceramic-capacitor` remains in the historical YOLO class metadata as model class `0`, but it is not a final deliverable class.

| Model class | Component label | Final policy | Physical box |
| --- | --- | --- | --- |
| 0 | ceramic-capacitor | ignored | no box |
| 1 | diode | valid | box 1 |
| 2 | electrolytic-capacitor | valid | box 2 |
| 3 | polyester-capacitor | valid | box 3 |
| 4 | resistor | valid | box 4 |
| 5 | transistor | valid | box 5 |

The current runtime remains compatible with old six-class YOLO metadata by accepting detections that report classes `0..5`. It maps only classes `1..5` to physical box IDs. Class `0` is filtered as ignored before it can be displayed as a valid class, counted, transmitted over serial, or sorted.

Class `0` never becomes a serial packet and never triggers sorting.
