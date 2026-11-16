import argparse
import bisect
import collections
import json
import os
from typing import Any, Dict, List, Optional, Tuple

from ..mounts import MountTranslator
from ..parser import parse
from ..symbolizer import ElfSymbolizer
from ..utils import mark_begin, mark_pid

# perf context markers in a raw stack. Addresses after one, up to the next, are in
# the context it names. A stack without markers is all user space.
_CONTEXT_MARKERS = {
    (1 << 64) - 32: "hypervisor",
    (1 << 64) - 128: "kernel",
    (1 << 64) - 512: "user",
    (1 << 64) - 2048: "guest",
    (1 << 64) - 2176: "guest_kernel",
    (1 << 64) - 2560: "guest_user",
}

# Pseudo-frames naming the context a marker leaves, as in
# sysprof-document-symbols.c's context_switches table.
_CONTEXT_SWITCH_LABELS = {
    None: "- - User - -",
    "hypervisor": "- - Hypervisor - -",
    "kernel": "- - Kernel - -",
    "user": "- - User - -",
    "guest": "- - Guest - -",
    "guest_kernel": "- - Guest Kernel - -",
    "guest_user": "- - Guest User - -",
}


def dump(args: argparse.Namespace) -> None:
    collapsed_stacktraces = getattr(args, "collapsed_stacktraces", False)
    data = parse(
        args.capture_file,
        marks=args.marks or not (args.counters or collapsed_stacktraces),
        counters=args.counters,
        stacktraces=collapsed_stacktraces,
    )

    if collapsed_stacktraces:
        rows = _collapsed_stacktraces_to_rows(data)
        if args.format == "json":
            _dump_json(rows)
        else:
            _dump_collapsed_stacktraces(rows)
        return

    if args.counters:
        headers = ["category", "name", "description", "time", "offset", "value"]
        rows = _counters_to_rows(data["counters"])
    else:
        headers = ["group", "pid", "name", "message", "time", "duration", "end_time"]
        rows = _marks_to_rows(data["marks"])

    if args.format == "json":
        _dump_json(rows)
    else:
        _dump_csv(headers, rows)


