import gzip
import mmap
import os
import re
import struct
import logging
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Tuple, Union

from ..mounts import MountDevice, MountInfoEntry, parse_mountinfo, parse_proc_mounts

_MAGIC = 0xFDCA975E
_FILE_HEADER_SIZE = 256
_FRAME_HEADER_SIZE = 24
_FRAME_ALIGN = 8

_FRAME_SAMPLE = 2
_FRAME_MAP = 3
_FRAME_PROCESS = 4
_FRAME_CTRDEF = 8
_FRAME_CTRSET = 9
_FRAME_MARK = 10
_FRAME_FILE_CHUNK = 13

# Frame types considered "data" by libsysprof when guessing a capture's end
# time (everything except TIMESTAMP, CTRDEF, FILE_CHUNK, JITMAP, METADATA,
# and OVERLAY).
_DATA_FRAME_TYPES = {2, 3, 4, 5, 6, 9, 10, 12, 14, 16, 17}

_COUNTER_TYPE_DOUBLE = 1

# sysprof itself resolves every sample against the binaries it saw mapped while
# recording, and bundles the result into the capture as a file named this (usually
# gzip-compressed, hence the two names) so a capture stays symbolizable without the
# original binaries. See SysprofPackedSymbol in libsysprof for the record layout this
# decodes: a table of (addr_begin, addr_end, pid, name_offset, tag_offset, padding)
# sorted by (pid, addr_begin), followed by the strings the offsets point into.
_SYMBOLS_FILE_NAMES = ("__symbols__.gz", "__symbols__")
_PACKED_SYMBOL_SIZE = 32

# The recorder also embeds a copy of /proc/kallsyms, the running kernel's own symbol
# table, in the same file-chunked, usually-gzipped way, which is how a kernel frame
# (e.g. a syscall's stack reaching into a driver) gets a real name like drm_ioctl
# instead of a bare address: unlike the bundled userspace symbols above, sysprof does
# not resolve these into "__symbols__" itself, so this file is decoded independently.
_KALLSYMS_FILE_NAMES = ("/proc/kallsyms.gz", "/proc/kallsyms")

_BUNDLED_FILE_NAMES = _SYMBOLS_FILE_NAMES + _KALLSYMS_FILE_NAMES

# sysprof-cli also bundles its own (i.e. the recording machine's) /proc/mounts,
# and, per profiled process, that process's own /proc/<pid>/mountinfo -- together
# enough to translate a path recorded inside a container's bind mount (e.g.
# Tools/Scripts/container-sdk-rootdir-wrapper's /sdk/webkit) back into a real path
# on this machine, the same way libsysprof's own SysprofMountNamespace does (see
# ../mounts). Unlike the fixed names above, a mountinfo path carries the pid it
# belongs to, so it is matched by pattern rather than by an exact name.
_PROC_MOUNTS_FILE_NAMES = ("/proc/mounts.gz", "/proc/mounts")
_MOUNTINFO_PATH_RE = re.compile(r"^/proc/(\d+)/mountinfo(\.gz)?$")


def parse(
    file_path: str, marks: bool, counters: bool, stacktraces: bool = False
) -> Dict[str, Any]:
    assert marks or counters or stacktraces

    with open(file_path, "rb") as fileobj:
        with mmap.mmap(fileobj.fileno(), 0, access=mmap.ACCESS_READ) as data:
            return _parse_capture(data, file_path, marks, counters, stacktraces)


