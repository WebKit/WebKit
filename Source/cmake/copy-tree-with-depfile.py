#!/usr/bin/env python3
"""Copy a directory tree and emit a Ninja/Make depfile for it.

Works by breaking the tree down into explicit mtime edges: the depfile lists
every file *and every directory* under the source, which covers the three ways
a tree can change.

  * a file is edited    -> that file's mtime changes
  * a file is deleted   -> a listed dep goes missing, which ninja treats as dirty
  * an entry is added
    or removed          -> the containing directory's mtime changes

With --dest, the depfile also lists each copied file at its destination, so
deleting or editing the copy re-runs it too. Copies keep their source's mtime,
so a fresh copy never looks newer than the stamp.

Omit --dest to only generate the depfile. Useful when a separate build tool
performs the copy with custom semantics, like compiling an asset catalog.

From CMake, use WEBKIT_COPY_DIRECTORY or webkit_get_directory_depfile_command in
WebKitMacros.cmake rather than invoking this script directly.

Caveats:

  * Invalidation is mtime-based, so benign changes to the directory will re-run
    the copy. Only files whose size or mtime differ from their source are
    rewritten, though.
  * If another build step writes into the source tree, you need to add an
    order-only edge from this command to that step to prevent reruns.
"""

import argparse
import fnmatch
import os
import shutil
import sys
import time


def escape(path):
    return path.replace("$", "$$").replace(" ", "\\ ").replace("#", "\\#")


def is_excluded(relpath, patterns):
    return any(fnmatch.fnmatch(relpath, pattern) for pattern in patterns)


def walk(source, excludes):
    """Yield (relpath, is_dir) for every entry under `source`, parents first.

    Symlinks are yielded but never traversed, so a symlink to a directory comes
    back with is_dir False. Excluded directories are pruned along with their
    whole subtree.
    """
    for dirpath, dirnames, filenames in os.walk(source, followlinks=False):
        kept_dirs = []
        for name in sorted(dirnames):
            full = os.path.join(dirpath, name)
            relpath = os.path.relpath(full, source)
            if is_excluded(relpath, excludes):
                continue
            if os.path.islink(full):
                yield relpath, False
                continue
            kept_dirs.append(name)
            yield relpath, True
        dirnames[:] = kept_dirs

        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            relpath = os.path.relpath(full, source)
            if not is_excluded(relpath, excludes):
                yield relpath, False


def collect_deps(source, excludes):
    """Return every file and directory under `source`, including `source` itself.

    Dangling symlinks are dropped: ninja stats through links, so a broken one
    would read as a permanently missing input and rebuild the edge every time.
    """
    deps = [source]
    for relpath, _ in walk(source, excludes):
        full = os.path.join(source, relpath)
        if os.path.exists(full):
            deps.append(full)
    deps.sort()
    return deps


def remove(path):
    """Remove whatever is at `path`, without following a symlink there."""
    if os.path.isdir(path) and not os.path.islink(path):
        shutil.rmtree(path)
    elif os.path.lexists(path):
        os.unlink(path)


def ensure_directory(path):
    """Make `path` a real directory, replacing anything else that's there.
    """
    if os.path.lexists(path) and (os.path.islink(path) or not os.path.isdir(path)):
        os.unlink(path)
    os.makedirs(path, exist_ok=True)


def copy_symlink(source, dest):
    target = os.readlink(source)
    if os.path.islink(dest) and os.readlink(dest) == target:
        return
    remove(dest)
    os.symlink(target, dest)


def is_up_to_date(source, dest):
    if os.path.islink(dest) or not os.path.isfile(dest):
        return False
    source_stat = os.stat(source)
    dest_stat = os.stat(dest)
    return source_stat.st_size == dest_stat.st_size and source_stat.st_mtime_ns == dest_stat.st_mtime_ns


def copy_file(source, dest):
    if is_up_to_date(source, dest):
        return
    # Unlink first, rather than writing over the destination, so that a
    # read-only file from an earlier copy doesn't make this fail.
    remove(dest)
    # Give the copy the mtime the source had before copying started, so an edit
    # that races with the copy doesn't make stale contents look up to date.
    source_stat = os.stat(source)
    shutil.copy2(source, dest, follow_symlinks=False)
    os.utime(dest, ns=(source_stat.st_atime_ns, source_stat.st_mtime_ns))


