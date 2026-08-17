import mmap
import os
import struct
import logging
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Tuple

_MAGIC = 0xFDCA975E
_FILE_HEADER_SIZE = 256
_FRAME_HEADER_SIZE = 24
_FRAME_ALIGN = 8

_FRAME_CTRDEF = 8
_FRAME_CTRSET = 9
_FRAME_MARK = 10

# Frame types considered "data" by libsysprof when guessing a capture's end
# time (everything except TIMESTAMP, CTRDEF, FILE_CHUNK, JITMAP, METADATA,
# and OVERLAY).
_DATA_FRAME_TYPES = {2, 3, 4, 5, 6, 9, 10, 12, 14, 16, 17}

_COUNTER_TYPE_DOUBLE = 1


def parse(file_path: str, marks: bool, counters: bool) -> Dict[str, Any]:
    assert marks or counters

    with open(file_path, "rb") as fileobj:
        with mmap.mmap(fileobj.fileno(), 0, access=mmap.ACCESS_READ) as data:
            return _parse_capture(data, file_path, marks, counters)


def _parse_capture(
    data: mmap.mmap, file_path: str, marks: bool, counters: bool
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

    parsed_marks.sort(key=lambda mark: (mark["group"], mark["name"], mark["end_time"]))

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
    group = _read_cstring(data, offset + 32, 24) or ""
    name = _read_cstring(data, offset + 56, 40) or ""
    message = _read_cstring(data, offset + 96, length - 96) or ""
    return {
        "name": name,
        "message": message,
        "duration": duration,
        "end_time": end_time,
        "group": group,
    }


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


def _read_cstring(data: mmap.mmap, offset: int, max_len: int) -> Optional[str]:
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
