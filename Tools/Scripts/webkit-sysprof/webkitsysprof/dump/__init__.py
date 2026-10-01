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

# Sentinel addresses libsysprof (and the perf infrastructure it borrows this from)
# splices into a raw stack to mark a transition between execution contexts (e.g. user
# code interrupted into the kernel), rather than an actual instruction pointer.
# Everything after one of these, up to the next one, is in the context it names; a
# stack with none of these markers at all is entirely in user space.
_CONTEXT_MARKERS = {
    (1 << 64) - 32: "hypervisor",
    (1 << 64) - 128: "kernel",
    (1 << 64) - 512: "user",
    (1 << 64) - 2048: "guest",
    (1 << 64) - 2176: "guest_kernel",
    (1 << 64) - 2560: "guest_user",
}

# Sysprof itself turns a marker into a visible pseudo-frame naming the context it is
# leaving (see sysprof-document-symbols.c's context_switches table), e.g. the
# boundary between a syscall's kernel frames and the userspace frames that called it
# shows up as "- - Kernel - -" between them. A stack that never touched anything but
# user code carries no markers at all, so there is nothing to resolve here; "user" is
# only reached for a marker crossing back out of some other context.
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
    """One row per distinct stack, folded to the format stackcollapse-perf.pl and
    flamegraph.pl use: a semicolon-separated stack from the process down to the
    innermost frame, paired with how many samples had exactly that stack.

    Sorted by stack, like stackcollapse-perf.pl's own output, so the dump is
    deterministic and diffable rather than in whatever order the capture's samples
    happened to interleave across processes.
    """
    maps_by_pid = {
        pid: _drop_overlapping_maps(maps) for pid, maps in data["maps"].items()
    }
    processes = data["processes"]
    # sysprof itself resolves every sample against the binaries it saw mapped while
    # recording and bundles the result into the capture, so a capture stays
    # symbolizable without them; this is what makes it possible at all for a stack
    # from another machine or architecture, or from a stripped binary, to come back
    # with real function names.
    bundled_symbols = _BundledSymbols(data["symbols"])
    # The kernel is mapped the same way into every process, so unlike bundled
    # userspace symbols this is one flat table, not one per pid.
    kernel_symbols = _KernelSymbols(data["kernel_symbols"])
    # Shared across every sample, so a library referenced by many stacks has its ELF
    # symbol table read and parsed once rather than once per address resolved
    # against it. Only reached for an address the bundle above did not cover, e.g. a
    # capture recorded by a sysprof too old to bundle symbols. mount_translator lets
    # it find a WebKit binary recorded at a path that only exists inside whatever
    # container profiled it (see ../mounts), the same way sysprof's own symbolizer
    # does, rather than assuming any one container convention.
    mount_translator = MountTranslator(data["host_mount_devices"], data["mounts_by_pid"])
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


# Never equal to any real identity a resolver returns (those are always tuples),
# so the very first frame of a stack is never mistaken for a duplicate of nothing.
_NO_FRAME_YET = object()


def _collapse_stack(
    sample: Dict[str, Any],
    pid_maps: List[Dict[str, Any]],
    processes: Dict[int, str],
    bundled_symbols: "_BundledSymbols",
    kernel_symbols: "_KernelSymbols",
    symbolizer: ElfSymbolizer,
) -> str:
    # Samples carry the stack innermost-first (the currently executing frame first,
    # then its callers), so this walks it in that order to track which context a
    # given address is in: everything up to the first marker is of whatever context
    # the sample started in, and a marker changes it for everything after, until the
    # next one. A folded stack reads outermost-first, so the result is reversed once
    # built.
    #
    # A marker other than the very first address is turned into a visible
    # "- - <Context> - -" pseudo-frame naming the context it leaves, the same as
    # Sysprof's own callgraph does; the very first one, if the stack starts with
    # one, is dropped instead, since nothing came before it to leave.
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
            # An address covered by neither the bundle nor any mapping (so there is
            # nothing at all to say about it, not even which file it might belong
            # to) is dropped rather than shown as a bare address: Sysprof's own
            # callgraph does the same, since a stack frame that names nothing is
            # not information, and a raw address is not a function a reader could
            # recognize or search for.
            if resolved is None:
                continue
            identity, text = resolved
        last_identity = _append_frame(frames, last_identity, identity, text)

    if not frames and context == "user":
        # sysprof's own fallback for a traceable whose only content, after all of
        # the above, is a single context-switch marker and nothing real at all
        # (sysprof_callgraph_add_traceable's "corrupted unwind" case): shown as a
        # distinct "Unwindable" frame, so the reader can tell a broken/empty unwind
        # from a process that is simply idle, rather than as a bare process name.
        frames = ["Unwindable"]
    elif context == "kernel":
        # A stack that entered the kernel but whose unwind never made it back out
        # (no trailing marker at all, so `context` is still "kernel" once the whole
        # raw stack has been walked) gets one more "- - Kernel - -" boundary, as the
        # outermost frame. Without it, a sample that happened to be a kernel-only
        # unwind — extremely common, since walking a kernel stack back into
        # whatever userspace code was interrupted is not always possible — would
        # read as if the process had called straight into kernel code itself, with
        # nothing to say a context switch happened at all.
        kernel_label = _CONTEXT_SWITCH_LABELS["kernel"]
        _append_frame(frames, last_identity, ("switch", "kernel"), kernel_label)

    frames.reverse()
    return ";".join([_process_name(pid, processes)] + frames)


