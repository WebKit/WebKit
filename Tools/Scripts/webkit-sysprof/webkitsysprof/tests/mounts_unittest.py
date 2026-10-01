"""Fine-grained tests for webkitsysprof.mounts: the /proc/mounts and
/proc/<pid>/mountinfo line parsers, and the translation of a path recorded inside
some pid's own mount namespace back into a real path on this machine (a Python port
of libsysprof's own SysprofMountNamespace, restricted to ordinary bind mounts --
see the module docstring for what is deliberately left out and why).
"""

import unittest

from webkitsysprof.mounts import (
    MountDevice,
    MountInfoEntry,
    MountTranslator,
    parse_mountinfo,
    parse_proc_mounts,
)


class ParseProcMountsTest(unittest.TestCase):
    def test_an_ordinary_line_is_parsed(self):
        self.assertEqual(
            parse_proc_mounts("/dev/sda1 / ext4 rw,relatime 0 0\n"),
            [MountDevice("/dev/sda1", "/", None)],
        )

    def test_a_btrfs_line_s_subvolume_is_extracted_from_its_options(self):
        # Only btrfs carries a subvolume; sysprof_document_load_mounts() only ever
        # looks for "subvol=" in a btrfs line's own options column.
        self.assertEqual(
            parse_proc_mounts(
                "/dev/sda1 / btrfs rw,relatime,subvol=/@root,compress=zstd 0 0\n"
            ),
            [MountDevice("/dev/sda1", "/", "/@root")],
        )

    def test_a_non_btrfs_line_never_gets_a_subvolume_even_with_a_look_alike_option(self):
        self.assertEqual(
            parse_proc_mounts("/dev/sda1 / ext4 rw,subvol=/should-not-count 0 0\n"),
            [MountDevice("/dev/sda1", "/", None)],
        )

    def test_several_lines_are_kept_in_file_order(self):
        text = (
            "/dev/mapper/vg-lv / ext4 rw,relatime 0 0\n"
            "/dev/mapper/vg-lv /var/snap/firefox/common/host-hunspell ext4 ro,noexec 0 0\n"
        )
        self.assertEqual(
            parse_proc_mounts(text),
            [
                MountDevice("/dev/mapper/vg-lv", "/", None),
                MountDevice(
                    "/dev/mapper/vg-lv", "/var/snap/firefox/common/host-hunspell", None
                ),
            ],
        )

    def test_a_line_with_too_few_fields_is_skipped(self):
        self.assertEqual(parse_proc_mounts("/dev/sda1 / ext4\n"), [])

    def test_an_empty_file_has_no_devices(self):
        self.assertEqual(parse_proc_mounts(""), [])


class ParseMountinfoTest(unittest.TestCase):
    def test_an_ordinary_bind_mount_line_is_parsed(self):
        # Real line from a container-sdk-rootdir-wrapper capture: root names the
        # real host directory /sdk/webkit was bind-mounted from.
        line = (
            "20849 20805 252:1 /home/user/webkit/WebKitBuild/lib "
            "/sdk/webkit/WebKitBuild/lib ro,nosuid,nodev,relatime "
            "- ext4 /dev/mapper/ubuntu--vg-ubuntu--lv rw\n"
        )

        self.assertEqual(
            parse_mountinfo(line),
            [
                MountInfoEntry(
                    "/sdk/webkit/WebKitBuild/lib",
                    "/home/user/webkit/WebKitBuild/lib",
                    "ext4",
                    "/dev/mapper/ubuntu--vg-ubuntu--lv",
                    "rw",
                )
            ],
        )

    def test_optional_fields_before_the_separator_are_skipped_over(self):
        # proc(5): an arbitrary number of "tag[:value]" optional fields can appear
        # between the mount options and the "-" separator; the columns after it
        # (filesystem type, mount source, super options) must still land right.
        line = "36 35 98:0 / /mnt1 rw,noatime master:1 shared:2 - ext3 /dev/root rw,errors=continue\n"

        self.assertEqual(
            parse_mountinfo(line),
            [MountInfoEntry("/mnt1", "/", "ext3", "/dev/root", "rw,errors=continue")],
        )

    def test_a_line_with_enough_fields_but_no_separator_is_skipped(self):
        # 10 whitespace-separated fields (the bare minimum this checks for at all),
        # but none of them is the "-" that proc(5) says always follows the
        # optional fields -- a malformed line this parser must not misread.
        line = "36 35 98:0 / /mnt1 rw,noatime master:1 shared:2 unbindable:3 propagate:4\n"

        self.assertEqual(parse_mountinfo(line), [])

    def test_a_line_with_too_few_fields_is_skipped(self):
        self.assertEqual(parse_mountinfo("36 35 98:0 / /mnt1\n"), [])

    def test_several_lines_are_kept_in_file_order(self):
        text = (
            "20849 20805 252:1 /a /sdk/webkit/lib ro - ext4 /dev/sda1 rw\n"
            "20881 20805 252:1 /b /sdk/webkit ro - ext4 /dev/sda1 rw\n"
        )

        self.assertEqual(
            [entry.mount_point for entry in parse_mountinfo(text)],
            ["/sdk/webkit/lib", "/sdk/webkit"],
        )


