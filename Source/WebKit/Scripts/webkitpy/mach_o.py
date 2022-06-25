#!/usr/bin/env python3
# encoding: utf-8

import os
import re
import struct

from typing import Any, Dict, Iterable, List, Tuple, Callable
from io import BufferedReader, BufferedRandom, BytesIO, SEEK_CUR


MH_OBJECT = 1
MH_EXECUTE = 2
MH_FVMLIB = 3
MH_CORE = 4
MH_PRELOAD = 5
MH_DYLIB = 6
MH_DYLINKER = 7
MH_BUNDLE = 8

MACHO_32LE_SIGNATURE = b'\xce\xfa\xed\xfe'
MACHO_64LE_SIGNATURE = b'\xcf\xfa\xed\xfe'
MACHO_FAT_SIGNATURE = b'\xca\xfe\xba\xbe'

LC_REQ_DYLD = 0x80000000

LC_SEGMENT = 1
LC_LOAD_DYLIB = 12
LC_ID_DYLIB = 13
LC_LOAD_WEAK_DYLIB = 24 | LC_REQ_DYLD
LC_SEGMENT_64 = 25
LC_CODE_SIGNATURE = 29
LC_DYLD_ENVIRONMENT = 39
LC_SOURCE_VERSION = 42

S_ZEROFILL = 1
S_GB_ZEROFILL = 12
S_THREAD_LOCAL_ZEROFILL = 18

CPU_ARCH_MASK = 0xff000000
CPU_ARCH_ABI64 = 0x01000000

CPU_TYPE_X86 = 7
CPU_TYPE_X86_64 = CPU_TYPE_X86 | CPU_ARCH_ABI64
CPU_TYPE_ARM = 12
CPU_TYPE_ARM64 = CPU_TYPE_ARM | CPU_ARCH_ABI64

CPU_SUBTYPE_ARM64E = 2

DYLD_SEGMENT32_STRUCT_SIZE = 56
DYLD_SECTION32_STRUCT_SIZE = 76

DYLD_SEGMENT64_STRUCT_SIZE = 72
DYLD_SECTION64_STRUCT_SIZE = 86

_cpu_names: Dict[int, str] = {
    CPU_TYPE_ARM: "arm",
    CPU_TYPE_ARM64: "arm64",
    CPU_TYPE_X86: "i386",
    CPU_TYPE_X86_64: "x86_64",
}

_arch_suffix: Dict[int, str] = {
    CPU_SUBTYPE_ARM64E: "e"
}

_command_map: Dict[int, Any] = None


def _utf8_bytes_to_str(data: bytes):
    return data.decode('utf-8').split("\x00")[0]


def _mach_o_int_field(offset: int, length: int = 4):
    def _get_mach_o_int_field(offset: int, length: int = 4):
        return lambda self: int.from_bytes(self.data[offset:(offset + length)], byteorder="little", signed=False)

    def _set_mach_o_int_field(offset: int, length: int = 4):
        def func(self, value: int):
            self.data[offset:(offset + length)] = value.to_bytes(length, byteorder="little", signed=False)
        return func

    return property(_get_mach_o_int_field(offset, length), _set_mach_o_int_field(offset, length))


def _mach_o_chararray_field(offset: int, length: int):
    def _get_mach_o_chararray_field(offset: int, length: int):
        return lambda self: _utf8_bytes_to_str(self.data[offset:(offset + length)])

    def _set_mach_o_chararray_field(offset: int, length: int):
        def func(self, value: str):
            encoded_string = value.encode("utf-8")

            if len(encoded_string) > length:
                encoded_string = encoded_string[:length]
            elif len(encoded_string) < length:
                encoded_string += bytearray(length - len(encoded_string))

            self.data[offset:(offset + length)] = encoded_string
        return func

    return property(_get_mach_o_chararray_field(offset, length), _set_mach_o_chararray_field(offset, length))


def _mach_o_varchar_field(offset_property_name: str):
    def _get_mach_o_varchar_field(offset_property_name: str):
        return lambda self: _utf8_bytes_to_str(self.data[getattr(self, offset_property_name):])

    def _set_mach_o_varchar_field(offset_property_name: str):
        def func(self, value: str):
            encoded_string = value.encode("utf-8")
            self.data[getattr(self, offset_property_name):] = encoded_string

            # MACH-O commands needs to be memory-aligned.
            if (len(self.data) % 8) != 0:
                self.data += bytearray(8 - (len(self.data) % 8))

            self.command_size = len(self.data)
        return func

    return property(_get_mach_o_varchar_field(offset_property_name), _set_mach_o_varchar_field(offset_property_name))


