"""Fine-grained tests for the .syscap frame parsing direct_parser does.

Builds the smallest capture files that exercise SAMPLE, MAP and PROCESS frames byte
by byte, so a change to the frame layout (say, a field width) is caught here rather
than only showing up as a subtly wrong flamegraph downstream.
"""

import gzip
import struct
import tempfile
import unittest
from pathlib import Path

from webkitsysprof import parser
from webkitsysprof.parser import direct_parser

_MAGIC = 0xFDCA975E
_FILE_HEADER_SIZE = 256
_FRAME_ALIGN = 8


def _frame_header(frame_len, frame_type, pid=0, time=0, cpu=0) -> bytes:
    return struct.pack("<HhiqII", frame_len, cpu, pid, time, frame_type, 0)


def _build_frame(frame_type, body: bytes, pid=0, time=0) -> bytes:
    # The frame's length field must count the alignment padding too, since that is
    # what a reader advances by to find the next frame; padding on the outside of an
    # already-declared length would desync every frame after this one.
    total_len = 24 + len(body)
    remainder = total_len % _FRAME_ALIGN
    if remainder:
        padding = _FRAME_ALIGN - remainder
        body += b"\x00" * padding
        total_len += padding
    return _frame_header(total_len, frame_type, pid, time) + body


def _sample_frame(pid, tid, time, addresses) -> bytes:
    body = struct.pack("<Ii", len(addresses), tid) + b"".join(
        struct.pack("<Q", address) for address in addresses
    )
    return _build_frame(direct_parser._FRAME_SAMPLE, body, pid, time)


def _map_frame(pid, start, end, offset, inode, filename) -> bytes:
    body = struct.pack("<QQQQ", start, end, offset, inode) + filename.encode() + b"\x00"
    return _build_frame(direct_parser._FRAME_MAP, body, pid)


def _process_frame(pid, cmdline) -> bytes:
    body = cmdline.encode() + b"\x00"
    return _build_frame(direct_parser._FRAME_PROCESS, body, pid)


def _file_chunk_frame(path, chunk_data, is_last=True) -> bytes:
    is_last_and_len = (1 if is_last else 0) | (len(chunk_data) << 16)
    body = (
        struct.pack("<I", is_last_and_len)
        + path.encode().ljust(256, b"\x00")
        + chunk_data
    )
    return _build_frame(direct_parser._FRAME_FILE_CHUNK, body)


def _packed_symbols_blob(entries) -> bytes:
    """The uncompressed layout __symbols__(.gz) decodes to: entries as (addr_begin,
    addr_end, pid, name) or (addr_begin, addr_end, pid, name, tag) turned into
    fixed-size SysprofPackedSymbol records (see direct_parser._parse_packed_symbols),
    terminated by one all-zero record, followed by the strings the records' name and
    tag offsets point into. A 4-tuple entry gets no tag_offset at all (0), the same
    as sysprof gives a symbol with no nick.
    """
    packed_len = (len(entries) + 1) * direct_parser._PACKED_SYMBOL_SIZE
    strings = b"\x00"  # Offset 0 is conventionally the empty string.
    records = b""
    for entry in entries:
        addr_begin, addr_end, pid, name = entry[:4]
        tag = entry[4] if len(entry) > 4 else None
        name_offset = packed_len + len(strings)
        strings += name.encode() + b"\x00"
        if tag is not None:
            tag_offset = packed_len + len(strings)
            strings += tag.encode() + b"\x00"
        else:
            tag_offset = 0
        records += struct.pack(
            "<QQIIII", addr_begin, addr_end, pid, name_offset, tag_offset, 0
        )
    records += struct.pack("<QQIIII", 0, 0, 0, 0, 0, 0)  # Sentinel.
    return records + strings


def _capture_bytes(frames: bytes, header_time=0, header_end_time=0) -> bytes:
    header = bytearray(_FILE_HEADER_SIZE)
    struct.pack_into("<II", header, 0, _MAGIC, 1 << 8)
    struct.pack_into("<qq", header, 72, header_time, header_end_time)
    return bytes(header) + frames


