import argparse
import logging
import os
import sys
from typing import Optional, Sequence

from .analyze import analyze
from .dump import dump
from .histogram import delta_histogram
from .summary import summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="webkit-sysprof")
    subparsers = parser.add_subparsers(dest="command", required=True)

    dump_parser = subparsers.add_parser("dump", help="Dump capture data as CSV")
    dump_group = dump_parser.add_mutually_exclusive_group()
    dump_group.add_argument("--marks", action="store_true", help="Dump marks (default)")
    dump_group.add_argument("--counters", action="store_true", help="Dump counters")
    dump_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    dump_parser.add_argument(
        "-f",
        "--format",
        choices=["csv", "json"],
        default="csv",
        help="Format to be printed to STDOUT",
    )
    dump_parser.set_defaults(func=dump)

    summary_parser = subparsers.add_parser(
        "summary", help="Print a summary of a capture"
    )
    summary_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    summary_parser.set_defaults(func=summary)

    analyze_parser = subparsers.add_parser("analyze", help="Analyze capture")
    analyze_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    analyze_parser.add_argument(
        "-f",
        "--format",
        choices=["text", "json"],
        default="text",
        help="Type of report to be printed to STDOUT",
    )
    analyze_parser.add_argument(
        "-t",
        "--timespan",
        type=str,
        default="-",
        help="Timespan in milliseconds e.g. 0-5000",
    )
    analyze_parser.set_defaults(func=analyze)

    histogram_parser = subparsers.add_parser(
        "delta-histogram", help="Generate histogram from capture"
    )
    histogram_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    histogram_parser.add_argument(
        "mark_type",
        metavar="MARK_TYPE",
        help="Mark name to compute the delta-time histogram for",
    )
    histogram_parser.add_argument(
        "-t",
        "--timespan",
        type=str,
        default="-",
        help="Timespan in milliseconds e.g. 0-5000",
    )
    histogram_parser.set_defaults(func=delta_histogram)

    return parser


def main(argv: Optional[Sequence[str]] = None) -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)8s  %(message)s",
    )
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except BrokenPipeError:
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, sys.stdout.fileno())
        sys.exit(1)


if __name__ == "__main__":
    main()
