"""Translates a path from a profiled process's mount namespace (e.g. a container
bind mount like `/sdk/webkit`) to a path on this machine.

Port of libsysprof's SysprofMountNamespace (sysprof-mount-namespace.c,
sysprof-mount.c) limited to bind mounts. Overlay translation is left out, since an
image-only path has no host file to translate to anyway. Inputs are the capture's
bundled `/proc/mounts` and each process's `/proc/<pid>/mountinfo`.
"""

from typing import Dict, List, NamedTuple, Optional, Sequence


class MountDevice(NamedTuple):
    """One line of /proc/mounts: where a filesystem is mounted on this machine."""

    fs_spec: str
    mount_point: str
    subvolume: Optional[str]


class MountInfoEntry(NamedTuple):
    """One line of some process's own /proc/<pid>/mountinfo."""

    mount_point: str
    root: str
    filesystem_type: Optional[str]
    mount_source: Optional[str]
    superblock_options: Optional[str]


def parse_proc_mounts(text: str) -> List[MountDevice]:
    """Parses a bundled /proc/mounts, keeping file order. As in
    sysprof_mount_namespace_find_device(), the first entry for an fs_spec wins.
    """
    devices = []
    for line in text.splitlines():
        # Mirrors g_strsplit(line, " ", 5).
        parts = line.split(" ", 4)
        if len(parts) != 5:
            continue
        fs_spec, mount_point, filesystem_type, options = parts[:4]
        subvolume = None
        if filesystem_type == "btrfs":
            for option in options.split(","):
                if option.startswith("subvol="):
                    subvolume = option.split("=", 1)[1]
                    break
        devices.append(MountDevice(fs_spec, mount_point, subvolume))
    return devices


def parse_mountinfo(text: str) -> List[MountInfoEntry]:
    """Parses a /proc/<pid>/mountinfo, keeping file order.

    Format (proc(5)): mount_id parent_id major:minor root mount_point options
    [optional fields...] "-" fstype mount_source super_options. The number of
    optional fields varies, so the "-" is searched for.
    """
    entries = []
    for line in text.splitlines():
        parts = line.split(" ", 19)  # mirrors g_strsplit(line, " ", 20).
        if len(parts) < 10:
            continue

        root, mount_point = parts[3], parts[4]

        separator = None
        for index in range(5, len(parts)):
            if parts[index] == "-":
                separator = index
                break
        if separator is None or separator + 3 >= len(parts):
            continue

        filesystem_type = parts[separator + 1]
        mount_source = parts[separator + 2]
        superblock_options = parts[separator + 3]
        entries.append(
            MountInfoEntry(
                mount_point, root, filesystem_type, mount_source, superblock_options
            )
        )
    return entries


def _superblock_option(options: Optional[str], key: str) -> Optional[str]:
    if not options:
        return None
    for option in options.split(","):
        if option == key:
            return ""
        if option.startswith(key + "="):
            return option.split("=", 1)[1]
    return None


def _relative_path(mount: MountInfoEntry, path: str) -> Optional[str]:
    if mount.mount_point == "/":
        return path
    if path.startswith(mount.mount_point + "/"):
        start = len(mount.mount_point)
        return path[start:]
    return None


def _find_device(
    devices: Sequence[MountDevice], mount: MountInfoEntry, subvolume: Optional[str]
) -> Optional[MountDevice]:
    for device in devices:
        if device.fs_spec != mount.mount_source:
            continue
        if subvolume is not None:
            if device.subvolume != subvolume:
                continue
            # Sysprof skips this for Silverblue/GNOME OS style systems.
            if device.mount_point == "/sysroot":
                continue
        return device
    return None


class MountTranslator:
    """Port of sysprof_mount_namespace_translate()'s non-overlay branch. Returns
    candidate host paths, most specific mount first, or an empty list if nothing
    translates.
    """

    def __init__(
        self,
        host_mount_devices: Sequence[MountDevice],
        mounts_by_pid: Dict[int, Sequence[MountInfoEntry]],
    ) -> None:
        self._host_devices = host_mount_devices
        self._mounts_by_pid = mounts_by_pid

    def translate(self, pid: int, path: str) -> List[str]:
        mounts = self._mounts_by_pid.get(pid)
        if not mounts:
            return []

        candidates = []
        for mount in sorted(mounts, key=lambda entry: -len(entry.mount_point)):
            relative = _relative_path(mount, path)
            if relative is None:
                continue

            subvolume = _superblock_option(mount.superblock_options, "subvol")
            device = _find_device(self._host_devices, mount, subvolume)
            if device is None:
                continue

            root = mount.root
            if subvolume is not None:
                if root == subvolume:
                    root = "/"
                elif root.startswith(subvolume + "/"):
                    start = len(subvolume)
                    root = root[start:]

            candidates.append(
                _join_mount_relative_paths(device.mount_point, root, relative)
            )
        return candidates


def _join_mount_relative_paths(*parts: str) -> str:
    """Like g_build_filename(): exactly one "/" between segments."""
    result = parts[0] or "/"
    for part in parts[1:]:
        result = result.rstrip("/") + "/" + part.lstrip("/")
    return result