def _parse_capture(
    data: mmap.mmap,
    file_path: str,
    marks: bool,
    counters: bool,
    stacktraces: bool = False,
) -> Dict[str, Any]:
    logging.info("Parsing .syscap file...")
    magic, version_bits = struct.unpack_from("<II", data, 0)
    if magic != _MAGIC:
        raise ValueError(f"{file_path} is not a sysprof capture file")
    if not (version_bits >> 8) & 0x1:
        raise NotImplementedError("Big-endian captures are not supported")

    capture_time = _read_cstring(data, 8, 64) or ""
    header_time, header_end_time = struct.unpack_from("<qq", data, 72)

    frames = _index_frames(data)
    frames.sort(key=lambda entry: entry[3])

    guessed_end_nsec = 0
    parsed_marks: List[Dict[str, Any]] = []
    counter_defs: Dict[int, Dict[str, Any]] = {}
    counter_order: List[int] = []
    parsed_stacktraces: List[Dict[str, Any]] = []
    maps_by_pid: Dict[int, List[Dict[str, Any]]] = {}
    processes: Dict[int, str] = {}
    bundled_chunks: Dict[str, List[bytes]] = {}
    completed_bundled_chunks: set = set()

    for offset, length, frame_type, time in frames:
        if frame_type in _DATA_FRAME_TYPES and time > guessed_end_nsec:
            guessed_end_nsec = time

        if frame_type == _FRAME_MARK:
            duration = struct.unpack_from("<q", data, offset + 24)[0]
            end_time = time + duration
            if end_time > guessed_end_nsec:
                guessed_end_nsec = end_time
            if marks:
                parsed_marks.append(
                    _parse_mark(data, offset, length, duration, end_time)
                )
        elif frame_type == _FRAME_CTRDEF and counters:
            _parse_ctrdef(data, offset, counter_defs, counter_order)
        elif frame_type == _FRAME_CTRSET and counters:
            _parse_ctrset(data, offset, length, time, counter_defs)
        elif frame_type == _FRAME_SAMPLE and stacktraces:
            parsed_stacktraces.append(_parse_sample(data, offset, length, time))
        elif frame_type == _FRAME_MAP and stacktraces:
            _parse_map(data, offset, length, maps_by_pid)
        elif frame_type == _FRAME_PROCESS and stacktraces:
            _parse_process(data, offset, length, processes)
        elif frame_type == _FRAME_FILE_CHUNK and stacktraces:
            _accumulate_bundled_file_chunk(
                data, offset, length, bundled_chunks, completed_bundled_chunks
            )

    parsed_marks.sort(key=lambda mark: (mark["group"], mark["name"], mark["end_time"]))
    bundled_symbols_by_pid = _decode_bundled_symbols(bundled_chunks)
    kernel_symbols = _decode_kallsyms(bundled_chunks)
    host_mount_devices = _decode_host_mount_devices(bundled_chunks)
    mounts_by_pid = _decode_mounts_by_pid(bundled_chunks)

    parsed_counters = [
        {
            "category": counter_defs[counter_id]["category"],
            "name": counter_defs[counter_id]["name"],
            "description": counter_defs[counter_id]["description"],
            "values": counter_defs[counter_id]["values"],
        }
        for counter_id in counter_order
    ]

    end_nsec = guessed_end_nsec if guessed_end_nsec > header_time else header_end_time

    return {
        "document": {
            "title": os.path.basename(file_path),
            "subtitle": _format_subtitle(capture_time),
            "timespan": [header_time, end_nsec],
        },
        "marks": parsed_marks,
        "counters": parsed_counters,
        "stacktraces": parsed_stacktraces,
        "maps": maps_by_pid,
        "processes": processes,
        "symbols": bundled_symbols_by_pid,
        "kernel_symbols": kernel_symbols,
        "host_mount_devices": host_mount_devices,
        "mounts_by_pid": mounts_by_pid,
    }


def _index_frames(data: mmap.mmap) -> List[Tuple[int, int, int, int]]:
    frames = []
    pos = _FILE_HEADER_SIZE
    total_len = len(data)
    while pos < total_len - 2:
        (frame_len,) = struct.unpack_from("<H", data, pos)
        if frame_len < _FRAME_HEADER_SIZE or frame_len % _FRAME_ALIGN != 0:
            break
        frame_type = data[pos + 16]
        (frame_time,) = struct.unpack_from("<q", data, pos + 8)
        frames.append((pos, frame_len, frame_type, frame_time))
        pos += frame_len
    return frames


