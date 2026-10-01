"""Resolves an address within a mapped file to the ELF function that covers it.

The capture itself carries no symbols, only which file was mapped where (see
`dump`'s use of the MAP frames the parser collects). This reads that file's own ELF
symbol table off disk, which is only possible where the exact binary the capture was
recorded against is still reachable at (or, via a `MountTranslator`, translatable to)
the path it was mapped from -- e.g. a capture taken on the machine it is now being
dumped on, or a WebKit binary recorded while running inside a container whose bind
mount for that path this machine still has mounted elsewhere (see ../mounts). Where
neither can be read or parsed, resolution quietly gives up, and `dump` falls back to
naming the file and offset instead.
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

# Sysprof's own local ELF reader (contrib/elfparser/elfparser.c's read_table() and
# elf_parser_lookup_symbol()) only trusts a symbol defined in the file's literal
# ".text" section, of this type and one of these ordinary bindings; matching that
# here keeps this from resolving a frame sysprof itself would only show as "In File
# ...+0x..." for, e.g. one in .text.unlikely or a PLT stub.
_STB_LOCAL = 0
_STB_GLOBAL = 1
_STB_WEAK = 2
_VALID_BINDINGS = (_STB_LOCAL, _STB_GLOBAL, _STB_WEAK)

_EHDR_SIZE = 64


class ElfSymbolizer:
    """Resolves file offsets against whatever ELF files it is asked about.

    Reads and parses a given file's symbol table at most once: the first resolve()
    against a (pid, path) loads it, and every later one, successful or not, reuses
    what was found (or the fact that nothing was).
    """

    def __init__(self, mount_translator: Optional[MountTranslator] = None) -> None:
        self._symbols_by_path: Dict[Tuple[Optional[int], str], Optional["_ElfSymbols"]] = {}
        self._mount_translator = mount_translator

    def resolve(
        self, path: str, file_offset: int, pid: Optional[int] = None
    ) -> Optional[Tuple[Any, str]]:
        """The (identity, text) sysprof's own ELF reader would give this file offset,
        or None. `identity` is the same for every offset resolved to the same table
        entry, and different for every other resolution this or any other symbolizer
        makes, so a caller collapsing consecutive identical frames (the way sysprof
        itself does) can compare identities without caring what the display text is.

        Identity and display text are always keyed by `path` exactly as given, even
        where the file actually read from disk was found via `pid`'s mount
        namespace translation (see ../mounts.MountTranslator): what changes is only
        where the bytes came from, not what a caller should treat this as the same
        file as. `pid` is only ever used to pick which process's mount namespace to
        translate `path` through; passing none (the default) just means no
        translation is attempted beyond `path` itself.
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
        # Sorted by address, so the function covering a given one is found by
        # bisecting rather than scanning every symbol the file has.
        entries = sorted(entries)
        self._addresses = [entry[0] for entry in entries]
        self._entries = entries
        self._segments = segments
        self._text_range = text_range

    def resolve(self, path: str, file_offset: int) -> Optional[Tuple[Any, str]]:
        address = self._address_of(file_offset)
        if address is None or not self._entries or self._text_range is None:
            return None

        # Sysprof's own ELF reader (contrib/elfparser/elfparser.c's
        # elf_parser_lookup_symbol()) rejects anything past the end of the .text
        # section outright, before ever looking at which symbol would have matched.
        _text_start, text_end = self._text_range
        if address > text_end:
            return None

        index = bisect.bisect_right(self._addresses, address) - 1
        if index < 0:
            return None

        symbol_address, size, name = self._entries[index]
        offset = address - symbol_address
        # A non-zero size caps how far a symbol reaches; a symbol of unknown size
        # (common for hand-written assembly) is not capped at all and, via the
        # bisect above, reaches until whichever symbol comes next (or .text's own
        # end, already checked above) — the same as sysprof's own reader treats it,
        # rather than only ever covering the exact address it was recorded at.
        if size and offset >= size:
            return None

        name = _demangle(name)
        text = name if offset == 0 else f"{name}+0x{offset:x}"
        return (path, symbol_address), text

    def _address_of(self, file_offset: int) -> Optional[int]:
        # A file offset is where a stack address landed within the file on disk; a
        # symbol's address is where the linker placed it in the file's own address
        # space. A loadable segment is what ties the two together, since it is
        # written with both its place in the file and the address it loads at.
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
            # ValueError is what mmap raises on an empty file; either way, a
            # candidate this tool cannot even open is no different from one with
            # nothing to resolve, so the next candidate (if any) is tried instead.
            continue
    return None


def _candidate_paths(
    path: str, pid: Optional[int], mount_translator: Optional[MountTranslator]
) -> List[str]:
    """`path` itself, tried first since it is already correct outside of any
    container (or from inside whichever one recorded it), followed by whatever
    `pid`'s own mount namespace translates it to (see ../mounts.MountTranslator),
    most specific match first, for a path that is only reachable that way -- e.g.
    a WebKit binary a container bind-mounted from a real directory this machine
    still has mounted somewhere, just not at that same container-internal path.
    """
    candidates = [path]
    if mount_translator is not None and pid is not None:
        for translated in mount_translator.translate(pid, path):
            if translated not in candidates:
                candidates.append(translated)
    return candidates


# elf_demangle() in sysprof's own contrib/elfparser/elfparser.c tries Rust
# demangling first (legacy Rust manglings can otherwise partially demangle as C++)
# and only then abi::__cxa_demangle(); WebKit has no Rust in it, so this only ever
# needs the second half of that -- the same routine libstdc++ itself uses, loaded
# once and reused for every symbol name rather than shelling out to c++filt per
# name.
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
    """The demangled form of `name`, exactly like sysprof-elf.c's own
    sysprof_elf_get_symbol_at_address_internal() -- which only ever attempts this
    for a name starting with the Itanium C++ mangling prefix -- would show, or
    `name` unchanged where demangling does not apply, isn't available on this
    machine, or fails (e.g. a name an unrelated convention also happened to start
    with "_Z").
    """
    if _CXA_DEMANGLE is None or not name.startswith("_Z"):
        return name
    if name in _demangle_cache:
        return _demangle_cache[name]

    demangle, free = _CXA_DEMANGLE
    status = ctypes.c_int()
    result = demangle(name.encode("utf-8", "surrogateescape"), None, None, ctypes.byref(status))
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
        # 32-bit and big-endian ELF are not handled; dump() falls back to naming the
        # file and offset for those instead of resolving a symbol.
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

        # Sysprof's own ELF reader never resolves anything in a file with no
        # literal ".text" section at all (elf_parser_lookup_symbol() bails out
        # before even looking at the symbol table), and only ever trusts a symbol
        # defined in that exact section; matching that here is what
        # _read_symbols() below filters candidates against.
        text_index = None
        for index, name in enumerate(section_names):
            if name == ".text":
                text_index = index
                break

        if text_index is None:
            return _ElfSymbols([], segments, None)

        text_section = sections[text_index]
        text_range = (text_section["addr"], text_section["addr"] + text_section["size"])

        # A stripped shared library keeps only .dynsym, the exported subset of a full
        # .symtab; prefer .symtab where both exist, since it also carries the local
        # (static) functions that never made it into .dynsym.
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
