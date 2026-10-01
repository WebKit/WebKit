"""What the tests build captures from, and read what a command printed with.

A synthetic mark is shaped like what the parser produces: it carries no begin
time, so everything derives one from `end_time` and `duration`, and it names the
process that emitted it by pid as well as by kind.
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
    """An ELF64 LE file with one R+X PT_LOAD segment covering file offset 0 up to
    the end of `code_size` bytes of (placeholder, all-zero) code, mapped at vaddr 0,
    so a file offset and the vaddr it translates to are the same number. The code
    is covered by a ".text" section (sysprof's own ELF reader only ever trusts a
    symbol defined in a section literally named that), and a .symtab names
    `symbols`, each given as (name, file_offset, size), pointing into it.
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

    # Index 0 is always the reserved null section; .text is index 1, so that is
    # what the symbols built below point their st_shndx at.
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


def mark(name, begin_msec, end_msec, group="WebKit (Web)", pid=1):
    return {
        "group": group,
        "pid": pid,
        "name": name,
        "message": "",
        "duration": msec_to_nsec(end_msec - begin_msec),
        "end_time": msec_to_nsec(end_msec),
    }


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
    """A number equal to another within a tolerance, relative unless `abs` says so.

    Durations are divided and summed before they are compared, so comparing them
    exactly would test the rounding of binary floating point rather than the
    analysis. Works inside a list or a dict too, since a float compared against one
    of these defers to it.
    """

    def __init__(self, value, rel=1e-6, abs=None):
        self.value = value
        self.tolerance = abs if abs is not None else rel * max(builtins.abs(value), 1.0)

    def __eq__(self, other):
        return builtins.abs(other - self.value) <= self.tolerance

    def __repr__(self):
        return f"~{self.value!r}"


class SysprofTestCase(unittest.TestCase):
    """A test reading back what the command under it printed.

    OutputCapture keeps stdout and the root logger to itself, so that a command
    configuring logging does not outlive the test that ran it.
    """

    def setUp(self):
        capture = OutputCapture()
        capture.__enter__()
        self.addCleanup(capture.__exit__, None, None, None)
        self._capture = capture

    def stdout(self):
        return self._capture.stdout.getvalue()