def _parse_mark(
    data: mmap.mmap, offset: int, length: int, duration: int, end_time: int
) -> Dict[str, Any]:
    # The frame header is len, cpu, pid, time, so the process that emitted the mark
    # is four bytes in. The group names its kind, e.g. "WebKit (Web)", which two
    # processes of one kind share.
    (pid,) = struct.unpack_from("<i", data, offset + 4)
    group = _read_cstring(data, offset + 32, 24) or ""
    name = _read_cstring(data, offset + 56, 40) or ""
    message = _read_cstring(data, offset + 96, length - 96) or ""
    return {
        "name": name,
        "message": message,
        "duration": duration,
        "end_time": end_time,
        "group": group,
        "pid": pid,
    }


def _parse_sample(
    data: mmap.mmap, offset: int, length: int, time: int
) -> Dict[str, Any]:
    # SysprofCaptureSample: frame, n_addrs:16 + padding:16, tid, addrs[n_addrs]. The
    # addresses are the raw stack the profiler unwound, innermost frame first, exactly
    # as libsysprof wrote them; nothing here resolves or reorders them.
    (pid,) = struct.unpack_from("<i", data, offset + 4)
    n_addrs_and_padding, tid = struct.unpack_from("<Ii", data, offset + 24)
    n_addrs = n_addrs_and_padding & 0xFFFF
    # n_addrs is a plain field of the frame, not implied by its (8-byte-aligned)
    # length, so a truncated or corrupted capture could declare more addresses than
    # the frame actually has room for; reading that many would run into whatever
    # comes after it in the mmap instead of raising. Clamping to what the frame's
    # own length allows keeps a bad frame from being misread as a bogus stack
    # rather than simply a short one.
    n_addrs = min(n_addrs, max(0, (length - 32) // 8))
    addresses = list(struct.unpack_from(f"<{n_addrs}Q", data, offset + 32))
    return {"pid": pid, "tid": tid, "time": time, "addresses": addresses}


def _parse_map(
    data: mmap.mmap,
    offset: int,
    length: int,
    maps_by_pid: Dict[int, List[Dict[str, Any]]],
) -> None:
    # SysprofCaptureMap: frame, start, end, offset, inode, filename. One of these is
    # written per file mapped into a process, and is what a raw stack address is
    # resolved against: which file covered it, and where within that file.
    (pid,) = struct.unpack_from("<i", data, offset + 4)
    start, end, file_offset, inode = struct.unpack_from("<QQQQ", data, offset + 24)
    filename = _read_cstring(data, offset + 56, length - 56) or ""
    maps_by_pid.setdefault(pid, []).append(
        {
            "start": start,
            "end": end,
            "offset": file_offset,
            "inode": inode,
            "filename": filename,
        }
    )


def _parse_process(
    data: mmap.mmap, offset: int, length: int, processes: Dict[int, str]
) -> None:
    # SysprofCaptureProcess: frame, cmdline. Frames are processed in time order, so a
    # pid seen more than once (e.g. after it re-execs) keeps its most recent cmdline.
    (pid,) = struct.unpack_from("<i", data, offset + 4)
    processes[pid] = _read_cstring(data, offset + 24, length - 24) or ""


def _is_wanted_bundled_path(path: str) -> bool:
    """Whether `path` names one of the files this reader ever buffers: the
    bundled-symbols and kallsyms files (see _BUNDLED_FILE_NAMES), the recording
    machine's own /proc/mounts, or some profiled process's own
    /proc/<pid>/mountinfo. Everything else sysprof-cli bundles (mountinfo.gz for
    a pid never profiled long enough to matter, glxinfo.gz, cpuinfo.gz, a
    container's /run/.containerenv, ...) is of no use here and is left unread
    rather than buffered for nothing.
    """
    return (
        path in _BUNDLED_FILE_NAMES
        or path in _PROC_MOUNTS_FILE_NAMES
        or _MOUNTINFO_PATH_RE.match(path) is not None
    )


def _accumulate_bundled_file_chunk(
    data: mmap.mmap,
    offset: int,
    length: int,
    chunks_by_path: Dict[str, List[bytes]],
    completed_paths: set,
) -> None:
    # SysprofCaptureFileChunk: frame, is_last:1 + padding1:15 + len:16, path[256],
    # data[len]. A file this large arrives split across several of these, all
    # sharing one path, with the last one flagged; a path this reader has no use
    # for (see _is_wanted_bundled_path) is skipped rather than buffered. A path
    # already completed (its is_last chunk already seen) is left alone rather than
    # appended to further: sysprof only ever bundles a given path once per
    # capture, so further chunks under the same name would be a second, distinct
    # file this reader has no way to tell apart from the first, and concatenating
    # the two would just corrupt both.
    (is_last_and_len,) = struct.unpack_from("<I", data, offset + 24)
    is_last = bool(is_last_and_len & 0x1)
    chunk_len = (is_last_and_len >> 16) & 0xFFFF
    path = _read_cstring(data, offset + 28, 256) or ""
    if path not in completed_paths and _is_wanted_bundled_path(path):
        chunk_start = offset + 284
        chunk = bytes(data[chunk_start: chunk_start + chunk_len])
        chunks_by_path.setdefault(path, []).append(chunk)
        if is_last:
            completed_paths.add(path)


def _decode_bundled_file(
    chunks_by_path: Dict[str, List[bytes]], gz_name: str, raw_name: str
) -> Optional[bytes]:
    if chunks_by_path.get(gz_name):
        try:
            return gzip.decompress(b"".join(chunks_by_path[gz_name]))
        except OSError:
            # A capture cut off mid-write can leave a truncated, unreadable blob;
            # that is no different from one that never bundled the file at all.
            return None
    if chunks_by_path.get(raw_name):
        return b"".join(chunks_by_path[raw_name])
    return None


def _decode_bundled_symbols(
    chunks_by_path: Dict[str, List[bytes]],
) -> Dict[int, List[Tuple[int, int, str]]]:
    blob = _decode_bundled_file(chunks_by_path, *_SYMBOLS_FILE_NAMES)
    return _parse_packed_symbols(blob) if blob else {}


def _decode_kallsyms(chunks_by_path: Dict[str, List[bytes]]) -> List[Tuple[int, str]]:
    blob = _decode_bundled_file(chunks_by_path, *_KALLSYMS_FILE_NAMES)
    return _parse_kallsyms(blob) if blob else []


def _decode_host_mount_devices(
    chunks_by_path: Dict[str, List[bytes]],
) -> List[MountDevice]:
    blob = _decode_bundled_file(chunks_by_path, *_PROC_MOUNTS_FILE_NAMES)
    return parse_proc_mounts(blob.decode("utf-8", errors="replace")) if blob else []


def _decode_mounts_by_pid(
    chunks_by_path: Dict[str, List[bytes]],
) -> Dict[int, List[MountInfoEntry]]:
    mounts_by_pid: Dict[int, List[MountInfoEntry]] = {}
    for path in chunks_by_path:
        match = _MOUNTINFO_PATH_RE.match(path)
        if match is None:
            continue
        gz_path = path if path.endswith(".gz") else path + ".gz"
        raw_path = path[: -len(".gz")] if path.endswith(".gz") else path
        blob = _decode_bundled_file(chunks_by_path, gz_path, raw_path)
        if blob:
            mounts_by_pid[int(match.group(1))] = parse_mountinfo(
                blob.decode("utf-8", errors="replace")
            )
    return mounts_by_pid


def _parse_kallsyms(blob: bytes) -> List[Tuple[int, str]]:
    # Each line of /proc/kallsyms is "<address> <type letter> <name> [module]"; only
    # the address and name matter here. kallsyms often lists a run of aliases at the
    # very same address (a function and the section label it starts, say), and
    # keeping only the first of a run, as sysprof itself does, is enough to resolve
    # a stack address to *a* name for that address.
    entries: List[Tuple[int, str]] = []
    last_address: Optional[int] = None
    for line in blob.decode("utf-8", errors="replace").splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        try:
            address = int(fields[0], 16)
        except ValueError:
            continue
        if address == last_address:
            continue
        entries.append((address, fields[2]))
        last_address = address
    entries.sort()
    return entries


def _parse_packed_symbols(blob: bytes) -> Dict[int, List[Tuple[int, int, str]]]:
    by_pid: Dict[int, List[Tuple[int, int, str]]] = {}
    offset = 0
    while offset + _PACKED_SYMBOL_SIZE <= len(blob):
        try:
            addr_begin, addr_end, pid, name_offset, tag_offset, _padding = (
                struct.unpack_from("<QQIIII", blob, offset)
            )
        except struct.error:
            break
        offset += _PACKED_SYMBOL_SIZE

        # sysprof terminates the table with one record of all zeroes; nothing
        # meaningful follows it (the trailing bytes are the string table).
        if addr_begin == 0 and addr_end == 0 and pid == 0 and name_offset == 0:
            break

        name = _read_cstring(blob, name_offset, len(blob) - name_offset)
        if not name:
            continue

        # sysprof itself carries this separately as the symbol's "nick" (e.g. which
        # library a libc/GLib/Gio function belongs to; see
        # sysprof_bundled_symbolizer_symbolize()'s use of tag_offset), shown
        # alongside the name rather than as part of it. Folding it in here as
        # "name (nick)" keeps that information rather than silently dropping it,
        # without needing every other place that reads a bundled symbol's name to
        # know about a second field. An empty tag string is sysprof's own
        # convention for "no nick" (offset 0 conventionally names the empty
        # string), so it is treated exactly like a zero offset.
        tag = (
            _read_cstring(blob, tag_offset, len(blob) - tag_offset)
            if tag_offset
            else ""
        )
        if tag:
            name = f"{name} ({tag})"

        by_pid.setdefault(pid, []).append((addr_begin, addr_end, name))

    for entries in by_pid.values():
        entries.sort()
    return by_pid


def _parse_ctrdef(
    data: mmap.mmap,
    offset: int,
    counter_defs: Dict[int, Dict[str, Any]],
    counter_order: List[int],
) -> None:
    (n_counters,) = struct.unpack_from("<H", data, offset + 24)
    base = offset + 32
    for i in range(n_counters):
        counter_offset = base + i * 128
        category = _read_cstring(data, counter_offset, 32) or ""
        name = _read_cstring(data, counter_offset + 32, 32) or ""
        description = _read_cstring(data, counter_offset + 64, 52) or ""
        (id_and_type,) = struct.unpack_from("<I", data, counter_offset + 116)
        counter_id = id_and_type & 0xFFFFFF
        counter_type = (id_and_type >> 24) & 0xFF

        if counter_id not in counter_defs:
            counter_defs[counter_id] = {
                "category": category,
                "name": name,
                "description": description,
                "type": counter_type,
                "values": [],
            }
            counter_order.append(counter_id)


def _parse_ctrset(
    data: mmap.mmap,
    offset: int,
    length: int,
    time: int,
    counter_defs: Dict[int, Dict[str, Any]],
) -> None:
    (n_groups,) = struct.unpack_from("<H", data, offset + 24)
    base = offset + 32
    for group_index in range(n_groups):
        group_offset = base + group_index * 96
        if group_offset + 96 > offset + length:
            break
        for slot in range(8):
            (counter_id,) = struct.unpack_from("<I", data, group_offset + slot * 4)
            if counter_id == 0:
                break

            counter = counter_defs.get(counter_id)
            if counter is None:
                continue

            value_offset = group_offset + 32 + slot * 8
            if counter["type"] == _COUNTER_TYPE_DOUBLE:
                (value,) = struct.unpack_from("<d", data, value_offset)
            else:
                (value,) = struct.unpack_from("<q", data, value_offset)

            counter["values"].append({"time": time, "offset": time, "value": value})


def _read_cstring(
    data: Union[mmap.mmap, bytes], offset: int, max_len: int
) -> Optional[str]:
    end = offset + max_len
    field = bytes(data[offset:end])
    terminator = field.find(b"\x00")
    if terminator == -1:
        return None
    return field[:terminator].decode("utf-8", errors="replace")


def _format_subtitle(capture_time: str) -> Optional[str]:
    if not capture_time:
        return None
    try:
        parsed = datetime.strptime(capture_time, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=timezone.utc
        )
    except ValueError:
        return f"Recording at {capture_time}"
    return parsed.astimezone().strftime("Recording at %X %x")
