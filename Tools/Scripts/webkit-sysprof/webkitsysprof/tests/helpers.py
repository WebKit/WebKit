"""Builders for synthetic captures and a test case that captures output.

Synthetic marks are shaped like parser output. They carry no begin time, only
`end_time` and `duration`, and name their process by pid and by group.
"""

import builtins
import struct
import unittest

from webkitcorepy import OutputCapture

from webkitsysprof.utils import msec_to_nsec

_ELF_EHDR_SIZE = 64
_ELF_PHDR_SIZE = 56
_ELF_SHDR_SIZE = 64
_ELF_SHT_STRTAB = 3
_ELF_SHT_SYMTAB = 2
_ELF_STT_FUNC = 2


def build_minimal_elf(symbols, code_size=0x2000):
    """An ELF64 LE file with one R+X PT_LOAD at vaddr 0, so file offsets equal vaddrs.

    The zeroed code lies in a ".text" section, the only section sysprof trusts
    symbols in. `symbols` are (name, file_offset, size) tuples.
    """
    code_offset = _ELF_EHDR_SIZE + _ELF_PHDR_SIZE
    code = b"\x00" * code_size
    segment_size = code_offset + code_size

    strtab = b"\x00"
    name_offsets = []
    for name, _offset, _size in symbols:
        name_offsets.append(len(strtab))
        strtab += name.encode() + b"\x00"
    strtab_offset = code_offset + len(code)

    # Index 0 is the null section, so .text is index 1.
    section_names = [b"", b".text", b".symtab", b".strtab", b".shstrtab"]
    text_index = 1
    shstrtab = b"\x00".join(section_names) + b"\x00"
    shstrtab_name_offset = {}
    position = 0
    for name in section_names:
        shstrtab_name_offset[name] = position
        position += len(name) + 1
    shstrtab_offset = strtab_offset + len(strtab)

    symtab = struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)  # index 0 is always null.
    for (name, file_offset, size), name_offset in zip(symbols, name_offsets):
        st_info = _ELF_STT_FUNC  # binding STB_LOCAL (0), type STT_FUNC.
        symtab += struct.pack(
            "<IBBHQQ", name_offset, st_info, 0, text_index, file_offset, size
        )
    symtab_offset = shstrtab_offset + len(shstrtab)

    section_header_offset = symtab_offset + len(symtab)

    def section_header(name, sh_type, addr, offset, size, link):
        return struct.pack(
            "<IIQQQQIIQQ",
            shstrtab_name_offset[name],
            sh_type,
            0,
            addr,
            offset,
            size,
            link,
            0,
            1,
            0,
        )

    sections = (
        section_header(b"", 0, 0, 0, 0, 0)
        + section_header(b".text", 1, code_offset, code_offset, code_size, 0)
        + section_header(b".symtab", _ELF_SHT_SYMTAB, 0, symtab_offset, len(symtab), 3)
        + section_header(b".strtab", _ELF_SHT_STRTAB, 0, strtab_offset, len(strtab), 0)
        + section_header(
            b".shstrtab", _ELF_SHT_STRTAB, 0, shstrtab_offset, len(shstrtab), 0
        )
    )

    ehdr = bytearray(_ELF_EHDR_SIZE)
    ehdr[0:4] = b"\x7fELF"
    ehdr[4] = 2  # ELFCLASS64
    ehdr[5] = 1  # ELFDATA2LSB
    ehdr[6] = 1  # EI_VERSION
    struct.pack_into("<H", ehdr, 0x10, 3)  # e_type = ET_DYN
    struct.pack_into("<Q", ehdr, 0x20, _ELF_EHDR_SIZE)  # e_phoff
    struct.pack_into("<Q", ehdr, 0x28, section_header_offset)  # e_shoff
    struct.pack_into("<H", ehdr, 0x34, _ELF_EHDR_SIZE)  # e_ehsize
    struct.pack_into("<H", ehdr, 0x36, _ELF_PHDR_SIZE)  # e_phentsize
    struct.pack_into("<H", ehdr, 0x38, 1)  # e_phnum
    struct.pack_into("<H", ehdr, 0x3A, _ELF_SHDR_SIZE)  # e_shentsize
    struct.pack_into("<H", ehdr, 0x3C, 5)  # e_shnum
    struct.pack_into("<H", ehdr, 0x3E, 4)  # e_shstrndx: .shstrtab is section 4

    phdr = struct.pack(
        "<IIQQQQQQ",
        1,  # p_type = PT_LOAD
        5,  # p_flags = PF_R | PF_X
        0,  # p_offset
        0,  # p_vaddr
        0,  # p_paddr
        segment_size,  # p_filesz
        segment_size,  # p_memsz
        0x1000,
    )

    return bytes(ehdr) + phdr + code + strtab + shstrtab + symtab + sections


def mark(name, begin_msec, end_msec, group="WebKit (Web)", pid=1, message=""):
    return {
        "group": group,
        "pid": pid,
        "name": name,
        "message": message,
        "duration": msec_to_nsec(end_msec - begin_msec),
        "end_time": msec_to_nsec(end_msec),
    }


def instant(name, msec, message="", pid=1, group="WebKit (Web)"):
    return mark(name, msec, msec, group=group, pid=pid, message=message)


