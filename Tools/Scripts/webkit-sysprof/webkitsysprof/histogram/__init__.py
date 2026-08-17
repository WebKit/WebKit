import argparse
import math
import statistics
from typing import Any, Dict, List

from ..parser import parse
from ..utils import (
    MSEC_PER_SEC,
    nsec_to_msec,
    parse_timespan_argument,
    trim_sysprof_data_to_timespan,
)


def delta_histogram(args: argparse.Namespace) -> None:
    timespan_begin, timespan_end = parse_timespan_argument(args.timespan)
    parsed_data = parse(args.capture_file, marks=True, counters=False)
    trimmed_data = trim_sysprof_data_to_timespan(
        parsed_data, timespan_begin, timespan_end
    )
    deltas_ms = _calculate_delta_times_ms(trimmed_data["marks"], args.mark_type)

    if not deltas_ms:
        print(f"No data available for mark: {args.mark_type}")
        return

    _plot_delta_time_distribution(args.mark_type, deltas_ms)


def _calculate_delta_times_ms(
    marks: List[Dict[str, Any]], mark_name: str
) -> List[float]:
    end_times = sorted(mark["end_time"] for mark in marks if mark["name"] == mark_name)
    if len(end_times) < 2:
        return []
    return [
        nsec_to_msec(end_times[i] - end_times[i - 1]) for i in range(1, len(end_times))
    ]


def _calculate_optimal_bins(deltas_ms: List[float]) -> int:
    # Freedman-Diaconis rule, falling back to Sturges' rule for a tiny IQR.
    if len(deltas_ms) < 2:
        return 10

    q25, q75 = (
        statistics.quantiles(deltas_ms, n=4)[0],
        statistics.quantiles(deltas_ms, n=4)[2],
    )
    iqr = q75 - q25

    if iqr > 0:
        n = len(deltas_ms)
        bin_width = 2 * iqr * (n ** (-1 / 3))
        n_bins = int((max(deltas_ms) - min(deltas_ms)) / bin_width)
    else:
        n_bins = int(math.ceil(math.log2(len(deltas_ms)) + 1))

    return min(max(n_bins, 10), 100)


def _plot_delta_time_distribution(mark_name: str, deltas_ms: List[float]) -> None:
    import matplotlib.pyplot as plt

    mean_val = statistics.mean(deltas_ms)
    median_val = statistics.median(deltas_ms)
    min_val = min(deltas_ms)
    max_val = max(deltas_ms)
    std_val = statistics.stdev(deltas_ms) if len(deltas_ms) > 1 else 0.0
    n_bins = _calculate_optimal_bins(deltas_ms)

    stats_text = (
        f"Sample Size: {len(deltas_ms):,}\n"
        f"Std Dev: {std_val:.2f} ms\n"
        f"Frequency: {MSEC_PER_SEC / mean_val:.1f} Hz"
    )

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 12), gridspec_kw={"hspace": 0.15})
    for ax, log_scale in ((ax1, False), (ax2, True)):
        ax.hist(
            deltas_ms,
            bins=n_bins,
            alpha=0.8,
            color="#4A90E2",
            edgecolor="#2E5C8A",
            linewidth=0.8,
        )
        ax.axvline(
            mean_val,
            color="#E74C3C",
            linestyle="--",
            linewidth=2.5,
            label=f"Mean: {mean_val:.2f} ms",
            alpha=0.8,
        )
        ax.axvline(
            min_val,
            color="#34495E",
            linestyle=":",
            linewidth=2,
            label=f"Min: {min_val:.2f} ms",
            alpha=0.7,
        )
        ax.axvline(
            max_val,
            color="#34495E",
            linestyle=":",
            linewidth=2,
            label=f"Max: {max_val:.2f} ms",
            alpha=0.7,
        )
        ax.axvline(
            median_val,
            color="#27AE60",
            linestyle="--",
            linewidth=2,
            label=f"Median: {median_val:.2f} ms",
            alpha=0.8,
        )

        if log_scale:
            ax.set_yscale("log")
            ax.set_ylabel("Count (Log Scale)", fontsize=14, fontweight="semibold")
            ax.grid(True, alpha=0.15, linestyle=":", which="minor")
        else:
            ax.set_ylabel("Count", fontsize=14, fontweight="semibold")

        ax.set_xlabel(
            "Time Between Consecutive Marks (ms)", fontsize=14, fontweight="semibold"
        )
        ax.legend(loc="upper right", fontsize=11, framealpha=0.9, edgecolor="gray")
        ax.grid(True, alpha=0.3, linestyle="--")
        ax.tick_params(axis="both", which="major", labelsize=12)
        ax.text(
            0.02,
            0.35,
            stats_text,
            transform=ax.transAxes,
            fontsize=11,
            verticalalignment="top",
            horizontalalignment="left",
            bbox=dict(
                boxstyle="round,pad=0.5",
                facecolor="white",
                edgecolor="gray",
                alpha=0.95,
            ),
        )

    fig.suptitle(
        f"Delta Time Distribution for {mark_name}",
        fontsize=18,
        fontweight="bold",
        y=0.98,
    )
    plt.subplots_adjust(left=0.08, bottom=0.05, right=0.98, top=0.92, hspace=0.15)
    plt.show()