def _append_frame(
    frames: List[str], last_identity: Any, identity: Any, text: str
) -> Any:
    """Appends `text` unless it names the very same thing the last frame appended
    did, and returns the identity to compare the next frame against.

    Mirrors sysprof_document_symbolize_traceable(), which only counts a resolved
    symbol if it differs from the one immediately before it: a run of raw addresses
    that all land in the same function (recursion, or several samples of one tight
    loop) reads as that one frame, not the same name repeated over and over.
    `identity` names *which table entry* resolved a frame (an address range, a
    matched ELF symbol, or the always-distinct fallback address itself), not the
    display text, so two different functions that merely demangle to the same name
    (e.g. a class's two constructor overloads) are never mistaken for each other.
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
    # sysprof's own fallback for a kernel address it cannot symbolize either, e.g.
    # one in a loaded module the capture's kallsyms snapshot has nothing for. Named
    # by its own exact address, like sysprof's own per-address fallback symbol, so
    # it is never treated as identical to a neighboring one.
    return ("in_kernel", address), f"In Kernel+0x{address:x}"


def _resolve_address(
    address: int,
    pid: int,
    pid_maps: List[Dict[str, Any]],
    bundled_symbols: "_BundledSymbols",
    symbolizer: ElfSymbolizer,
) -> Optional[Tuple[Any, str]]:
    """The (identity, text) of the function a stack address falls within, or
    failing that, the file mapped over it and its offset within that file, or None
    where nothing at all is known about it.

    The capture's own bundled symbols (see _BundledSymbols) are tried first, since
    they need nothing beyond the capture itself. Failing that, symbolizing means
    reading the ELF symbol table of the file the address was mapped from off disk,
    which only works where that exact file is still there to read, e.g. a capture
    being dumped on the machine (or an identical rootfs) it was recorded on, and a
    sysprof old enough not to have bundled symbols in the first place. Where none of
    that names anything, this falls back to the file and offset, the same fallback
    sysprof itself shows for a frame it cannot symbolize. An address outside every
    mapped range (most often a kernel address, since kernel maps are not captured)
    resolves to nothing at all.
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
            # Named by its own exact address, like the kernel fallback above, so a
            # run of these never collapses into one just for sharing a file.
            identity = (mapping["filename"], address)
            return identity, f"In File {mapping['filename']}+0x{relative:x}"
    return None


def _drop_overlapping_maps(maps: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Sysprof's own address layout drops the earlier-starting of two mappings that
    overlap (see SysprofAddressLayout's find_duplicates(), which this mirrors),
    e.g. a growing [stack] mapping getting a second, larger MAP frame at an
    overlapping address, or, as seen in practice, a loader's initial reservation
    overlapping the narrower vdso mapping carved out of it afterwards. Keeping both
    would make resolving an address depend on which of the two happened to come
    first in the list, rather than on the mapping actually in effect; an address
    that only the dropped, earlier mapping covered resolves to nothing at all,
    exactly as it does for Sysprof itself.
    """
    ordered = sorted(maps, key=lambda mapping: (mapping["start"], mapping["end"]))
    kept = []
    for index, mapping in enumerate(ordered):
        if index + 1 < len(ordered) and mapping["end"] > ordered[index + 1]["start"]:
            continue
        kept.append(mapping)
    return kept


class _BundledSymbols:
    """Looks up the (addr_begin, addr_end, name) ranges webkitsysprof.parser reads
    out of a capture's own embedded "__symbols__" file, sysprof's own record-time
    symbolization of every process it saw.

    Built once per dump and reused across every sample, since the ranges for a pid
    are shared by however many of its samples land in the same function; the
    (address -> range) index for a pid is itself built lazily, on that pid's first
    lookup, since most dumps only ever touch a handful of the capture's processes.
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
            # Already sorted by (pid, addr_begin) by the parser, so this is just the
            # addr_begin column pulled out for bisect to search.
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
    """Looks up webkitsysprof.parser's decoding of a capture's own bundled copy of
    /proc/kallsyms, resolving an address the walk in _collapse_stack has determined
    to be in kernel context.

    kallsyms lines carry no end for the symbol they name, so, unlike _BundledSymbols,
    a symbol's range runs up to whichever address the next one starts at.
    """

    def __init__(self, entries: List[Tuple[int, str]]) -> None:
        self._entries = entries
        self._addresses = [entry[0] for entry in entries]
        self._low = entries[0][0] if entries else None
        # kallsyms gives no end for its last symbol either; like sysprof itself,
        # this treats it as covering a further 64 KiB rather than everything after.
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
    # Only a real path is meant to have its directory stripped here; a kernel
    # thread's own "comm" can itself contain a "/" (e.g. "kworker/1:2-events",
    # "migration/0"), and running it through basename() regardless would both
    # mangle the name and silently merge unrelated threads that happen to share a
    # trailing number ("migration/0", "ksoftirqd/0" and "cpuhp/0" would all become
    # the single process name "0").
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
