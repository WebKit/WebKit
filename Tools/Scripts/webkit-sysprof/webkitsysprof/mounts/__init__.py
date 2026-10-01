"""Translates a path recorded inside a profiled process's own mount namespace
(e.g. a container's bind-mounted view of the world) back into a path reachable from
wherever this tool is running now.

This is a Python port of libsysprof's own SysprofMountNamespace (see
sysprof-mount-namespace.c and sysprof-mount.c), restricted to the case that
actually arises for a WebKit capture: an ordinary bind mount (such as
Tools/Scripts/container-sdk-rootdir-wrapper's `/sdk/webkit`, or any other bind
mount a container puts a real host directory at) of a filesystem this machine
still has mounted somewhere. It deliberately leaves out overlayfs- and
OVERLAY-frame-based translation (sysprof's `is_overlay` mounts and its
`fs_type == "overlay"` branch): a path that only exists inside a container image
layer, with no bind mount back to a real host directory, has nothing on this
machine to translate to either way, so those cases fall through to "no
candidates" here exactly as they would if this simply hadn't been implemented.

The data this needs is the capture's own bundled copy of `/proc/mounts` (this
machine's real mount table, as sysprof-cli itself saw it while recording -- see
`parser.direct_parser`'s decoding of that file) and, per profiled process, that
process's own `/proc/<pid>/mountinfo` (its view from inside whatever mount
namespace it was running in). Both are ordinary things sysprof-cli bundles into
every capture; nothing here is specific to any one container convention.
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
    """The (fs_spec, mount_point, subvolume) of every line of a bundled
    /proc/mounts, in file order -- the same order `find_device` below trusts when
    more than one entry shares an fs_spec (e.g. the same filesystem bind-mounted
    at two places): the earlier line wins, exactly as libsysprof's own
    sysprof_mount_namespace_find_device() does by returning its first match.
    """
    devices = []
    for line in text.splitlines():
        # mirrors g_strsplit(line, " ", 5): device, mount_point, fstype, options,
        # and everything else (the dump/pass columns /proc/mounts also carries)
        # folded into one last field neither of them needs.
        parts = line.split(" ", 4)
        if len(parts) != 5:
            continue
        fs_spec, mount_point, filesystem_type, options = parts[0], parts[1], parts[2], parts[3]
        subvolume = None
        if filesystem_type == "btrfs":
            for option in options.split(","):
                if option.startswith("subvol="):
                    subvolume = option[len("subvol="):]
                    break
        devices.append(MountDevice(fs_spec, mount_point, subvolume))
    return devices


def parse_mountinfo(text: str) -> List[MountInfoEntry]:
    """Every line of one process's own /proc/<pid>/mountinfo, in file order.

    See proc(5) for the format: mount_id parent_id major:minor root mount_point
    options [optional fields...] "-" filesystem_type mount_source super_options.
    The optional fields are of unbounded number, so where the "-" separator
    actually falls has to be found rather than assumed at a fixed index.
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
            MountInfoEntry(mount_point, root, filesystem_type, mount_source, superblock_options)
        )
    return entries


def _superblock_option(options: Optional[str], key: str) -> Optional[str]:
    if not options:
        return None
    for option in options.split(","):
        if option == key:
            return ""
        if option.startswith(key + "="):
            return option[len(key) + 1:]
    return None


def _relative_path(mount: MountInfoEntry, path: str) -> Optional[str]:
    if mount.mount_point == "/":
        return path
    if path.startswith(mount.mount_point + "/"):
        return path[len(mount.mount_point):]
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
            # Sysprof's own convention for e.g. Silverblue/GNOME OS style systems.
            if device.mount_point == "/sysroot":
                continue
        return device
    return None


class MountTranslator:
    """Ports sysprof_mount_namespace_translate()'s non-overlay branch: candidate
    real paths for a path recorded inside some pid's own mount namespace, longest
    (most specific) matching mount first, the same order a caller should try them
    in. An empty list means nothing here could translate it -- not that the
    original path is wrong, just that this translator has nothing to say about it
    (e.g. no mountinfo was ever bundled for that pid, or the path names something
    that only ever existed inside a container image with no real host backing).
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
                    root = root[len(subvolume):]

            candidates.append(
                _join_mount_relative_paths(device.mount_point, root, relative)
            )
        return candidates


def _join_mount_relative_paths(*parts: str) -> str:
    """g_build_filename(device_mount_point, root, relative, NULL): joins path
    segments with exactly one "/" between them, regardless of which ones already
    carry a leading or trailing one.
    """
    result = parts[0] or "/"
    for part in parts[1:]:
        result = result.rstrip("/") + "/" + part.lstrip("/")
    return result
