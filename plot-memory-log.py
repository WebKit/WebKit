#!/usr/bin/env python3

import argparse
import os
import re
import sys


LINE_RE = re.compile(r"^(.*?)(-?\d+(?:\.\d+)?)$")
SERIES_SUFFIX_RE = re.compile(r" \(\d+\)$")
SHORTHAND_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

def parse_log(path):
    series_order = []
    rows = []
    current = {}
    current_timestamp = None
    warnings = []

    def finalize_block():
        nonlocal current, current_timestamp
        if current_timestamp is None and not current:
            current = {}
            current_timestamp = None
            return
        if current_timestamp is None:
            warnings.append("Skipping block without Timestamp")
        else:
            row = {"Timestamp": current_timestamp}
            row.update(current)
            rows.append(row)
        current = {}
        current_timestamp = None

    with open(path, "r", encoding="utf-8") as handle:
        for line_no, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                if current_timestamp is not None or current:
                    finalize_block()
                continue
            match = LINE_RE.match(line)
            if not match:
                warnings.append(f"Unrecognized line: {line!r}")
                continue
            label = match.group(1).strip()
            value_str = match.group(2)
            value = float(value_str) if "." in value_str else int(value_str)
            if label == "Timestamp":
                if current_timestamp is not None or current:
                    finalize_block()
                current_timestamp = int(value)
                continue
            if label in current:
                continue
            current[label] = value
            if label not in series_order:
                series_order.append(label)

    finalize_block()
    return rows, series_order, warnings


def build_series(rows, series_order, relative_time):
    timestamps = [row.get("Timestamp") for row in rows if row.get("Timestamp") is not None]
    if relative_time and timestamps:
        base = timestamps[0]
        timestamps = [ts - base for ts in timestamps]
    series = []
    for label in series_order:
        values = [row.get(label) for row in rows]
        series.append((label, values))
    return timestamps, series


def parse_label_list(values):
    if not values:
        return []
    labels = []
    for value in values:
        for part in value.split(","):
            item = part.strip()
            if item:
                labels.append(item)
    return labels


def base_label(label):
    return SERIES_SUFFIX_RE.sub("", label)


def build_shorthand_map(labels):
    mapping = {}
    reverse = {}
    for label in labels:
        if label in reverse:
            continue
        if len(mapping) >= len(SHORTHAND_CHARS):
            break
        key = SHORTHAND_CHARS[len(mapping)]
        mapping[key] = label
        reverse[label] = key
    return mapping, reverse


