"""Resolves a file offset in a mapped file to its ELF function, reading the
binary from disk at its recorded path or a `MountTranslator` translation of it.
Returns None when the file is unavailable, and `dump` falls back to file+offset.
"""

import bisect
import ctypes
import mmap
import struct
from typing import Any, Dict, List, Optional, Tuple

from ..mounts import MountTranslator

_ELF_MAGIC = b"\x7fELF"
_ELFCLASS64 = 2
_ELFDATA2LSB = 1

_PT_LOAD = 1
_SHT_SYMTAB = 2
_SHT_DYNSYM = 11
_STT_FUNC = 2

# Like sysprof's contrib/elfparser/elfparser.c (read_table(),
# elf_parser_lookup_symbol()), only FUNC symbols with these bindings in the literal
# ".text" section are used.
_STB_LOCAL = 0
_STB_GLOBAL = 1
_STB_WEAK = 2
_VALID_BINDINGS = (_STB_LOCAL, _STB_GLOBAL, _STB_WEAK)

_EHDR_SIZE = 64


class ElfSymbolizer:
    """Resolves file offsets in ELF files, caching each (pid, path) result,
    including failures."""

    def __init__(self, mount_translator: Optional[MountTranslator] = None) -> None:
        self._symbols_by_path: Dict[
            Tuple[Optional[int], str], Optional["_ElfSymbols"]
        ] = {}
        self._mount_translator = mount_translator

    def resolve(
        self, path: str, file_offset: int, pid: Optional[int] = None
    ) -> Optional[Tuple[Any, str]]:
        """(identity, text) as sysprof's ELF reader would give it, or None.
        `identity` is unique per symbol table entry, for collapsing repeated frames.

        Results are keyed by `path` as given even if a translated path was read.
        `pid` only selects the mount namespace for translation.
        """
        cache_key = (pid, path)
        if cache_key not in self._symbols_by_path:
            self._symbols_by_path[cache_key] = _load_elf_symbols(
                path, pid, self._mount_translator
            )
        symbols = self._symbols_by_path[cache_key]
        return symbols.resolve(path, file_offset) if symbols is not None else None


class _ElfSymbols:
    def __init__(
        self,
        entries: List[Tuple[int, int, str]],
        segments: List[Tuple[int, int, int]],
        text_range: Optional[Tuple[int, int]],
    ) -> None:
        entries = sorted(entries)
        self._addresses = [entry[0] for entry in entries]
        self._entries = entries
        self._segments = segments
        self._text_range = text_range

    def resolve(self, path: str, file_offset: int) -> Optional[Tuple[Any, str]]:
        address = self._address_of(file_offset)
        if address is None or not self._entries or self._text_range is None:
            return None

        # elf_parser_lookup_symbol() rejects addresses past the end of .text.
        _text_start, text_end = self._text_range
        if address > text_end:
            return None

        index = bisect.bisect_right(self._addresses, address) - 1
        if index < 0:
            return None

        symbol_address, size, name = self._entries[index]
        offset = address - symbol_address
        # A zero-size symbol (common in assembly) extends to the next one, as in
        # sysprof's reader.
        if size and offset >= size:
            return None

        name = _demangle(name)
        text = name if offset == 0 else f"{name}+0x{offset:x}"
        return (path, symbol_address), text

    def _address_of(self, file_offset: int) -> Optional[int]:
        # Map a file offset to a link-time address via the PT_LOAD segments.
        for segment_offset, segment_size, segment_address in self._segments:
            if segment_offset <= file_offset < segment_offset + segment_size:
                return segment_address + (file_offset - segment_offset)
        return None


def _load_elf_symbols(
    path: str, pid: Optional[int], mount_translator: Optional[MountTranslator]
) -> Optional[_ElfSymbols]:
    for candidate in _candidate_paths(path, pid, mount_translator):
        try:
            with open(candidate, "rb") as fileobj:
                with mmap.mmap(fileobj.fileno(), 0, access=mmap.ACCESS_READ) as data:
                    return _parse_elf_symbols(data)
        except (OSError, ValueError):
            # mmap raises ValueError on an empty file.
            continue
    return None


def _candidate_paths(
    path: str, pid: Optional[int], mount_translator: Optional[MountTranslator]
) -> List[str]:
    """`path` first, then its mount namespace translations for `pid`."""
    candidates = [path]
    if mount_translator is not None and pid is not None:
        for translated in mount_translator.translate(pid, path):
            if translated not in candidates:
                candidates.append(translated)
    return candidates


# sysprof's elf_demangle() tries Rust before abi::__cxa_demangle(). WebKit has no
# Rust, so only __cxa_demangle is loaded, once, instead of running c++filt per name.
def _load_cxa_demangle() -> Optional[Tuple[Any, Any]]:
    try:
        libstdcxx = ctypes.CDLL("libstdc++.so.6")
        libc = ctypes.CDLL(None)
    except OSError:
        return None

    demangle = libstdcxx.__cxa_demangle
    demangle.restype = ctypes.c_void_p
    demangle.argtypes = (
        ctypes.c_char_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
    )
    free = libc.free
    free.argtypes = (ctypes.c_void_p,)
    return demangle, free


