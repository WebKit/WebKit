"""Fine-grained tests for the ELF symbol table reader in webkitsysprof.symbolizer.

Builds the smallest ELF64 files that exercise it byte by byte (one PT_LOAD segment,
one .text/.symtab/.strtab set), the same approach parser_unittest.py takes for
.syscap frames, so a change to the field layout is caught here rather than only
showing up as symbols silently failing to resolve downstream.
"""

import struct
import tempfile
import unittest
from pathlib import Path

from .helpers import build_minimal_elf

from webkitsysprof.mounts import MountDevice, MountInfoEntry, MountTranslator
from webkitsysprof.symbolizer import _CXA_DEMANGLE, ElfSymbolizer

# Standard ELF64_Sym st_info nibbles: low 4 bits are the type, high 4 the binding.
_STT_OBJECT = 1
_STT_GNU_IFUNC = 10
_STB_GLOBAL = 1 << 4


def _resolve_text(symbolizer, path, file_offset, pid=None):
    """Just the display text of resolve(), for tests that don't care about identity."""
    resolved = symbolizer.resolve(path, file_offset, pid=pid)
    return resolved[1] if resolved is not None else None


class SymbolizerTest(unittest.TestCase):
    def _write(self, data: bytes) -> str:
        tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(tempdir.cleanup)
        path = str(Path(tempdir.name) / "test.so")
        Path(path).write_bytes(data)
        return path

    def test_resolves_an_address_at_the_start_of_a_function(self):
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))

        # code_offset (0 within the PT_LOAD segment) + 0x100 == the file offset the
        # symbol was placed at.
        self.assertEqual(_resolve_text(ElfSymbolizer(), path, 0x100), "my_function")

    def test_resolves_an_address_partway_through_a_function(self):
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))

        self.assertEqual(
            _resolve_text(ElfSymbolizer(), path, 0x110), "my_function+0x10"
        )

    def test_an_address_past_every_known_function_is_not_resolved(self):
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x10)]))

        for file_offset, reason in [
            (0x100 + 0x10, "past the end of the only function"),
            (100 * 1024 * 1024, "beyond the mapped segment entirely"),
        ]:
            with self.subTest(reason=reason):
                self.assertIsNone(ElfSymbolizer().resolve(path, file_offset))

    def test_a_zero_size_symbol_covers_up_to_the_next_symbol(self):
        # Some hand-written assembly carries no size in the symbol table. Sysprof's
        # own ELF reader (contrib/elfparser/elfparser.c) only ever caps a symbol's
        # reach by an explicit non-zero size, so a size-0 symbol is not special:
        # it covers everything up to whichever symbol comes next, the same as one
        # with a real size covers up to its own declared end.
        path = self._write(
            build_minimal_elf([("entry_point", 0x100, 0), ("next_fn", 0x140, 0x10)])
        )
        symbolizer = ElfSymbolizer()

        self.assertEqual(_resolve_text(symbolizer, path, 0x100), "entry_point")
        self.assertEqual(_resolve_text(symbolizer, path, 0x13F), "entry_point+0x3f")
        self.assertEqual(_resolve_text(symbolizer, path, 0x140), "next_fn")

    def test_an_address_past_the_end_of_text_is_not_resolved_even_with_no_next_symbol(
        self,
    ):
        # A zero-size symbol has nothing to cap it except .text's own end (sysprof's
        # elf_parser_lookup_symbol() rejects anything past that outright), unlike a
        # local ELF's fallback ("In File ...") text, which only needs *some* mapping
        # to cover an address and does not stop at .text specifically.
        path = self._write(
            build_minimal_elf([("entry_point", 0x100, 0)], code_size=0x200)
        )

        self.assertIsNone(ElfSymbolizer().resolve(path, 0x100 + 0x200))

    def test_a_missing_file_is_not_resolved(self):
        self.assertIsNone(ElfSymbolizer().resolve("/no/such/file-webkitsysprof", 0x10))

    def test_a_non_elf_file_is_not_resolved(self):
        path = self._write(b"not an ELF file, just some bytes" * 4)

        self.assertIsNone(ElfSymbolizer().resolve(path, 0x10))

    def test_resolution_is_cached_so_a_later_call_survives_the_file_disappearing(self):
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))
        symbolizer = ElfSymbolizer()
        self.assertEqual(_resolve_text(symbolizer, path, 0x100), "my_function")

        Path(path).unlink()

        self.assertEqual(_resolve_text(symbolizer, path, 0x110), "my_function+0x10")

    def test_resolving_the_same_symbol_twice_gives_the_same_identity(self):
        # dump._append_frame() collapses a run of consecutive, identically-resolved
        # frames by comparing identities, so two resolutions of the same underlying
        # symbol (here, two different addresses inside it) must compare equal, and
        # a resolution of a *different* symbol must not.
        path = self._write(
            build_minimal_elf([("my_function", 0x100, 0x50), ("other_fn", 0x200, 0x10)])
        )
        symbolizer = ElfSymbolizer()

        identity_a, _ = symbolizer.resolve(path, 0x100)
        identity_b, _ = symbolizer.resolve(path, 0x110)
        identity_c, _ = symbolizer.resolve(path, 0x200)

        self.assertEqual(identity_a, identity_b)
        self.assertNotEqual(identity_a, identity_c)

    def test_a_symbol_outside_the_text_section_is_ignored(self):
        # Sysprof's own ELF reader (read_table() in contrib/elfparser/elfparser.c)
        # only trusts a symbol whose section is literally ".text"; one that claims
        # to belong to some other section index is not a function this resolves,
        # the same as it would not resolve on sysprof's own reference reader.
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))
        data = bytearray(Path(path).read_bytes())
        _retarget_only_symbol_shndx(data, new_shndx=3)  # .strtab's index, not .text's.
        Path(path).write_bytes(bytes(data))

        self.assertIsNone(ElfSymbolizer().resolve(path, 0x100))

    def test_a_non_function_symbol_is_ignored(self):
        # STT_OBJECT (data) and STT_GNU_IFUNC (an indirect function resolver, which
        # sysprof's own reader also does not special-case) are not STT_FUNC, so
        # neither is trusted as naming a stack frame.
        for symbol_type, label in [(_STT_OBJECT, "object"), (_STT_GNU_IFUNC, "ifunc")]:
            with self.subTest(type=label):
                path = self._write(build_minimal_elf([("my_symbol", 0x100, 0x50)]))
                data = bytearray(Path(path).read_bytes())
                _retarget_only_symbol_type(data, symbol_type)
                Path(path).write_bytes(bytes(data))

                self.assertIsNone(ElfSymbolizer().resolve(path, 0x100))

    def test_an_unusual_symbol_binding_is_ignored(self):
        # STB_GLOBAL is one of the ordinary bindings sysprof's own reader accepts
        # (along with LOCAL and WEAK); anything else (e.g. the GNU-specific
        # STB_GNU_UNIQUE, used for some C++ template instantiations) is not.
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))
        data = bytearray(Path(path).read_bytes())
        _retarget_only_symbol_binding(data, binding=10 << 4)  # STB_GNU_UNIQUE
        Path(path).write_bytes(bytes(data))

        self.assertIsNone(ElfSymbolizer().resolve(path, 0x100))

    def test_an_ordinary_global_binding_is_accepted(self):
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))
        data = bytearray(Path(path).read_bytes())
        _retarget_only_symbol_binding(data, binding=_STB_GLOBAL)
        Path(path).write_bytes(bytes(data))

        self.assertEqual(_resolve_text(ElfSymbolizer(), path, 0x100), "my_function")

    def test_a_file_with_no_text_section_resolves_nothing(self):
        # elf_parser_lookup_symbol() bails out immediately for a file with no
        # literal ".text" section at all, regardless of what its symbol table says.
        path = self._write(build_minimal_elf([("my_function", 0x100, 0x50)]))
        data = bytearray(Path(path).read_bytes())
        _rename_text_section(data)
        Path(path).write_bytes(bytes(data))

        self.assertIsNone(ElfSymbolizer().resolve(path, 0x100))

    def test_a_path_only_reachable_through_a_pid_s_mount_namespace_is_resolved(self):
        # A container's bind mount (e.g. Tools/Scripts/container-sdk-rootdir-wrapper's
        # /sdk/webkit) makes a real host directory show up at a container-internal
        # path; a capture recorded from inside it names WebKit binaries by that
        # container-internal path, which this machine, outside the container, does
        # not have -- but the profiled process's own bundled /proc/<pid>/mountinfo
        # says exactly which real directory that path is a bind mount of (see
        # mounts_unittest.py), so resolve() must find the library through it.
        with tempfile.TemporaryDirectory() as real_root:
            library = str(Path(real_root) / "libWPEWebKit.so")
            Path(library).write_bytes(build_minimal_elf([("my_function", 0x100, 0x50)]))

            translator = MountTranslator(
                host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
                mounts_by_pid={
                    100: [
                        MountInfoEntry("/sdk/webkit", real_root, "ext4", "/dev/sda1", "rw")
                    ]
                },
            )
            symbolizer = ElfSymbolizer(translator)

            container_path = "/sdk/webkit/libWPEWebKit.so"
            resolved = symbolizer.resolve(container_path, 0x100, pid=100)

            self.assertEqual(
                _resolve_text(symbolizer, container_path, 0x100, pid=100), "my_function"
            )
            # Identity stays keyed by the path as given (the one every other frame
            # resolved against the same mapping will also be asked about), not by
            # wherever the bytes were actually read from.
            self.assertEqual(resolved[0][0], container_path)

    def test_a_path_that_exists_as_given_is_not_second_guessed(self):
        # A capture dumped from right where it was recorded already has every path
        # reachable as given, so a translated candidate (which nothing here asked
        # to exist) must never be tried, let alone preferred, ahead of it.
        with tempfile.TemporaryDirectory() as tempdir:
            library = str(Path(tempdir) / "libfoo.so")
            Path(library).write_bytes(build_minimal_elf([("real_function", 0x100, 0x50)]))

            translator = MountTranslator(
                host_mount_devices=[], mounts_by_pid={100: []}
            )
            symbolizer = ElfSymbolizer(translator)

            self.assertEqual(
                _resolve_text(symbolizer, library, 0x100, pid=100), "real_function"
            )

    def test_a_path_no_mount_translates_is_never_resolved(self):
        translator = MountTranslator(host_mount_devices=[], mounts_by_pid={100: []})
        symbolizer = ElfSymbolizer(translator)

        self.assertIsNone(symbolizer.resolve("/sdk/webkit/libfoo.so", 0x100, pid=100))

    def test_resolve_without_a_pid_never_consults_the_mount_translator(self):
        # A caller that has no pid at all (or no translator, the default) gets
        # exactly the old behaviour: only the literal path is ever tried.
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
            mounts_by_pid={100: [MountInfoEntry("/sdk/webkit", "/nonexistent", "ext4", "/dev/sda1", "rw")]},
        )
        symbolizer = ElfSymbolizer(translator)

        self.assertIsNone(symbolizer.resolve("/sdk/webkit/libfoo.so", 0x100))

    @unittest.skipIf(
        _CXA_DEMANGLE is None,
        "libstdc++.so.6's __cxa_demangle is not loadable on this machine (e.g. macOS)",
    )
    def test_a_mangled_cpp_name_is_demangled_like_sysprof_s_own_reader(self):
        # sysprof-elf.c's sysprof_elf_get_symbol_at_address_internal() demangles a
        # symbol starting with the Itanium C++ mangling prefix via the very same
        # abi::__cxa_demangle() this uses, so a WebKit stack should read the same
        # way here as it would in the real Sysprof GUI.
        mangled = "_ZN3WTF14GSocketMonitor20socketSourceCallbackEP8_GSocket12GIOConditionPS0_"
        path = self._write(build_minimal_elf([(mangled, 0x100, 0x50)]))

        self.assertEqual(
            _resolve_text(ElfSymbolizer(), path, 0x100),
            "WTF::GSocketMonitor::socketSourceCallback(_GSocket*, GIOCondition, WTF::GSocketMonitor*)",
        )

    def test_an_unmangled_name_is_left_alone(self):
        path = self._write(build_minimal_elf([("g_main_context_iterate", 0x100, 0x50)]))

        self.assertEqual(
            _resolve_text(ElfSymbolizer(), path, 0x100), "g_main_context_iterate"
        )

    def test_a_name_only_starting_like_a_mangled_one_is_left_alone(self):
        # A name that merely starts with "_Z" but is not valid Itanium mangling is
        # still handed to __cxa_demangle() (matching sysprof-elf.c's own check,
        # which looks only at the first two characters), which rejects it and
        # returns NULL; that must fall back to the original text, not raise or
        # produce something else.
        path = self._write(build_minimal_elf([("_Zzzz_not_actually_mangled", 0x100, 0x50)]))

        self.assertEqual(
            _resolve_text(ElfSymbolizer(), path, 0x100), "_Zzzz_not_actually_mangled"
        )


