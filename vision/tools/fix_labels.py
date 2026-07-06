"""Remap labels from the legacy five-class capture order to final model IDs.

The final model keeps class 0 as ceramic-capacitor for compatibility with the
trained weights, but the sorter deliberately ignores class 0. This migration
script is only for old label files whose source ID 0 meant resistor.
"""

from __future__ import annotations

import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_LABEL_DIR = SCRIPT_DIR.parent / "captured_data"

LEGACY_TO_FINAL_CLASS_ID = {
    "0": "4",  # legacy resistor -> final resistor
    "1": "1",  # diode
    "2": "5",  # transistor
    "3": "3",  # polyester-capacitor
    "4": "2",  # electrolytic-capacitor
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label-dir", type=Path, default=DEFAULT_LABEL_DIR)
    parser.add_argument("--dry-run", action="store_true", help="Print files that would change.")
    return parser.parse_args()


def remap_file(path: Path, dry_run: bool) -> bool:
    original_lines = path.read_text(encoding="utf-8").splitlines()
    updated_lines: list[str] = []

    for line in original_lines:
        parts = line.split()
        if parts and parts[0] in LEGACY_TO_FINAL_CLASS_ID:
            parts[0] = LEGACY_TO_FINAL_CLASS_ID[parts[0]]
            updated_lines.append(" ".join(parts))
        else:
            updated_lines.append(line)

    if updated_lines == original_lines:
        return False

    if not dry_run:
        path.write_text("\n".join(updated_lines) + "\n", encoding="utf-8")
    return True


def main() -> int:
    args = parse_args()
    if not args.label_dir.exists():
        raise FileNotFoundError(f"Label directory not found: {args.label_dir}")

    changed = 0
    for path in sorted(args.label_dir.glob("*.txt")):
        if path.name == "classes.txt":
            continue
        if remap_file(path, args.dry_run):
            changed += 1
            action = "would update" if args.dry_run else "updated"
            print(f"{action}: {path}")

    print(f"Processed {changed} label files. Final class 0 is not produced by this remap.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