_CXA_DEMANGLE = _load_cxa_demangle()
_demangle_cache: Dict[str, str] = {}


def _demangle(name: str) -> str:
    """Demangles "_Z" names like sysprof-elf.c's
    sysprof_elf_get_symbol_at_address_internal(). Returns `name` unchanged on failure.
    """
    if _CXA_DEMANGLE is None or not name.startswith("_Z"):
        return name
    if name in _demangle_cache:
        return _demangle_cache[name]

    demangle, free = _CXA_DEMANGLE
    status = ctypes.c_int()
    result = demangle(
        name.encode("utf-8", "surrogateescape"), None, None, ctypes.byref(status)
    )
    demangled = name
    if result:
        try:
            raw = ctypes.cast(result, ctypes.c_char_p).value
            if raw is not None:
                demangled = raw.decode("utf-8", "replace")
        finally:
            free(result)

    _demangle_cache[name] = demangled
    return demangled


def _parse_elf_symbols(data: Any) -> Optional[_ElfSymbols]:
    if len(data) < _EHDR_SIZE or bytes(data[:4]) != _ELF_MAGIC:
        return None
    if data[4] != _ELFCLASS64 or data[5] != _ELFDATA2LSB:
        # 32-bit and big-endian ELF are not supported.
        return None

    try:
        (e_phoff,) = struct.unpack_from("<Q", data, 0x20)
        (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
        e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x36)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x3A)

        segments = _read_program_headers(data, e_phoff, e_phentsize, e_phnum)
        sections = _read_section_headers(data, e_shoff, e_shentsize, e_shnum)
    except struct.error:
        return None

    if not sections or e_shstrndx >= len(sections):
        return _ElfSymbols([], segments, None)

    try:
        section_names = [
            _read_cstring(data, sections[e_shstrndx]["offset"] + section["name_offset"])
            for section in sections
        ]

        # Like elf_parser_lookup_symbol(), resolve nothing without a ".text".
        text_index = None
        for index, name in enumerate(section_names):
            if name == ".text":
                text_index = index
                break

        if text_index is None:
            return _ElfSymbols([], segments, None)

        text_section = sections[text_index]
        text_range = (text_section["addr"], text_section["addr"] + text_section["size"])

        # Prefer .symtab, which also has static functions. Stripped libraries only
        # have .dynsym.
        symtab = None
        wanted = ((_SHT_SYMTAB, ".symtab"), (_SHT_DYNSYM, ".dynsym"))
        for wanted_type, wanted_name in wanted:
            for section, name in zip(sections, section_names):
                if section["type"] == wanted_type and name == wanted_name:
                    symtab = section
                    break
            if symtab is not None:
                break

        if symtab is None or symtab["link"] >= len(sections):
            return _ElfSymbols([], segments, text_range)

        entries = _read_symbols(data, symtab, sections[symtab["link"]], text_index)
    except struct.error:
        return _ElfSymbols([], segments, None)

    return _ElfSymbols(entries, segments, text_range)


def _read_program_headers(
    data: Any, offset: int, entry_size: int, count: int
) -> List[Tuple[int, int, int]]:
    segments = []
    for index in range(count):
        entry_offset = offset + index * entry_size
        if entry_offset + 40 > len(data):
            break
        p_type, _flags, p_offset, p_vaddr, _paddr, p_filesz = struct.unpack_from(
            "<IIQQQQ", data, entry_offset
        )
        if p_type == _PT_LOAD:
            segments.append((p_offset, p_filesz, p_vaddr))
    return segments


def _read_section_headers(
    data: Any, offset: int, entry_size: int, count: int
) -> List[Dict[str, int]]:
    sections = []
    for index in range(count):
        entry_offset = offset + index * entry_size
        if entry_offset + 44 > len(data):
            break
        name_offset, sh_type, _flags, addr, sh_offset, sh_size, sh_link = (
            struct.unpack_from("<IIQQQQI", data, entry_offset)
        )
        sections.append(
            {
                "name_offset": name_offset,
                "type": sh_type,
                "addr": addr,
                "offset": sh_offset,
                "size": sh_size,
                "link": sh_link,
            }
        )
    return sections


def _read_symbols(
    data: Any, symtab: Dict[str, int], strtab: Dict[str, int], text_index: int
) -> List[Tuple[int, int, str]]:
    entries = []
    entry_size = 24
    offset = symtab["offset"]
    end = min(offset + symtab["size"], len(data))
    while offset + entry_size <= end:
        st_name, st_info, _other, st_shndx, st_value, st_size = struct.unpack_from(
            "<IBBHQQ", data, offset
        )
        offset += entry_size

        symbol_type = st_info & 0xF
        binding = st_info >> 4
        if (
            st_value == 0
            or st_shndx != text_index
            or symbol_type != _STT_FUNC
            or binding not in _VALID_BINDINGS
        ):
            continue

        name = _read_cstring(data, strtab["offset"] + st_name)
        if name:
            entries.append((st_value, st_size, name))
    return entries


def _read_cstring(data: Any, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        return ""
    end = data.find(b"\x00", offset)
    if end == -1:
        return ""
    return bytes(data[offset:end]).decode("utf-8", errors="replace")