def copy_tree(source, dest, excludes):
    """Merge the contents of `source` into `dest`. Symlinks are copied as links.

    Returns the destination path of every regular file copied. Links and
    directories are left out because their mtimes don't follow the source's.
    """
    copied = []
    ensure_directory(dest)
    for relpath, is_dir in walk(source, excludes):
        src_path = os.path.join(source, relpath)
        dst_path = os.path.join(dest, relpath)
        if is_dir:
            ensure_directory(dst_path)
        elif os.path.islink(src_path):
            copy_symlink(src_path, dst_path)
        else:
            copy_file(src_path, dst_path)
            copied.append(dst_path)
    return copied


def check_destination(source, dest):
    """Refuse destinations that would make the copy destroy or recurse into the source."""
    src_real = os.path.realpath(source)
    dst_real = os.path.realpath(dest)

    if os.path.dirname(dst_real) == dst_real:
        raise SystemExit("refusing to copy over filesystem root: %s" % dst_real)
    if dst_real == src_real:
        raise SystemExit("refusing to copy: the destination is the source (%s)" % dst_real)
    if src_real.startswith(dst_real + os.sep):
        raise SystemExit(
            "refusing to copy: destination %s contains the source %s" % (dst_real, src_real)
        )
    if dst_real.startswith(src_real + os.sep):
        raise SystemExit(
            "refusing to copy: source %s contains the destination %s" % (src_real, dst_real)
        )


def write_depfile(path, target, deps):
    """Write the depfile, via a temporary so a crash can't leave a partial one."""
    lines = ["%s:" % escape(target)]
    lines.extend("  %s" % escape(dep) for dep in deps)

    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)

    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        f.write(" \\\n".join(lines))
        f.write("\n")
    os.replace(tmp, path)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--source",
        action="append",
        required=True,
        help="directory to copy from. Repeatable when --dest is omitted, to scan "
        "several directories into one depfile.",
    )
    parser.add_argument(
        "--dest",
        help="directory to copy into; the source's contents are merged into it. "
        "If omitted, nothing is copied and only the depfile is written.",
    )
    parser.add_argument("--depfile", required=True, help="depfile to write")
    parser.add_argument(
        "--stamp",
        help="stamp file to touch on success; use this as the custom command's OUTPUT",
    )
    parser.add_argument(
        "--depfile-target",
        help="target name in the depfile (default: the stamp, else the destination). "
        "It must match the edge output as ninja spells it; CMake rewrites this for "
        "you under policy CMP0116 NEW.",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        metavar="PATTERN",
        help="glob excluded from both the copy and dependency tracking, matched "
        "against source-relative paths: 'foo' only skips a top-level foo, while "
        "'*/foo' also skips nested ones. Repeatable.",
    )
    parser.add_argument(
        "--delete-destination",
        action="store_true",
        help="remove the destination before copying, for exact-mirror semantics",
    )
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(argv)
    start_time_ns = time.time_ns()

    sources = [os.path.abspath(source) for source in args.source]
    for source in sources:
        if not os.path.isdir(source):
            sys.stderr.write("error: source is not a directory: %s\n" % source)
            return 1

    target = args.depfile_target or args.stamp or args.dest
    if not target:
        parser.error("--depfile-target or --stamp is required without --dest")

    if args.dest:
        if len(sources) != 1:
            parser.error("--dest takes exactly one --source")
        source = sources[0]
        dest = os.path.abspath(args.dest)
        check_destination(source, dest)
        if args.delete_destination:
            remove(dest)
        if args.verbose:
            sys.stderr.write("copying %s to %s\n" % (source, dest))
        copied = copy_tree(source, dest, args.exclude)
    elif args.delete_destination:
        parser.error("--delete-destination requires --dest")

    # Scan after the copy, so the depfile reflects anything that landed in the
    # source while the copy was running.
    deps = []
    for source in sources:
        deps.extend(collect_deps(source, args.exclude))
    if args.dest:
        deps.extend(sorted(copied))

    if args.stamp:
        stamp_parent = os.path.dirname(args.stamp)
        if stamp_parent:
            os.makedirs(stamp_parent, exist_ok=True)
        with open(args.stamp, "w"):
            pass
        # Backdate the stamp to when the copy started. ninja then sees any
        # source edited mid-copy as newer than the stamp, and copies again.
        os.utime(args.stamp, ns=(start_time_ns, start_time_ns))

    write_depfile(args.depfile, target, deps)

    if args.verbose:
        sys.stderr.write("wrote %s (%d deps)\n" % (args.depfile, len(deps)))

    return 0


if __name__ == "__main__":
    sys.exit(main())