class MachOCommand:
    data: bytearray
    command: int = _mach_o_int_field(0)
    command_size: int = _mach_o_int_field(4)

    def __init__(self, data: bytes):
        self.data = bytearray(data)


class MachOSourceVersionCommand(MachOCommand):
    version: int = _mach_o_int_field(8, 8)

    def version_str(self) -> str:
        return f"{(self.version >> 40) & 0xFFFFFF}.{(self.version >> 30) & 0x3FF}.{(self.version >> 20) & 0x3FF}.{(self.version >> 10) & 0x3FF}.{self.version & 0x3FF}"


class MachOSection:
    data: bytearray
    sectname: str
    segname: str
    addr: int
    size: int
    offset: int
    align: int
    reloff: int
    nreloc: int
    flags: int
    reserved1: int
    reserved2: int

    def __init__(self, data: bytes):
        self.data = bytearray(data)


class MachOSegmentCommand(MachOCommand):
    name: str
    vmaddr: int
    vmsize: int
    fileoff: int
    filesize: int
    maxprot: int
    initprot: int
    nsects: int
    flags: int
    sections: List[MachOSection]


class MachOSection32(MachOSection):
    sectname: str = _mach_o_chararray_field(8, 16)
    segname: str = _mach_o_chararray_field(24, 16)
    addr: int = _mach_o_int_field(40)
    size: int = _mach_o_int_field(44)
    offset: int = _mach_o_int_field(48)
    align: int = _mach_o_int_field(52)
    reloff: int = _mach_o_int_field(56)
    nreloc: int = _mach_o_int_field(60)
    flags: int = _mach_o_int_field(64)
    reserved1: int = _mach_o_int_field(68)
    reserved2: int = _mach_o_int_field(72)


class MachOSegment32Command(MachOSegmentCommand):
    name: str = _mach_o_chararray_field(8, 16)
    vmaddr: int = _mach_o_int_field(24)
    vmsize: int = _mach_o_int_field(28)
    fileoff: int = _mach_o_int_field(32)
    filesize: int = _mach_o_int_field(36)
    maxprot: int = _mach_o_int_field(40)
    initprot: int = _mach_o_int_field(44)
    nsects: int = _mach_o_int_field(48)
    flags: int = _mach_o_int_field(52)

    def __init__(self, data: bytes):
        super().__init__(data)

        offset = DYLD_SEGMENT32_STRUCT_SIZE
        self.sections = []
        for section in range(0, self.nsects):
            self.sections.append(MachOSection32(data[offset:(offset + DYLD_SECTION32_STRUCT_SIZE)]))
            offset += DYLD_SECTION32_STRUCT_SIZE


class MachOSection64(MachOSection):
    sectname: str = _mach_o_chararray_field(8, 16)
    segname: str = _mach_o_chararray_field(24, 16)
    addr: int = _mach_o_int_field(40, 8)
    size: int = _mach_o_int_field(48, 8)
    offset: int = _mach_o_int_field(56)
    align: int = _mach_o_int_field(60)
    reloff: int = _mach_o_int_field(64)
    nreloc: int = _mach_o_int_field(68)
    flags: int = _mach_o_int_field(72)
    reserved1: int = _mach_o_int_field(76)
    reserved2: int = _mach_o_int_field(80)
    reserved3: int = _mach_o_int_field(84)


class MachOSegment64Command(MachOSegmentCommand):
    name: str = _mach_o_chararray_field(8, 16)
    vmaddr: int = _mach_o_int_field(24, 8)
    vmsize: int = _mach_o_int_field(32, 8)
    fileoff: int = _mach_o_int_field(40, 8)
    filesize: int = _mach_o_int_field(48, 8)
    maxprot: int = _mach_o_int_field(56)
    initprot: int = _mach_o_int_field(60)
    nsects: int = _mach_o_int_field(64)
    flags: int = _mach_o_int_field(68)

    def __init__(self, data: bytes):
        super().__init__(data)

        offset = DYLD_SEGMENT64_STRUCT_SIZE
        self.sections = []
        for section in range(0, self.nsects):
            self.sections.append(MachOSection32(data[offset:(offset + DYLD_SECTION64_STRUCT_SIZE)]))
            offset += DYLD_SECTION64_STRUCT_SIZE