def main():
    parser = argparse.ArgumentParser(
        description="Plot memory log metrics over time.",
    )
    parser.add_argument("input", nargs="+", help="Path(s) to memory log text file(s).")
    parser.add_argument(
        "-o",
        "--output",
        help="Output image path (png, svg, pdf). Defaults to <input>.png.",
    )
    parser.add_argument(
        "--relative-time",
        action="store_true",
        help="Plot time as seconds since the first timestamp.",
    )
    parser.add_argument(
        "--include",
        action="append",
        help="Include only these labels or shorthands (repeat or comma-separated).",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        help="Exclude these labels or shorthands (repeat or comma-separated).",
    )
    parser.add_argument(
        "--no-legend",
        action="store_true",
        help="Do not render the legend.",
    )
    parser.add_argument(
        "--list-labels",
        action="store_true",
        help="Print label shorthands and exit.",
    )
    parser.add_argument(
        "--max-seconds",
        type=float,
        help="Limit chart to this many seconds from the start.",
    )
    args = parser.parse_args()

    output = args.output
    if not output:
        base, _ = os.path.splitext(args.input[0])
        output = f"{base}.png"

    # Parse all input files and combine series
    all_series_order = []
    all_base_labels = []
    combined_timestamps = None
    combined_series = []
    file_count = len(args.input)
    
    for file_idx, input_file in enumerate(args.input):
        try:
            rows, series_order, warnings = parse_log(input_file)
        except ValueError as exc:
            print(f"Error reading {input_file}: {exc}", file=sys.stderr)
            return 1
        if not rows:
            print(f"No data rows found in {input_file}.", file=sys.stderr)
            return 1
        
        # For multiple files, always use relative time so they overlap.
        # For single file, respect the --relative-time flag.
        use_relative = (len(args.input) > 1) or args.relative_time
        timestamps, series = build_series(rows, series_order, use_relative)
        if not timestamps:
            print(f"No timestamps found in {input_file}.", file=sys.stderr)
            return 1
        
        # Use timestamps from first file; note that multiple files should have same timestamps
        if combined_timestamps is None:
            combined_timestamps = timestamps
        
        # Add file suffix to labels if multiple files. Store both original
        # label and a display label so filtering can match the original
        # label while the legend shows the file-specific display label.
        file_suffix = f" ({input_file})" if file_count > 1 else ""
        for label, values in series:
            display_label = label + file_suffix
            combined_series.append((label, display_label, values, timestamps))
            all_series_order.append(label)
    
    # Build base labels from all combined series
    base_labels = []
    for label in all_series_order:
        label_base = base_label(label)
        if label_base not in base_labels:
            base_labels.append(label_base)
    shorthand_map, reverse_map = build_shorthand_map(base_labels)
    if len(shorthand_map) < len(base_labels):
        warnings.append("Not enough shorthand characters for all labels.")

    if args.list_labels:
        for label in base_labels:
            key = reverse_map.get(label, "?")
            print(f"{key} = {label}")
        return 0

    include_tokens = parse_label_list(args.include)
    exclude_tokens = parse_label_list(args.exclude)
    include_labels = set()
    exclude_labels = set()
    for token in include_tokens:
        if len(token) == 1 and token in shorthand_map:
            include_labels.add(shorthand_map[token])
        else:
            include_labels.add(token)
    for token in exclude_tokens:
        if len(token) == 1 and token in shorthand_map:
            exclude_labels.add(shorthand_map[token])
        else:
            exclude_labels.add(token)
    if include_labels or exclude_labels:
        filtered_series = []
        for orig_label, display_label, values, ts in combined_series:
            label_base = base_label(orig_label)
            if include_labels and label_base not in include_labels:
                continue
            if exclude_labels and label_base in exclude_labels:
                continue
            filtered_series.append((orig_label, display_label, values, ts))
        combined_series = filtered_series
        if not combined_series:
            print("No series matched the include/exclude filters.", file=sys.stderr)
            return 1

    # Truncate series to max_seconds if specified
    if args.max_seconds is not None:
        truncated_series = []
        for orig_label, display_label, values, ts in combined_series:
            # Find the index where timestamps exceed max_seconds
            cutoff_idx = len(ts)
            for i, timestamp in enumerate(ts):
                if timestamp > args.max_seconds:
                    cutoff_idx = i
                    break
            truncated_series.append((orig_label, display_label, values[:cutoff_idx], ts[:cutoff_idx]))
        combined_series = truncated_series
        if not combined_series or all(len(ts) == 0 for _, _, _, ts in combined_series):
            print(f"No data points within {args.max_seconds} seconds.", file=sys.stderr)
            return 1

    # Convert combined_series quadruples to (display_label, values, timestamps) for plotting
    series = [(display_label, values, ts) for orig_label, display_label, values, ts in combined_series]

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import numpy as np
    except Exception as exc:
        print("matplotlib is required to generate a chart.", file=sys.stderr)
        print(f"Import error: {exc}", file=sys.stderr)
        return 1

    fig, ax = plt.subplots(figsize=(14, 6))
    
    # Use a colormap to generate distinct colors for each series
    num_series = len(series)
    if num_series <= 20:
        cmap = plt.get_cmap("tab20")
        colors = [cmap(i) for i in range(num_series)]
    else:
        cmap = plt.get_cmap("hsv")
        colors = [cmap(i / num_series) for i in range(num_series)]
    
    for (label, values, ts), color in zip(series, colors):
        scaled = [value / (1024 * 1024) if value is not None else None for value in values]
        ax.plot(ts, scaled, label=label, color=color)

    ax.set_xlabel("Seconds" if (len(args.input) > 1 or args.relative_time) else "Timestamp")
    ax.set_ylabel("MB")
    ax.grid(True, linestyle="--", alpha=0.4)

    if not args.no_legend:
        ax.legend(loc="upper left", bbox_to_anchor=(1.02, 1.0), fontsize="small")
        fig.subplots_adjust(right=0.75)

    fig.tight_layout()
    fig.savefig(output, dpi=150)

    if warnings:
        print("Warnings:", file=sys.stderr)
        for warning in warnings:
            print(f"- {warning}", file=sys.stderr)

    print(f"Wrote chart to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
