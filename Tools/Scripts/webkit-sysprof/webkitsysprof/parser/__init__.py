import logging
from typing import Any, Dict

from . import direct_parser


def parse(file_path: str, marks: bool, counters: bool) -> Dict[str, Any]:
    parsed_data = direct_parser.parse(file_path, marks, counters)

    logging.info("Post-processing parsed data...")
    _make_parsed_data_timestamps_relative_to_0(parsed_data)
    return parsed_data


def _make_parsed_data_timestamps_relative_to_0(parsed_data: Dict[str, Any]) -> None:
    baseline = parsed_data["document"]["timespan"][0]
    parsed_data["document"]["timespan"][0] = 0
    parsed_data["document"]["timespan"][1] -= baseline
    for mark in parsed_data["marks"]:
        mark["end_time"] -= baseline
    for counter in parsed_data["counters"]:
        for counter_value in counter["values"]:
            counter_value["time"] -= baseline
            counter_value["offset"] -= baseline