def _find_only_symbol_offset(data: bytearray) -> int:
    """The file offset of build_minimal_elf()'s single non-null .symtab entry, by
    reading the section headers it wrote (rather than hardcoding layout offsets
    that would silently go stale if the builder's layout ever changes).
    """
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    (e_shnum,) = struct.unpack_from("<H", data, 0x3C)
    for index in range(e_shnum):
        entry = e_shoff + index * 64
        sh_type = struct.unpack_from("<I", data, entry + 4)[0]
        if sh_type == 2:  # SHT_SYMTAB
            (sh_offset,) = struct.unpack_from("<Q", data, entry + 24)
            return sh_offset + 24  # Entry 1 (index 0 is the null symbol), 24 bytes in.
    raise AssertionError("no .symtab section found")


def _retarget_only_symbol_shndx(data: bytearray, new_shndx: int) -> None:
    offset = _find_only_symbol_offset(data)
    struct.pack_into("<H", data, offset + 6, new_shndx)  # st_shndx is at byte 6.


def _retarget_only_symbol_type(data: bytearray, new_type: int) -> None:
    offset = _find_only_symbol_offset(data)
    (st_info,) = struct.unpack_from("<B", data, offset + 4)
    struct.pack_into("<B", data, offset + 4, (st_info & 0xF0) | new_type)


def _retarget_only_symbol_binding(data: bytearray, binding: int) -> None:
    offset = _find_only_symbol_offset(data)
    (st_info,) = struct.unpack_from("<B", data, offset + 4)
    struct.pack_into("<B", data, offset + 4, (st_info & 0x0F) | binding)


def _rename_text_section(data: bytearray) -> None:
    # Overwrites the ".shstrtab" bytes spelling out ".text" with a same-length name
    # that the parser will not recognize, so every other offset in the file (which
    # this test otherwise leaves untouched) stays valid.
    index = bytes(data).find(b".text\x00")
    assert index != -1
    data[index: index + 5] = b".teXt"


if __name__ == "__main__":
    unittest.main()