def _reasons_text(reasons):
    return ", ".join(reasons)


def composition(begin, end, reasons=("Animation",), pid=1, frame_at=None):
    """A RenderLayerTree holding a DidRenderFrame, a flush and a paint.

    The frame is at the midpoint unless `frame_at` is given. The flush and paint
    split the time before it. `reasons=None` gives an empty message, as before reasons were traced.
    """
    frame = (begin + end) / 2 if frame_at is None else frame_at
    message = "" if reasons is None else "reasons: " + _reasons_text(reasons)
    middle = begin + (frame - begin) / 2
    return [
        mark("RenderLayerTree", begin, end, pid=pid),
        instant("DidRenderFrame", frame, message, pid=pid),
        mark("FlushCompositingState", begin, middle, pid=pid),
        mark("PaintToGLContext", middle, frame, pid=pid),
    ]


def scheduled(msec, state, waiting=False, timer=False, reasons=(), pid=1):
    """A ScheduleComposition, worded as ThreadedCompositor.cpp words it."""
    return instant(
        "ScheduleComposition",
        msec,
        "reasons: {}, state: {}, waiting for tiles: {}, render timer active: {}".format(
            _reasons_text(reasons),
            state,
            "yes" if waiting else "no",
            "yes" if timer else "no",
        ),
        pid=pid,
    )


def frame_complete(msec, state, waiting=False, reasons=(), pid=1):
    """A FrameComplete, worded as ThreadedCompositor.cpp words it."""
    return instant(
        "FrameComplete",
        msec,
        "reasons: {}, state: {}, waiting for tiles: {}".format(
            _reasons_text(reasons), state, "yes" if waiting else "no"
        ),
        pid=pid,
    )


def requested(msec, pid=1):
    return instant("RequestCompositionForRenderingUpdate", msec, pid=pid)


def rendering_update(begin, end, pid=1):
    return mark("LayerTreeHostRenderingUpdate", begin, end, pid=pid)


def presented(msec, interval=16.667):
    return instant(
        "WaylandFramePresented",
        msec,
        "sequence=1, latency=1.000 ms, refreshInterval={} ms".format(interval),
        pid=0,
        group="WebKit (UI)",
    )


def discarded(msec):
    return instant("WaylandFrameDiscarded", msec, pid=0, group="WebKit (UI)")


def steady_animation(begin, frames, period=16.0, pid=1):
    """An accelerated animation compositing once per `period`.

    Each paint requests the next frame, so every frame but the last holds an
    InProgress ScheduleComposition and ends in a ScheduledWhileInProgress
    FrameComplete, which restarts the timer.
    """
    marks = [scheduled(begin - 0.5, "Idle", reasons=("Animation",), pid=pid)]
    for i in range(frames):
        t = begin + i * period
        marks += composition(t, t + 1, ("Animation",), pid=pid)
        last = i == frames - 1
        if not last:
            marks.append(
                scheduled(t + 0.3, "InProgress", reasons=("Animation",), pid=pid)
            )
        marks.append(
            frame_complete(
                t + period - 0.5,
                "InProgress" if last else "ScheduledWhileInProgress",
                reasons=() if last else ("Animation",),
                pid=pid,
            )
        )
    return marks


def stacktrace(pid, tid, addresses, time_msec=0):
    return {
        "pid": pid,
        "tid": tid,
        "time": msec_to_nsec(time_msec),
        "addresses": addresses,
    }


def stacktrace_data(
    stacktraces,
    maps=None,
    processes=None,
    symbols=None,
    kernel_symbols=None,
    host_mount_devices=None,
    mounts_by_pid=None,
):
    return {
        "stacktraces": stacktraces,
        "maps": maps or {},
        "processes": processes or {},
        "symbols": symbols or {},
        "kernel_symbols": kernel_symbols or [],
        "host_mount_devices": host_mount_devices or [],
        "mounts_by_pid": mounts_by_pid or {},
    }


def sysprof_data(marks, begin_msec=0, end_msec=1000):
    marks_by_name = {}
    for a_mark in marks:
        marks_by_name.setdefault(a_mark["name"], []).append(a_mark)
    return {
        "document": {
            "timespan": {
                "begin": msec_to_nsec(begin_msec),
                "end": msec_to_nsec(end_msec),
            }
        },
        "marks": marks_by_name,
    }


class approx:
    """A number compared within a tolerance, relative unless `abs` is given.

    Works inside lists and dicts, since float comparison defers to it.
    """

    def __init__(self, value, rel=1e-6, abs=None):
        self.value = value
        self.tolerance = abs if abs is not None else rel * max(builtins.abs(value), 1.0)

    def __eq__(self, other):
        return builtins.abs(other - self.value) <= self.tolerance

    def __repr__(self):
        return f"~{self.value!r}"


class SysprofTestCase(unittest.TestCase):
    """Captures stdout and the root logger so logging setup does not leak between tests."""

    def setUp(self):
        capture = OutputCapture()
        capture.__enter__()
        self.addCleanup(capture.__exit__, None, None, None)
        self._capture = capture

    def stdout(self):
        return self._capture.stdout.getvalue()