class CaptureFile:
    """A .syscap built from raw frame bytes, written to a temp file for parse()."""

    def __init__(self, frames: bytes = b"", header_time=0, header_end_time=0):
        self._tempdir = tempfile.TemporaryDirectory()
        self.path = str(Path(self._tempdir.name) / "test.syscap")
        Path(self.path).write_bytes(
            _capture_bytes(frames, header_time, header_end_time)
        )

    def close(self):
        self._tempdir.cleanup()


class ParserTest(unittest.TestCase):
    def _build(self, frames: bytes, **kwargs) -> CaptureFile:
        capture = CaptureFile(frames, **kwargs)
        self.addCleanup(capture.close)
        return capture

    def test_sample_frames_capture_pid_tid_time_and_the_raw_addresses(self):
        frames = _sample_frame(100, 200, 5, [0x1050, 0x9999])
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["stacktraces"],
            [{"pid": 100, "tid": 200, "time": 5, "addresses": [0x1050, 0x9999]}],
        )

    def test_sample_frame_with_no_addresses_is_captured_as_an_empty_stack(self):
        frames = _sample_frame(1, 1, 0, [])
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["stacktraces"], [{"pid": 1, "tid": 1, "time": 0, "addresses": []}]
        )

    def test_map_frames_are_captured_per_pid_in_frame_order(self):
        frames = _map_frame(100, 0x1000, 0x2000, 0, 42, "/usr/lib/libfoo.so")
        frames += _map_frame(100, 0x2000, 0x3000, 0x1000, 43, "/usr/lib/libbar.so")
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["maps"][100],
            [
                {
                    "start": 0x1000,
                    "end": 0x2000,
                    "offset": 0,
                    "inode": 42,
                    "filename": "/usr/lib/libfoo.so",
                },
                {
                    "start": 0x2000,
                    "end": 0x3000,
                    "offset": 0x1000,
                    "inode": 43,
                    "filename": "/usr/lib/libbar.so",
                },
            ],
        )

    def test_process_frames_capture_cmdline_by_pid(self):
        frames = _process_frame(100, "/usr/bin/wpe-bare-app --headless")
        frames += _process_frame(200, "kthreadd")
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["processes"],
            {100: "/usr/bin/wpe-bare-app --headless", 200: "kthreadd"},
        )

    def test_a_later_process_frame_for_the_same_pid_replaces_the_cmdline(self):
        # Frames are walked in time order, so a pid re-exec'ing (or simply reused)
        # should be read as whatever it was doing most recently.
        frames = _process_frame(100, "/usr/bin/old-name")
        frames += _process_frame(100, "/usr/bin/new-name")
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["processes"], {100: "/usr/bin/new-name"})

    def test_stacktrace_data_is_empty_when_not_requested(self):
        frames = _sample_frame(1, 1, 0, [0x10])
        frames += _map_frame(1, 0, 0x100, 0, 0, "/lib/x.so")
        frames += _process_frame(1, "/bin/x")
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=True, stacktraces=False
        )

        self.assertEqual(result["stacktraces"], [])
        self.assertEqual(result["maps"], {})
        self.assertEqual(result["processes"], {})

    def test_stacktrace_times_are_made_relative_to_the_capture_start(self):
        frames = _sample_frame(1, 1, 1500, [0x10])
        capture = self._build(frames, header_time=1000, header_end_time=2000)

        result = parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["stacktraces"][0]["time"], 500)

    def test_bundled_symbols_are_decoded_from_the_gzip_compressed_symbols_file(self):
        # This is what makes a capture symbolizable on its own: sysprof resolves
        # every sample at record time and bundles the result under this name, so
        # dump needs neither the original binaries nor to do any resolving itself.
        blob = _packed_symbols_blob(
            [
                (0x1000, 0x1050, 100, "my_function"),
                (0x2000, 0x2100, 100, "other_function"),
            ]
        )
        frames = _file_chunk_frame("__symbols__.gz", gzip.compress(blob))
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["symbols"][100],
            [(0x1000, 0x1050, "my_function"), (0x2000, 0x2100, "other_function")],
        )

    def test_bundled_symbols_keep_their_nick_alongside_the_name(self):
        # sysprof carries a symbol's "nick" (which library it belongs to, e.g. libc
        # or GLib; see sysprof_bundled_symbolizer_symbolize()'s use of tag_offset)
        # separately from its name; folding it in as "name (nick)" here is what
        # keeps that information rather than silently dropping it. A symbol with no
        # nick at all (tag_offset 0, as sysprof itself writes for one) is
        # unaffected.
        blob = _packed_symbols_blob(
            [
                (0x1000, 0x1050, 100, "fork_exec", "GLib"),
                (0x2000, 0x2100, 100, "my_function"),
            ]
        )
        frames = _file_chunk_frame("__symbols__.gz", gzip.compress(blob))
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["symbols"][100],
            [(0x1000, 0x1050, "fork_exec (GLib)"), (0x2000, 0x2100, "my_function")],
        )

    def test_bundled_symbols_are_reassembled_from_several_file_chunks(self):
        blob = _packed_symbols_blob([(0x1000, 0x1050, 100, "my_function")])
        compressed = gzip.compress(blob)
        midpoint = len(compressed) // 2
        frames = _file_chunk_frame(
            "__symbols__.gz", compressed[:midpoint], is_last=False
        )
        frames += _file_chunk_frame(
            "__symbols__.gz", compressed[midpoint:], is_last=True
        )
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["symbols"][100], [(0x1000, 0x1050, "my_function")])

    def test_bundled_symbols_support_the_uncompressed_fallback_name(self):
        blob = _packed_symbols_blob([(0x1000, 0x1050, 100, "my_function")])
        frames = _file_chunk_frame("__symbols__", blob)
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["symbols"][100], [(0x1000, 0x1050, "my_function")])

    def test_bundled_symbols_are_empty_when_the_compressed_data_is_corrupt(self):
        # A capture cut off mid-write, or one from a future format this cannot read,
        # should leave dump with nothing bundled rather than crash the whole parse.
        frames = _file_chunk_frame("__symbols__.gz", b"not actually gzip data")
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["symbols"], {})

    def test_symbols_are_empty_when_stacktraces_are_not_requested(self):
        blob = _packed_symbols_blob([(0x1000, 0x1050, 100, "my_function")])
        frames = _file_chunk_frame("__symbols__.gz", gzip.compress(blob))
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=True, counters=False, stacktraces=False
        )

        self.assertEqual(result["symbols"], {})

    def test_kernel_symbols_are_decoded_from_the_bundled_kallsyms_and_sorted(self):
        # Unlike the userspace "__symbols__" bundle, sysprof does not resolve kernel
        # frames itself; it only embeds a copy of /proc/kallsyms (see
        # sysprof-kallsyms-symbolizer.c), so dump has to decode this the same way
        # that reader does: one "<address> <type> <name>" line per symbol.
        kallsyms = "ffff800080013a20 t gic_handle_irq\nffff8000805e7240 T drm_ioctl\n"
        frames = _file_chunk_frame(
            "/proc/kallsyms.gz", gzip.compress(kallsyms.encode())
        )
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["kernel_symbols"],
            [(0xFFFF800080013A20, "gic_handle_irq"), (0xFFFF8000805E7240, "drm_ioctl")],
        )

    def test_kernel_symbols_keep_only_the_first_of_several_aliases_at_one_address(self):
        # kallsyms often lists more than one name for the same address back to back
        # (a function and the section label it starts, say); sysprof itself keeps
        # only the first one it sees there, and so does this.
        kallsyms = (
            "ffff800080010000 t gic_handle_irq\n"
            "ffff800080010000 T _stext\n"
            "ffff800080010000 t __pi__stext\n"
        )
        frames = _file_chunk_frame("/proc/kallsyms", kallsyms.encode())
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["kernel_symbols"], [(0xFFFF800080010000, "gic_handle_irq")]
        )

    def test_kernel_symbols_skip_lines_that_do_not_parse(self):
        kallsyms = "not a kallsyms line at all\nffff800080013a20 t gic_handle_irq\n\n"
        frames = _file_chunk_frame("/proc/kallsyms", kallsyms.encode())
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["kernel_symbols"], [(0xFFFF800080013A20, "gic_handle_irq")]
        )

    def test_kernel_symbols_are_empty_without_a_bundled_kallsyms(self):
        capture = self._build(_sample_frame(1, 1, 0, [0x10]))

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["kernel_symbols"], [])

    def test_host_mount_devices_are_decoded_from_the_bundled_proc_mounts(self):
        # sysprof-cli's own /proc/mounts, bundled once per capture (see
        # webkitsysprof.mounts.parse_proc_mounts): the device column and mount
        # point matter for translating a container path back to a real one; the
        # trailing dump/pass columns (the "0 0" here) do not.
        mounts = "/dev/sda1 / ext4 rw,relatime 0 0\n"
        frames = _file_chunk_frame("/proc/mounts.gz", gzip.compress(mounts.encode()))
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["host_mount_devices"], [("/dev/sda1", "/", None)]
        )

    def test_host_mount_devices_are_empty_without_a_bundled_proc_mounts(self):
        capture = self._build(_sample_frame(1, 1, 0, [0x10]))

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["host_mount_devices"], [])

    def test_mounts_by_pid_are_decoded_from_each_pid_s_own_bundled_mountinfo(self):
        # A container's bind mount (e.g. Tools/Scripts/container-sdk-rootdir-wrapper's
        # /sdk/webkit) shows up in the profiled process's own /proc/<pid>/mountinfo
        # with its "root" field naming the real directory it was bind-mounted from;
        # webkitsysprof.mounts.MountTranslator uses exactly this to resolve a WebKit
        # binary recorded at a container-internal path.
        mountinfo = (
            "20849 20805 252:1 /home/user/webkit/WebKitBuild/lib "
            "/sdk/webkit/WebKitBuild/lib ro,relatime "
            "- ext4 /dev/mapper/vg-lv rw\n"
        )
        frames = _file_chunk_frame(
            "/proc/577820/mountinfo.gz", gzip.compress(mountinfo.encode())
        )
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["mounts_by_pid"],
            {
                577820: [
                    (
                        "/sdk/webkit/WebKitBuild/lib",
                        "/home/user/webkit/WebKitBuild/lib",
                        "ext4",
                        "/dev/mapper/vg-lv",
                        "rw",
                    )
                ]
            },
        )

    def test_mounts_by_pid_supports_the_uncompressed_fallback_name(self):
        mountinfo = "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n"
        frames = _file_chunk_frame("/proc/42/mountinfo", mountinfo.encode())
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(
            result["mounts_by_pid"], {42: [("/", "/", "ext4", "/dev/sda1", "rw")]}
        )

    def test_mounts_by_pid_is_empty_without_any_bundled_mountinfo(self):
        capture = self._build(_sample_frame(1, 1, 0, [0x10]))

        result = direct_parser.parse(
            capture.path, marks=False, counters=False, stacktraces=True
        )

        self.assertEqual(result["mounts_by_pid"], {})

    def test_a_mountinfo_chunk_is_ignored_when_stacktraces_are_not_requested(self):
        mountinfo = "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n"
        frames = _file_chunk_frame("/proc/42/mountinfo", mountinfo.encode())
        capture = self._build(frames)

        result = direct_parser.parse(
            capture.path, marks=True, counters=False, stacktraces=False
        )

        self.assertEqual(result["mounts_by_pid"], {})
        self.assertEqual(result["host_mount_devices"], [])


if __name__ == "__main__":
    unittest.main()