class MountTranslatorTest(unittest.TestCase):
    def test_a_bind_mounted_path_translates_to_its_real_directory(self):
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
            mounts_by_pid={
                100: [
                    MountInfoEntry(
                        "/sdk/webkit", "/home/user/webkit", "ext4", "/dev/sda1", "rw"
                    )
                ]
            },
        )

        self.assertEqual(
            translator.translate(100, "/sdk/webkit/WebKitBuild/lib/libfoo.so"),
            ["/home/user/webkit/WebKitBuild/lib/libfoo.so"],
        )

    def test_the_most_specific_mount_point_is_tried_first(self):
        # Both the root mount and the more specific /sdk/webkit bind mount cover
        # the same path; the longer (more specific) mount point must come first,
        # since it is the one actually meant to answer for that subtree.
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
            mounts_by_pid={
                100: [
                    MountInfoEntry("/", "/", "ext4", "/dev/sda1", "rw"),
                    MountInfoEntry(
                        "/sdk/webkit", "/home/user/webkit", "ext4", "/dev/sda1", "rw"
                    ),
                ]
            },
        )

        self.assertEqual(
            translator.translate(100, "/sdk/webkit/libfoo.so"),
            [
                "/home/user/webkit/libfoo.so",
                # The root mount also matches (everything is under "/"), giving the
                # path back unchanged as a second, less specific candidate.
                "/sdk/webkit/libfoo.so",
            ],
        )

    def test_a_path_outside_every_mount_point_has_no_candidates(self):
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
            mounts_by_pid={
                100: [
                    MountInfoEntry(
                        "/sdk/webkit", "/home/user/webkit", "ext4", "/dev/sda1", "rw"
                    )
                ]
            },
        )

        self.assertEqual(translator.translate(100, "/usr/lib/libc.so.6"), [])

    def test_a_mount_whose_device_is_not_one_of_this_machine_s_own_has_no_candidate(self):
        # e.g. a path that only ever existed inside a container image layer, with
        # no bind mount back to a real host directory: this machine's own
        # /proc/mounts has nothing on that device, so there is nothing to
        # translate it to (matches the ordinary "not resolved" fallback exactly).
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", None)],
            mounts_by_pid={
                100: [
                    MountInfoEntry(
                        "/jhbuild/install", "/", "overlay", "overlay", "rw"
                    )
                ]
            },
        )

        self.assertEqual(translator.translate(100, "/jhbuild/install/lib/libc.so"), [])

    def test_an_unknown_pid_has_no_candidates(self):
        translator = MountTranslator(host_mount_devices=[], mounts_by_pid={})

        self.assertEqual(translator.translate(999, "/sdk/webkit/libfoo.so"), [])

    def test_a_btrfs_subvolume_is_folded_out_of_the_translated_path(self):
        # The mount's own root is relative to the whole filesystem, including
        # whichever subvolume it was mounted from; once the host device is known
        # to be mounted from that same subvolume, the subvolume prefix is not part
        # of the real path and must be stripped back out.
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", "/@root")],
            mounts_by_pid={
                100: [
                    MountInfoEntry(
                        "/sdk/webkit",
                        "/@root/home/user/webkit",
                        "btrfs",
                        "/dev/sda1",
                        "rw,subvol=/@root",
                    )
                ]
            },
        )

        self.assertEqual(
            translator.translate(100, "/sdk/webkit/libfoo.so"),
            ["/home/user/webkit/libfoo.so"],
        )

    def test_a_subvolume_mismatch_is_not_treated_as_the_same_device(self):
        translator = MountTranslator(
            host_mount_devices=[MountDevice("/dev/sda1", "/", "/@other")],
            mounts_by_pid={
                100: [
                    MountInfoEntry(
                        "/sdk/webkit",
                        "/@root/home/user/webkit",
                        "btrfs",
                        "/dev/sda1",
                        "rw,subvol=/@root",
                    )
                ]
            },
        )

        self.assertEqual(translator.translate(100, "/sdk/webkit/libfoo.so"), [])


if __name__ == "__main__":
    unittest.main()