class MachOLoadDylibCommand(MachOCommand):
    install_name_offset: int = _mach_o_int_field(8)
    timestamp: int = _mach_o_int_field(12)
    current_version: int = _mach_o_int_field(16)
    compatibility_version: int = _mach_o_int_field(20)
    install_name: str = _mach_o_varchar_field("install_name_offset")

    def _version_str(self, version: int) -> str:
        return f"{(version >> 16) & 0xffff}.{(version >> 8) & 0xff}.{version & 0xff}"

    def compatibility_version_str(self) -> str:
        return self._version_str(self.compatibility_version)

    def current_version_str(self) -> str:
        return self._version_str(self.current_version)


class MachODyldEnvironmentCommand(MachOCommand):
    pair_offset: int = _mach_o_int_field(8)
    pair: str = _mach_o_varchar_field("pair_offset")

    @property
    def variable(self):
        return self.pair.split("=")[0]

    @property
    def value(self):
        return self.pair.split("=")[1]

    @classmethod
    def build(self, pair: str) -> "MachODyldEnvironmentCommand":
        command = self(struct.pack("<IIII", LC_DYLD_ENVIRONMENT, 16, 12, 0))
        command.pair = pair
        return command


class MachOIDDylibCommand(MachOLoadDylibCommand):
    pass


_command_map = {
    LC_SEGMENT: MachOSegment32Command,
    LC_SEGMENT_64: MachOSegment64Command,
    LC_LOAD_DYLIB: MachOLoadDylibCommand,
    LC_LOAD_WEAK_DYLIB: MachOLoadDylibCommand,
    LC_SOURCE_VERSION: MachOSourceVersionCommand,
    LC_DYLD_ENVIRONMENT: MachODyldEnvironmentCommand,
    LC_ID_DYLIB: MachOIDDylibCommand,
}


class MachOHeader:
    data: bytearray
    start_offset: int
    end_offset: int
    magic: int
    cputype: int
    cpusubtype: int
    filetype: int
    command_count: int
    command_size: int
    commands: List[MachOCommand]
    flags: int

    def __init__(self):
        self.commands = []

    @property
    def used_header_size(self) -> int:
        return self.end_offset - self.start_offset

    @property
    def total_header_size(self) -> int:
        lowest_offset = None

        for command in self.commands:
            if not isinstance(command, MachOSegmentCommand):
                continue

            for section in command.sections:
                if section.offset <= 0 or section.flags & S_GB_ZEROFILL or section.flags & S_THREAD_LOCAL_ZEROFILL or section.flags & S_ZEROFILL:
                    continue

                if lowest_offset is None:
                    lowest_offset = section.offset
                else:
                    lowest_offset = min(lowest_offset, section.offset)

        return lowest_offset

    @property
    def available_header_size(self) -> int:
        return self.total_header_size - self.used_header_size

    @property
    def architecture_name(self) -> str:
        if self.cputype in _cpu_names:
            if self.cpusubtype in _arch_suffix:
                return _cpu_names[self.cputype] + _arch_suffix[self.cpusubtype]

            return _cpu_names[self.cputype]

        return "%i.%i" % (self.cputype, self.subcputype)

    @property
    def id_command(self) -> MachOIDDylibCommand:
        for command in self.commands:
            if isinstance(command, MachOIDDylibCommand):
                return command

    @property
    def dyld_env_commands(self) -> Iterable[MachODyldEnvironmentCommand]:
        for command in self.commands:
            if isinstance(command, MachODyldEnvironmentCommand):
                yield command

    @property
    def dyld_versioned_framework_paths(self) -> Iterable[str]:
        for command in self.dyld_env_commands:
            if command.variable == "DYLD_VERSIONED_FRAMEWORK_PATH":
                for entry in command.value.split(":"):
                    yield entry

    def filter_commands(self, func: Callable[[MachOCommand], bool]) -> None:
        self.commands = list(filter(func, self.commands))

    def update_file(self, binary_file: BufferedRandom) -> None:
        command_size = 0

        for command in self.commands:
            command_size += len(command.data)

        updated_header_size = len(self.data) + command_size
        total_header_size = self.total_header_size

        if updated_header_size > total_header_size:
            raise RuntimeError(f"Updated header ({updated_header_size} bytes) doesn't fit the available space ({total_header_size} bytes)")

        self.command_count = len(self.commands)
        self.command_size = command_size

        binary_file.seek(self.start_offset)
        binary_file.write(self.data)

        for command in self.commands:
            binary_file.write(command.data)

        if self.end_offset > binary_file.tell():
            binary_file.write(bytearray(self.end_offset - binary_file.tell()))
        else:
            self.end_offset = binary_file.tell()

    @classmethod
    def parse_command(self, data: bytes) -> MachOCommand:
        command = int.from_bytes(data[0:4], byteorder="little")
        if command in _command_map:
            return _command_map[command](data)

        return MachOCommand(data)


