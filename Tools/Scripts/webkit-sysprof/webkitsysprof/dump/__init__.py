import argparse
import json
from typing import Any, Dict, List

from ..parser import parse


def dump(args: argparse.Namespace) -> None:
    data = parse(args.capture_file, args.marks or not args.counters, args.counters)

    if args.counters:
        headers = ["category", "name", "description", "time", "offset", "value"]
        rows = _counters_to_rows(data["counters"])
    else:
        headers = ["group", "name", "message", "time", "duration", "end_time"]
        rows = _marks_to_rows(data["marks"])

    if args.format == "json":
        _dump_json(rows)
    else:
        _dump_csv(headers, rows)


def _marks_to_rows(marks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    return [
        {
            "group": mark["group"],
            "name": mark["name"],
            "message": mark["message"],
            "time": mark["end_time"] - mark["duration"],
            "duration": mark["duration"],
            "end_time": mark["end_time"],
        }
        for mark in marks
    ]


def _counters_to_rows(counters: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    rows = []
    for counter in counters:
        for value in counter["values"]:
            rows.append(
                {
                    "category": counter["category"],
                    "name": counter["name"],
                    "description": counter["description"],
                    "time": value["time"],
                    "offset": value["offset"],
                    "value": value["value"],
                }
            )
    return rows


def _dump_csv(headers: List[str], rows: List[Dict[str, Any]]) -> None:
    print(";".join(headers))
    for row in rows:
        print(";".join(str(row[header]) for header in headers))


def _dump_json(rows: List[Dict[str, Any]]) -> None:
    print(json.dumps(rows))
