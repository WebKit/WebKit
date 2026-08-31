import argparse
import logging
import os
import sys
from typing import Any, Callable, Optional, Sequence

from .analyze import analyze
from .cycle_analysis import (
    DEFAULT_MIN_LENGTH,
    DEFAULT_RESOLUTION,
    analyze_frame_cycle,
)
from .dump import dump
from .histogram import delta_histogram
from .summary import summary
from .utils import UsageError

TIMESPAN_HELP = (
    'Window of the capture to read, as "<begin>-<end>" in milliseconds from its'
    ' start, e.g. 0-5000. Either side may be left out: "500-" and "500" both mean'
    ' from 500ms to the end, "-500" the first 500ms. Anything else is an error.'
)


def _add_subcommand(
    subparsers: Any,
    name: str,
    help_text: str,
    func: Callable[[argparse.Namespace], None],
) -> argparse.ArgumentParser:
    """Add a subcommand that knows its own parser, for reporting usage errors."""
    command_parser = subparsers.add_parser(name, help=help_text)
    command_parser.set_defaults(func=func, command_parser=command_parser)
    return command_parser


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="webkit-sysprof")
    subparsers = parser.add_subparsers(dest="command", required=True)

    dump_parser = _add_subcommand(subparsers, "dump", "Dump capture data as CSV", dump)
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

    summary_parser = _add_subcommand(
        subparsers, "summary", "Print a summary of a capture", summary
    )
    summary_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )

    analyze_parser = _add_subcommand(subparsers, "analyze", "Analyze capture", analyze)
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
        help=TIMESPAN_HELP,
    )
    analyze_parser.add_argument(
        "-e",
        "--explain",
        action="store_true",
        help="Explain how to read the report, alongside the report itself"
        " (text format only)",
    )

    cycle_analysis_parser = _add_subcommand(
        subparsers,
        "cycle-analysis",
        "Visualize individual frame cycles",
        analyze_frame_cycle,
    )
    cycle_analysis_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    cycle_analysis_parser.add_argument(
        "-t",
        "--timespan",
        type=str,
        default="-",
        help=TIMESPAN_HELP,
    )
    cycle_analysis_parser.add_argument(
        "-n",
        "--max-cycles",
        type=int,
        default=40,
        help="Maximum number of cycles to draw",
    )
    cycle_analysis_parser.add_argument(
        "-r",
        "--resolution",
        type=float,
        default=DEFAULT_RESOLUTION,
        help="Milliseconds per cell",
    )
    cycle_analysis_parser.add_argument(
        "--order",
        choices=["first", "slowest"],
        default="first",
        help="Draw the first cycles or the slowest ones",
    )
    cycle_analysis_parser.add_argument(
        "--min-duration",
        type=float,
        default=0.0,
        help="Only draw cycles at least this long, in milliseconds",
    )
    cycle_analysis_parser.add_argument(
        "--min-length",
        type=float,
        default=DEFAULT_MIN_LENGTH,
        help="Shortest bar to draw, in milliseconds",
    )
    cycle_analysis_parser.add_argument(
        "--max-cells",
        type=int,
        default=400,
        help="Truncate bars longer than this many cells",
    )
    cycle_analysis_parser.add_argument(
        "--no-tile-lane",
        dest="tile_lane",
        action="store_false",
        help="Omit the second lane showing threaded tile painting",
    )
    cycle_analysis_parser.add_argument(
        "--color",
        choices=["auto", "always", "never"],
        default="auto",
        help="Colorize the bars",
    )

    histogram_parser = _add_subcommand(
        subparsers,
        "delta-histogram",
        "Generate histogram from capture",
        delta_histogram,
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
        help=TIMESPAN_HELP,
    )

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
    except UsageError as error:
        args.command_parser.error(str(error))
    except BrokenPipeError:
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, sys.stdout.fileno())
        sys.exit(1)


if __name__ == "__main__":
    main()