class MachOHeader32LE(MachOHeader):
    magic: int = _mach_o_int_field(0)
    cputype: int = _mach_o_int_field(4)
    cpusubtype: int = _mach_o_int_field(8)
    filetype: int = _mach_o_int_field(12)
    command_count: int = _mach_o_int_field(16)
    command_size: int = _mach_o_int_field(20)
    flags: int = _mach_o_int_field(24)

    def __init__(self, file: BufferedReader):
        super().__init__()
        self.start_offset = file.tell()
        self.data = bytearray(file.read(28))

        for _ in range(0, self.command_count):
            (_, command_size) = struct.unpack("<II", file.peek(8)[:8])
            self.commands.append(MachOHeader.parse_command(file.read(command_size)))

        self.end_offset = file.tell()


class MachOHeader64LE(MachOHeader):
    magic: int = _mach_o_int_field(0)
    cputype: int = _mach_o_int_field(4)
    cpusubtype: int = _mach_o_int_field(8)
    filetype: int = _mach_o_int_field(12)
    command_count: int = _mach_o_int_field(16)
    command_size: int = _mach_o_int_field(20)
    flags: int = _mach_o_int_field(24)
    reserved: int = _mach_o_int_field(28)

    def __init__(self, file: BufferedReader):
        super().__init__()
        self.start_offset = file.tell()
        self.data = bytearray(file.read(32))

        for _ in range(0, self.command_count):
            (_, command_size) = struct.unpack("<II", file.peek(8)[:8])
            self.commands.append(MachOHeader.parse_command(file.read(command_size)))

        self.end_offset = file.tell()


class MachOFile:
    headers: List[MachOHeader]

    def __init__(self, file: BufferedReader):
        self.headers = []
        magic = file.peek(4)[:4]

        if magic == MACHO_32LE_SIGNATURE:
            # 32-bits Mach-O
            self.headers.append(MachOHeader32LE(file))
            return

        if magic == MACHO_64LE_SIGNATURE:
            # 64-bits Mach-O
            self.headers.append(MachOHeader64LE(file))
            return

        if magic == MACHO_FAT_SIGNATURE:
            # Universal Mach-O
            (magic, architecture_count) = struct.unpack('>II', file.read(8))
            architectures = []

            for _ in range(0, architecture_count):
                architectures.append(file.read(20))

            for architecture in architectures:
                (cpu_type, cpu_sub_type, file_offset, size, align) = struct.unpack('>IIIII', architecture)
                file.seek(file_offset)
                magic = file.peek(4)[:4]

                if magic == MACHO_32LE_SIGNATURE:
                    self.headers.append(MachOHeader32LE(file))
                elif magic == MACHO_64LE_SIGNATURE:
                    self.headers.append(MachOHeader64LE(file))
                else:
                    raise RuntimeError("Invalid Mach-O header found")

            return

        raise RuntimeError("Not a supported Mach-O file")


def is_macho_bytes(data: bytes) -> bool:
    if len(data) < 4:
        return False

    magic = data[:4]
    return magic in [MACHO_64LE_SIGNATURE, MACHO_32LE_SIGNATURE, MACHO_FAT_SIGNATURE]


def is_macho_file(filename: str) -> bool:
    with open(filename, "rb") as file:
        magic = file.peek(4)[:4]
        return is_macho_bytes(magic)


def enumerate_macho_files(start_point: str) -> Iterable[Tuple[str, MachOFile]]:
    import tarfile

    if os.path.isdir(start_point):
        for root, _, files in os.walk(start_point, topdown=False, followlinks=False):
            for name in files:
                if name.endswith(".a"):
                    continue

                path = os.path.join(root, name)

                if os.path.islink(path):
                    continue

                try:
                    if not is_macho_file(path):
                        continue

                    with open(path, "rb") as file:
                        macho_file = MachOFile(file)
                except:
                    continue

                yield (os.path.relpath(path, start_point), macho_file)
    elif tarfile.is_tarfile(start_point):
        archive = tarfile.open(start_point)

        while True:
            member = archive.next()
            if not member:
                break

            if not member.isfile():
                continue

            if member.name.endswith(".a"):
                continue

            data = archive.extractfile(member).read()

            if is_macho_bytes(data):
                yield (os.path.normpath(member.name), MachOFile(BufferedReader(BytesIO(data))))
    else:
        with open(start_point, "rb") as file:
            yield (start_point, MachOFile(file))
