import argparse
import logging
import os
import sys
from typing import Any, Callable, Optional, Sequence

from .analyze import analyze
from .dump import dump
from .histogram import delta_histogram
from .summary import summary
from .timeline import (
    DEFAULT_MAX_ROWS,
    DEFAULT_RESOLUTION,
    DEFAULT_ROW_REFRESHES,
    FRAME_INTERVAL,
    UPDATE_LATENCY,
    timeline,
)
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
    dump_group.add_argument(
        "--collapsed-stacktraces",
        action="store_true",
        help="Dump collapsed stack traces, in the folded-stack format suitable for "
        "flamegraph.pl/stackcollapse-perf.pl",
    )
    dump_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    dump_parser.add_argument(
        "-f",
        "--format",
        choices=["csv", "json"],
        default="csv",
        help="Format to be printed to STDOUT. For --collapsed-stacktraces, csv means"
        " the plain folded-stack text format, and json a list of {stack, count}",
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
        help="Explain how to read the report.",
    )

    timeline_parser = _add_subcommand(
        subparsers,
        "timeline",
        "Draw what the main thread and the compositor did over time, with the"
        " frames numbered",
        timeline,
    )
    timeline_parser.add_argument(
        "capture_file", metavar="CAPTURE_FILE", help="Path to a .capture file"
    )
    timeline_window = timeline_parser.add_mutually_exclusive_group()
    timeline_window.add_argument(
        "-t",
        "--timespan",
        type=str,
        default="-",
        help=TIMESPAN_HELP,
    )
    timeline_window.add_argument(
        "--frames",
        type=str,
        default=None,
        help='Window of the frames "<first>-<last>", numbered from 1 over the whole'
        " capture, or a single frame to draw one row around",
    )
    timeline_parser.add_argument(
        "--worst",
        choices=[FRAME_INTERVAL, UPDATE_LATENCY],
        default=None,
        help="Draw a row around each of the worst events instead, worst first: the"
        " longest active frame intervals, or the rendering updates slowest to reach"
        " the screen",
    )
    timeline_parser.add_argument(
        "--row-length",
        type=float,
        default=None,
        help="Milliseconds per row (default: {} refresh intervals)".format(
            DEFAULT_ROW_REFRESHES
        ),
    )
    timeline_parser.add_argument(
        "-n",
        "--max-rows",
        type=int,
        default=DEFAULT_MAX_ROWS,
        help="Maximum number of rows to draw",
    )
    timeline_parser.add_argument(
        "-r",
        "--resolution",
        type=float,
        default=None,
        help="Milliseconds per cell (default: {}, coarser where a --worst row would"
        " not fit in --max-cells)".format(DEFAULT_RESOLUTION),
    )
    timeline_parser.add_argument(
        "--max-cells",
        type=int,
        default=400,
        help="Truncate bars longer than this many cells",
    )
    timeline_parser.add_argument(
        "--no-tile-lane",
        dest="tile_lane",
        action="store_false",
        help="Omit the tiles lane showing threaded tile painting",
    )
    timeline_parser.add_argument(
        "--color",
        choices=["auto", "always", "never"],
        default="auto",
        help="Colorize the bars",
    )
    timeline_parser.add_argument(
        "-e",
        "--explain",
        action="store_true",
        help="Explain the terms the timeline uses, after it",
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
