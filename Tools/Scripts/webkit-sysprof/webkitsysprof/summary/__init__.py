import argparse
from collections import Counter
from typing import Any, Dict, List

from ..parser import parse
from ..utils import nsec_to_sec


def summary(args: argparse.Namespace) -> None:
    data = parse(args.capture_file, marks=True, counters=True)

    _print_document_summary(args.capture_file, data["document"])
    _print_marks_summary(data["marks"])
    _print_counters_summary(data["counters"])


def _print_document_summary(capture_file: str, document: Dict[str, Any]) -> None:
    print(f"File: {capture_file}")
    print(f"Title: {document['title']}")
    print(f"Subtitle: {document['subtitle']}")
    print(
        "Timespan: {:.4f} - {:.4f} [s]".format(
            abs(nsec_to_sec(document["timespan"][0])),
            abs(nsec_to_sec(document["timespan"][1])),
        )
    )
    print()


def _print_marks_summary(marks: List[Dict]) -> None:
    print(f"Marks: {len(marks)}")

    occurrences = Counter(mark["name"] for mark in marks)
    for name, count in sorted(
        occurrences.items(), key=lambda item: (-item[1], item[0])
    ):
        print(f"  {name}: {count}")
    print()


def _print_counters_summary(counters: List[Dict]) -> None:
    print(f"Counters: {len(counters)}")

    for counter in sorted(
        counters, key=lambda counter: (counter["category"], counter["name"])
    ):
        print(
            f"  {counter['category']} / {counter['name']}: "
            f"{len(counter['values'])} values"
        )