def _marks_to_rows(marks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    return [
        {
            "group": mark["group"],
            "pid": mark_pid(mark),
            "name": mark["name"],
            "message": mark["message"],
            "time": mark_begin(mark),
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


def _collapsed_stacktraces_to_rows(data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """One row per distinct folded stack (stackcollapse-perf.pl format, process
    first) with its sample count, sorted by stack so output is deterministic.
    """
    maps_by_pid = {
        pid: _drop_overlapping_maps(maps) for pid, maps in data["maps"].items()
    }
    processes = data["processes"]
    bundled_symbols = _BundledSymbols(data["symbols"])
    # One table for all pids, since the kernel is mapped the same everywhere.
    kernel_symbols = _KernelSymbols(data["kernel_symbols"])
    # ELF fallback for addresses the bundle misses. Shared so each file is parsed
    # once. The translator finds binaries recorded at container paths.
    mount_translator = MountTranslator(
        data["host_mount_devices"], data["mounts_by_pid"]
    )
    symbolizer = ElfSymbolizer(mount_translator)
    counts: "collections.Counter[str]" = collections.Counter()
    for sample in data["stacktraces"]:
        stack = _collapse_stack(
            sample,
            maps_by_pid.get(sample["pid"], []),
            processes,
            bundled_symbols,
            kernel_symbols,
            symbolizer,
        )
        counts[stack] += 1
    return [{"stack": stack, "count": count} for stack, count in sorted(counts.items())]


# Never equal to a real identity (always a tuple), so the first frame is kept.
_NO_FRAME_YET = object()


def _collapse_stack(
    sample: Dict[str, Any],
    pid_maps: List[Dict[str, Any]],
    processes: Dict[int, str],
    bundled_symbols: "_BundledSymbols",
    kernel_symbols: "_KernelSymbols",
    symbolizer: ElfSymbolizer,
) -> str:
    # Walk innermost-first to track context, then reverse for folded output. Each
    # marker except a leading one becomes a pseudo-frame for the context it leaves,
    # as in Sysprof's callgraph.
    pid = sample["pid"]
    addresses = sample["addresses"]
    frames: List[str] = []
    context = None
    last_identity: Any = _NO_FRAME_YET
    for index, address in enumerate(addresses):
        marker_context = _CONTEXT_MARKERS.get(address)
        if marker_context is not None:
            if index != 0:
                label = _CONTEXT_SWITCH_LABELS[context]
                last_identity = _append_frame(
                    frames, last_identity, ("switch", context), label
                )
            context = marker_context
            continue
        if context == "kernel":
            identity, text = _resolve_kernel_address(address, kernel_symbols)
        else:
            resolved = _resolve_address(
                address, pid, pid_maps, bundled_symbols, symbolizer
            )
            # Unknown addresses are dropped, as in Sysprof's callgraph.
            if resolved is None:
                continue
            identity, text = resolved
        last_identity = _append_frame(frames, last_identity, identity, text)

    if not frames and context == "user":
        # sysprof_callgraph_add_traceable()'s "corrupted unwind" case: only a
        # marker and no real frames.
        frames = ["Unwindable"]
    elif context == "kernel":
        # A kernel-only unwind (common) gets an outermost "- - Kernel - -" frame so
        # it does not read as the process calling kernel code directly.
        kernel_label = _CONTEXT_SWITCH_LABELS["kernel"]
        _append_frame(frames, last_identity, ("switch", "kernel"), kernel_label)

    frames.reverse()
    return ";".join([_process_name(pid, processes)] + frames)


def _append_frame(
    frames: List[str], last_identity: Any, identity: Any, text: str
) -> Any:
    """Appends `text` unless `identity` equals the previous frame's, and returns
    `identity`. Mirrors sysprof_document_symbolize_traceable(). Comparing identity
    (the resolving table entry) rather than text keeps distinct functions with the
    same demangled name apart.
    """
    if identity != last_identity:
        frames.append(text)
    return identity


def _resolve_kernel_address(
    address: int, kernel_symbols: "_KernelSymbols"
) -> Tuple[Any, str]:
    resolved = kernel_symbols.resolve(address)
    if resolved is not None:
        return resolved
    # sysprof's fallback. Keyed by exact address so neighbors never collapse.
    return ("in_kernel", address), f"In Kernel+0x{address:x}"


def _resolve_address(
    address: int,
    pid: int,
    pid_maps: List[Dict[str, Any]],
    bundled_symbols: "_BundledSymbols",
    symbolizer: ElfSymbolizer,
) -> Optional[Tuple[Any, str]]:
    """(identity, text) for a user-space address: bundled symbols, then the ELF
    file on disk, then sysprof's "In File" fallback. None if no mapping covers it.
    """
    resolved = bundled_symbols.resolve(pid, address)
    if resolved is not None:
        return resolved

    for mapping in pid_maps:
        if mapping["start"] <= address < mapping["end"]:
            relative = address - mapping["start"] + mapping["offset"]
            resolved = symbolizer.resolve(mapping["filename"], relative, pid=pid)
            if resolved is not None:
                return resolved
            # Keyed by exact address, like the kernel fallback.
            identity = (mapping["filename"], address)
            return identity, f"In File {mapping['filename']}+0x{relative:x}"
    return None


def _drop_overlapping_maps(maps: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Drops the earlier-starting of two overlapping mappings, mirroring
    SysprofAddressLayout's find_duplicates() (e.g. a regrown [stack], or a loader
    reservation overlapping the vdso).
    """
    ordered = sorted(maps, key=lambda mapping: (mapping["start"], mapping["end"]))
    kept = []
    for index, mapping in enumerate(ordered):
        if index + 1 < len(ordered) and mapping["end"] > ordered[index + 1]["start"]:
            continue
        kept.append(mapping)
    return kept


class _BundledSymbols:
    """Lookup into the capture's bundled "__symbols__" ranges. The per-pid bisect
    index is built lazily, since most dumps touch few processes.
    """

    def __init__(self, entries_by_pid: Dict[int, List[Tuple[int, int, str]]]) -> None:
        self._entries_by_pid = entries_by_pid
        self._begins_by_pid: Dict[int, List[int]] = {}

    def resolve(self, pid: int, address: int) -> Optional[Tuple[Any, str]]:
        entries = self._entries_by_pid.get(pid)
        if not entries:
            return None

        begins = self._begins_by_pid.get(pid)
        if begins is None:
            # The parser already sorted entries by addr_begin.
            begins = [entry[0] for entry in entries]
            self._begins_by_pid[pid] = begins

        index = bisect.bisect_right(begins, address) - 1
        if index < 0:
            return None

        addr_begin, addr_end, name = entries[index]
        if address >= addr_end:
            return None
        return ("bundled", pid, addr_begin, addr_end), name


class _KernelSymbols:
    """Lookup into the bundled /proc/kallsyms. kallsyms has no symbol ends, so
    each symbol runs up to the next.
    """

    def __init__(self, entries: List[Tuple[int, str]]) -> None:
        self._entries = entries
        self._addresses = [entry[0] for entry in entries]
        self._low = entries[0][0] if entries else None
        # Like sysprof, the last symbol covers a further 64 KiB.
        self._high = entries[-1][0] + 0xFFFF if entries else None

    def resolve(self, address: int) -> Optional[Tuple[Any, str]]:
        if not self._entries or address < self._low or address >= self._high:
            return None

        index = bisect.bisect_right(self._addresses, address) - 1
        if index < 0:
            return None

        addr_begin, name = self._entries[index]
        return ("kernel", addr_begin), name


def _process_name(pid: int, processes: Dict[int, str]) -> str:
    cmdline = processes.get(pid, "")
    if not cmdline:
        return f"pid {pid}"
    first_token = cmdline.split(" ", 1)[0]
    # Only strip absolute paths. Kernel thread names like "migration/0" contain "/"
    # and basename() would merge them all into "0".
    name = os.path.basename(first_token) if first_token.startswith("/") else first_token
    return name or f"pid {pid}"


def _dump_collapsed_stacktraces(rows: List[Dict[str, Any]]) -> None:
    for row in rows:
        print(f"{row['stack']} {row['count']}")


def _dump_csv(headers: List[str], rows: List[Dict[str, Any]]) -> None:
    print(";".join(headers))
    for row in rows:
        print(";".join(str(row[header]) for header in headers))


def _dump_json(rows: List[Dict[str, Any]]) -> None:
    print(json.dumps(rows))
