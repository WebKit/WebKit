#!/usr/bin/env python3

# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Resyncs libwebrtc and its sub-libraries to a new Chromium release, on a single
# all-webrtc-<chrome version> branch. Run it without arguments, or with --help,
# for how to use it (the text is OVERVIEW below).

import argparse
import collections
import datetime
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

# FIXME: There is no automated test coverage. The resync replay, branch naming, remote
# selection and push (against a local bare repo) were only exercised by hand in scratch
# repos; a small unittest file reusing those fixtures would protect against regressions.
OVERVIEW = """Resync libwebrtc and its sub-libraries (abseil-cpp, boringssl, libvpx, ...) to a
new Chromium release, all on one branch named all-webrtc-<chrome version>
(e.g. all-webrtc-m155).

How to use it, in this order:

  1. {name} update-chromium <chrome version> <chromium branch>
         e.g.  {name} update-chromium m155 7xyz
     Fetches that Chromium release branch into your Chromium checkout
     (by default ../chromium/src, next to the WebKit checkout) and runs
     `gclient sync`. THIS CAN TAKE A LONG TIME -- start it well ahead of the
     resync. The Chrome version and checkout path are remembered for the
     next commands.

  2. {name} resync
     Creates all-webrtc-<chrome version> and, on it:
       - copies every library from Chromium (one commit per library),
       - reapplies the WebKit commits of the previous all-webrtc-<version>
         branch (it asks which version it was, and the name of the git remote
         -- a WebKit clone -- to fetch it from),
       - replays the commits made in WebKit since the last resync, one by one
         with their original titles (only their changes under
         Source/ThirdParty/libwebrtc/Source; it lists them and lets you choose),
       - then runs the validation (step 3).
     It pauses to let you resolve conflicts, then continues.

  3. {name} validate
     Checks the file lists in libwebrtc's CMakeLists.txt, reports lost
     {macro} code (and asks before building), builds WebKit with CMake
     (Debug), then asks whether to also build the fuzzers (make release
     ASAN=YES LIBFUZZER=YES SCHEME="All Fuzzers"), iOS simulator and iOS
     device (Release) -- worth saying yes at least once before the branch is
     final (--fuzzers, --ios-simulator and --ios-device answer yes up front).
     Then it runs the WebRTC layout tests. When everything passes, the branch is pushed
     to that git remote (its name is asked once) so the next resync can start
     from it. You will
     usually need to fix the branch and validate again: run `validate` as many
     times as needed (resync already runs it once).

Resyncing the same Chrome version again (say an updated m155, after the first
one landed in trunk): run `resync --chrome-version m155 --previous-version m155`
from the updated trunk. The name all-webrtc-m155 being taken, it proposes
all-webrtc-m155-2, reapplies the WebKit changes of the first branch, and asks
which commit the first resync landed in (the commits made since are replayed). Give
`m155-2` as the previous version in the next resync.

Everything is logged to update-libwebrtc-<command>-<timestamp>.log under
WebKitBuild/update-libwebrtc/ (ignored by git); the screen shows a condensed
view.
Use '{name} help' (same as --help), or '{name} help <command>' for a command's options."""

SCRIPTS_DIR = Path(__file__).resolve().parent

# Matches the way resync commits are titled in this repo's history, e.g.
# "Resync libwebrtc up to M150" or "Update libyuv to M143".
RESYNC_SUBJECT_RE = re.compile(r"^(resync|update)\b", re.IGNORECASE)

ALL_WEBRTC_KEY = "all-webrtc"

# A Chrome version such as "m155", optionally with a suffix ("m155-2") to tell
# apart another resync of the same Chrome version.
CHROME_VERSION_RE = r"m\d+(?:-[A-Za-z0-9.]+)*"

DEFAULT_MAIN_BRANCH = "main"

# Where the logs go, relative to the repo root; WebKitBuild is ignored by git.
LOG_DIRECTORY = "WebKitBuild/update-libwebrtc"
CMAKE_FILE = "Source/ThirdParty/libwebrtc/CMakeLists.txt"
WEBKIT_BUILD_MACRO = "WEBRTC_WEBKIT_BUILD"
WEBKIT_BUILD_PATHS = ["Source/ThirdParty/libwebrtc"]

# Layout test directories exercising libwebrtc and the codecs/containers it pulls in.
DEFAULT_TESTS = (
    "webrtc",
    "http/wpt/webrtc",
    "imported/w3c/web-platform-tests/webrtc",
    "imported/w3c/web-platform-tests/webrtc-encoded-transform",
)

# Keep symbol names mangled in link errors, so missing libwebrtc symbols can be
# added to Configurations/libwebrtc.exp as they are. -Xlinker (not -Wl,) because
# some targets link through swiftc, which rejects -Wl,. build-webkit hands
# --cmakeargs to a shell, hence the inner quotes around the flags.
CMAKE_LINKER_ARGS = ('-DCMAKE_SHARED_LINKER_FLAGS="-Xlinker --no-demangle" '
                     '-DCMAKE_EXE_LINKER_FLAGS="-Xlinker --no-demangle"')

# Lines of command output echoed on screen; everything goes to the log file.
OUTPUT_TAIL_LINES = 15
TAIL_LINES_ON_SUCCESS = 5
TAIL_LINES_ON_FAILURE = 30
ERROR_LINES_ON_FAILURE = 20


@dataclass
class Library:
    key: str
    description: str
    dest: str  # path relative to the WebKit repo root
    chromium_subpath: str  # path relative to the Chromium checkout's src/
    excludes: tuple = ()  # rsync --exclude patterns, relative to source root; excluded paths are also protected from --delete
    renames: tuple = ()  # (old, new) paths relative to dest, applied after copy
    extra_copies: tuple = ()  # (chromium subpath, dest subpath) pairs copied alongside the main source
    notes: tuple = ()  # manual follow-up steps to print after this library


LIBWEBRTC_ROOT = "Source/ThirdParty/libwebrtc/Source"
THIRD_PARTY = f"{LIBWEBRTC_ROOT}/third_party"

LIBRARIES = [
    Library(
        key="abseil-cpp",
        description="abseil-cpp",
        dest=f"{THIRD_PARTY}/abseil-cpp",
        chromium_subpath="third_party/abseil-cpp",
        renames=(
            (
                "absl/numeric/int128_have_intrinsic.inc",
                "absl/numeric/int128_have_intrinsic.inc.h",
            ),
        ),
        notes=(
            "Confirm WEBRTC_WEBKIT_BUILD-guarded code survived the resync.",
        ),
    ),
    Library(
        key="boringssl",
        description="BoringSSL",
        dest=f"{THIRD_PARTY}/boringssl",
        chromium_subpath="third_party/boringssl",
        excludes=("src/pki/testdata", "src/fuzz", "docs"),
        notes=(
            "Confirm WEBRTC_WEBKIT_BUILD-guarded code survived the resync.",
        ),
    ),
    Library(
        key="crc32c",
        description="crc32c",
        dest=f"{THIRD_PARTY}/crc32c",
        chromium_subpath="third_party/crc32c",
    ),
    Library(
        key="sframe",
        description="sframe (Cisco reference SFrame/RFC 9605 implementation)",
        dest=f"{THIRD_PARTY}/sframe",
        chromium_subpath="third_party/sframe",
    ),
    Library(
        key="libsrtp",
        description="libsrtp",
        dest=f"{THIRD_PARTY}/libsrtp",
        chromium_subpath="third_party/libsrtp",
        notes=(
            "Check whether third_party/libsrtp/.gitattributes is still needed.",
        ),
    ),
    Library(
        key="libyuv",
        description="libyuv",
        dest=f"{THIRD_PARTY}/libyuv",
        chromium_subpath="third_party/libyuv",
    ),
    Library(
        key="opus",
        description="opus",
        dest=f"{THIRD_PARTY}/opus",
        chromium_subpath="third_party/opus",
        excludes=("src/ChangeLog",),
        notes=(
            "Fix opus/src/silk/sort.c for FIXED_POINT if needed.",
        ),
    ),
    Library(
        key="pffft",
        description="pffft",
        dest=f"{THIRD_PARTY}/pffft",
        chromium_subpath="third_party/pffft",
        notes=(
            "Re-check the 'read void' compilation fix -- confirm it's still "
            "needed and reapply as a follow-up commit if so.",
        ),
    ),
    Library(
        key="rnnoise",
        description="rnnoise",
        dest=f"{THIRD_PARTY}/rnnoise",
        chromium_subpath="third_party/rnnoise",
    ),
    Library(
        key="libwebm",
        description="libwebm",
        dest=f"{THIRD_PARTY}/libwebm",
        chromium_subpath="third_party/libwebm/source",
    ),
    Library(
        key="libvpx",
        description="libvpx",
        dest=f"{THIRD_PARTY}/libvpx",
        chromium_subpath="third_party/libvpx",
        notes=(
            "Remove SVE/I8mm lines from "
            "source/config/linux/arm64-highbd/vpx_dsp_rtcd.h and vp9_rtcd.h "
            "(and the iOS/mac x64 vp9_rtcd.h variants) as needed.",
            "May need to rebuild simulator roots: "
            "sudo Tools/Scripts/build-root --groups webkit --train LuckE --ios-simulator",
        ),
    ),
    Library(
        key="libaom",
        description="libaom",
        dest=f"{THIRD_PARTY}/libaom",
        chromium_subpath="third_party/libaom",
        notes=(
            "Recreate the libaom GitHub mirror repo if needed.",
            "Update config files for any new platforms/toolchain versions.",
        ),
    ),
    Library(
        key="dav1d",
        description="dav1d AV1 decoder",
        dest="Source/WebCore/PAL/ThirdParty/dav1d",
        chromium_subpath="third_party/dav1d/libdav1d",
        # WebKit-only files living in the dav1d directory; config/ and
        # vcs_version.h come from other Chromium paths (see extra_copies).
        excludes=("/dav1d.xcodeproj", "/Configurations", "/config.h", "/config", "/vcs_version.h"),
        extra_copies=(
            ("third_party/dav1d/config", "config"),
            ("third_party/dav1d/version/vcs_version.h", "vcs_version.h"),
        ),
    ),
    Library(
        key="webrtc",
        description="libwebrtc itself",
        dest=f"{LIBWEBRTC_ROOT}/webrtc",
        chromium_subpath="third_party/webrtc",
        excludes=("sdk", "/webkit_sdk"),
        notes=(
            "sdk/ is excluded and removed from the destination by --delete; "
            "check whether anything there needs porting into webkit_sdk/ first.",
            "Diff WebCore's encoder.h/encoder.mm against the new libwebrtc "
            "encoder implementation and port any relevant changes.",
            "Update the libwebrtc Xcode project to remove files no longer in the tree.",
            "Update Configurations/libwebrtc.exp for missing symbols (the CMake build "
            "in `validate` already keeps link errors mangled with -Xlinker "
            "--no-demangle; for Xcode builds add it to WebCore.xcconfig).",
            "Add any missing headers to the libwebrtc Xcode target (Public headers).",
        ),
    ),
]

WEBRTC = next(lib for lib in LIBRARIES if lib.key == "webrtc")

GLOBAL_NOTES = (
    "Before the branch is final, run `validate` and answer yes to the fuzzers, "
    "iOS simulator and iOS device builds (or pass --fuzzers --ios-simulator "
    "--ios-device).",
    "Compile WebCore and add any missing libwebrtc headers to the Xcode "
    "target (Public headers) as needed.",
)


_log_file = None
_log_path = None


def open_log(root, command):
    """Open the log file receiving everything done by this run (commands, their
    full output, prompts and answers); the screen only gets a condensed view."""
    global _log_file, _log_path
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    log_directory = root / LOG_DIRECTORY
    log_directory.mkdir(parents=True, exist_ok=True)
    _log_path = log_directory / f"update-libwebrtc-{command}-{timestamp}.log"
    _log_file = open(_log_path, "a", buffering=1)
    return _log_path


def log(text=""):
    if _log_file:
        _log_file.write(text + ("" if text.endswith("\n") else "\n"))


def say(text="", end="\n"):
    """Print to the screen and record in the log."""
    print(text, end=end)
    log(text)


# FIXME: About 30 places call subprocess.run(["git", ...], cwd=root, capture_output=True,
# text=True, ...) by hand. A git(root, *args, check=False) helper returning the stripped
# output would shrink the file and make error handling and logging consistent.
def run(cmd, cwd=None, check=True, stream=False):
    """Run a command, logging it and its full output. The screen only gets
    the tail of the output when the command fails, unless `stream` is set
    (for long-running commands whose output the user wants to follow)."""
    log(f"$ {' '.join(str(c) for c in cmd)}" + (f"   (in {cwd})" if cwd else ""))
    if stream:
        say(f"$ {' '.join(str(c) for c in cmd)}" + (f"   (in {cwd})" if cwd else ""))
        result = subprocess.run(cmd, cwd=cwd)
        log(f"[exit status {result.returncode}]")
        if check and result.returncode != 0:
            raise subprocess.CalledProcessError(result.returncode, cmd)
        return result
    result = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log(result.stdout)
    log(f"[exit status {result.returncode}]")
    if result.returncode != 0:
        say(f"  Command failed: {' '.join(str(c) for c in cmd)}")
        tail = result.stdout.splitlines()[-OUTPUT_TAIL_LINES:]
        for line in tail:
            say(f"    {line}")
        say(f"  (full output in {_log_path})")
        if check:
            raise subprocess.CalledProcessError(result.returncode, cmd, output=result.stdout)
    return result


def prompt(message, default=None):
    suffix = f" [{default}]" if default else ""
    value = input(f"{message}{suffix}: ").strip()
    log(f"{message}{suffix}: {value}")
    return value or default


def confirm(message, default=False):
    suffix = " [Y/n] " if default else " [y/N] "
    value = input(message + suffix).strip().lower()
    log(f"{message}{suffix}{value}")
    if not value:
        return default
    return value in ("y", "yes")


def state_path(root):
    out = subprocess.run(
        ["git", "rev-parse", "--git-path", "update-libwebrtc-state.json"],
        cwd=root, capture_output=True, text=True, check=True,
    ).stdout.strip()
    return Path(out) if Path(out).is_absolute() else root / out


def load_state(root):
    """Settings remembered between commands (Chrome version, Chromium path),
    kept inside .git so they never show up as untracked files."""
    try:
        return json.loads(state_path(root).read_text())
    except (OSError, ValueError):
        return {}


def save_state(root, **updates):
    state = load_state(root)
    state.update(updates)
    state_path(root).write_text(json.dumps(state, indent=2) + "\n")


def repo_root():
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True, text=True, check=True,
    )
    return Path(result.stdout.strip())


def current_branch(root):
    result = subprocess.run(
        ["git", "rev-parse", "--abbrev-ref", "HEAD"],
        cwd=root, capture_output=True, text=True, check=True,
    )
    return result.stdout.strip()


def has_unresolved_operation(root):
    """True if a cherry-pick/merge/etc. is mid-flight or the index has
    unresolved conflicts -- switching branches would fail (or lose state) in
    that case, so callers should leave the user on the current branch."""
    git_dir = subprocess.run(
        ["git", "rev-parse", "--git-dir"],
        cwd=root, capture_output=True, text=True, check=True,
    ).stdout.strip()
    git_dir_path = (root / git_dir) if not Path(git_dir).is_absolute() else Path(git_dir)
    if (git_dir_path / "CHERRY_PICK_HEAD").exists() or (git_dir_path / "MERGE_HEAD").exists():
        return True
    unmerged = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=U"],
        cwd=root, capture_output=True, text=True, check=True,
    ).stdout.strip()
    return bool(unmerged)


def checkout_branch_if_safe(root, branch_name):
    """Best-effort return to `branch_name`. Refuses (and warns) if there's an
    unresolved cherry-pick/merge in progress, since `git checkout` would
    either fail outright or silently abandon that state."""
    if has_unresolved_operation(root):
        say(f"  Not switching back to '{branch_name}': there's an unresolved "
            f"cherry-pick/merge in this checkout. Resolve it (or `git cherry-pick "
            f"--abort`) before switching branches.")
        return False
    run(["git", "checkout", branch_name], cwd=root)
    return True


def sanitize_branch_component(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", value.strip()).strip("-")


def ensure_branch(root, branch_name, assume_yes=False):
    """Check out `branch_name`, creating it if needed. Returns the branch
    name, or None if the user chose to stay on the current branch. With
    assume_yes, an existing branch is checked out without asking."""
    if current_branch(root) == branch_name:
        say(f"  Already on branch '{branch_name}'.")
        return branch_name

    branch_exists = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", branch_name],
        cwd=root, capture_output=True, text=True,
    ).returncode == 0

    if branch_exists:
        if not assume_yes and not confirm(f"  Branch '{branch_name}' already exists. Check it out?", default=True):
            say("  Staying on current branch.")
            return None
        run(["git", "checkout", branch_name], cwd=root)
        return branch_name

    if not confirm(f"  Create branch '{branch_name}' for this resync?", default=True):
        say("  Staying on current branch.")
        return None
    run(["git", "checkout", "-b", branch_name], cwd=root)
    return branch_name


def choose_branch_name(root, chrome_version, remote):
    """all-webrtc-<version>, unless that name is already taken, locally or on
    `remote` (typically by an earlier resync of the same Chrome version, now
    landed): then offer the next free all-webrtc-<version>-N instead."""
    base = f"{ALL_WEBRTC_KEY}-{sanitize_branch_component(chrome_version)}"
    if current_branch(root) == base:
        return base

    def taken(name):
        return local_branch_exists(root, name) or remote_has_branch(root, remote, name)

    if not taken(base):
        return base
    where = "locally" if local_branch_exists(root, base) else f"on remote '{remote}'"
    number = 2
    while taken(f"{base}-{number}"):
        number += 1
    new_name = f"{base}-{number}"
    say(f"  Branch '{base}' already exists {where}.")
    choice = prompt(f"  Use a [n]ew branch '{new_name}' (another resync of the same Chrome version), "
                    f"or continue on the [e]xisting '{base}'", default="n")
    return base if choice.strip().lower() in ("e", "existing") else new_name


def resolve_source_root(lib, chromium_path):
    source_root = chromium_path / lib.chromium_subpath
    if not source_root.is_dir():
        say(f"  Chromium path does not exist: {source_root}")
        return None
    return source_root


def find_last_resync_commit(root, dest):
    """Return (hash, subject) of the most recent commit touching `dest` whose
    subject looks like a resync/update commit, or (None, None) if none is
    found. git prefilters on the message (which also matches body lines); the
    subject check is what decides."""
    result = subprocess.run(
        ["git", "log", "--extended-regexp", "--regexp-ignore-case", "--grep=^(resync|update)",
         "--format=%H\t%s", "--", dest],
        cwd=root, capture_output=True, text=True, check=True,
    )
    for line in result.stdout.splitlines():
        commit_hash, _, subject = line.partition("\t")
        if RESYNC_SUBJECT_RE.match(subject.strip()):
            return commit_hash, subject.strip()
    return None, None


def has_changes(root, path):
    status = subprocess.run(
        ["git", "status", "--porcelain", "--", path],
        cwd=root, capture_output=True, text=True, check=True,
    )
    return bool(status.stdout.strip())


def commit_path(root, path, message):
    """Stage everything under `path` (relative to `root`) and commit it.
    Returns True if a commit was created, False if there was nothing to commit."""
    if not has_changes(root, path):
        say(f"  Nothing to commit under {path}.")
        return False
    run(["git", "add", "-A", "--", path], cwd=root)
    run(["git", "commit", "-m", message], cwd=root)
    return True


def commit_copy_step(root, lib, chromium_label, ask=True):
    if ask and not confirm(f"  Commit the copied '{lib.key}' files now?", default=True):
        say("  Leaving the copy uncommitted.")
        return
    message = f"Resync {lib.key} from {chromium_label}"
    commit_path(root, lib.dest, message)


def rsync_copy(source_root, dest_root, excludes, dry_run):
    """Mirror a directory (with --delete), or copy a single file to `dest_root`."""
    is_file = source_root.is_file()
    cmd = ["rsync", "-a"] + ([] if is_file else ["--delete"])
    if dry_run:
        cmd += ["-n", "-v"]
    cmd += ["--exclude", ".git"]
    for pattern in excludes:
        cmd += ["--exclude", pattern]
    cmd += [str(source_root), str(dest_root)] if is_file else [f"{source_root}/", f"{dest_root}/"]
    result = run(cmd)
    if not dry_run:
        return
    lines = [line for line in result.stdout.splitlines()
             if line and not line.startswith(("sending ", "sent ", "total size"))]
    deleted = [line for line in lines if line.startswith("deleting ")]
    say(f"  {len(lines) - len(deleted)} path(s) would be updated, {len(deleted)} deleted "
        f"(full list in {_log_path}).")
    for line in deleted[:20]:
        say(f"    {line}")
    if len(deleted) > 20:
        say(f"    ... and {len(deleted) - 20} more deletions")


def copy_library(chromium_path, root, lib, dry_run):
    """Copy a library's main source and any extra_copies from Chromium."""
    rsync_copy(chromium_path / lib.chromium_subpath, root / lib.dest, lib.excludes, dry_run)
    for chromium_subpath, dest_subpath in lib.extra_copies:
        rsync_copy(chromium_path / chromium_subpath, root / lib.dest / dest_subpath, (), dry_run)


def apply_renames(dest_root, renames):
    for old, new in renames:
        old_path = dest_root / old
        new_path = dest_root / new
        if old_path.exists():
            say(f"  Renaming {old} -> {new}")
            old_path.rename(new_path)


def detect_branch_base(root, tip_local_ref):
    """Find where a fetched branch forked off WebKit history, i.e. the
    merge-base between its tip and our current HEAD. Returns the commit hash,
    or None if the branch shares no history with this checkout (e.g. it was
    fetched from an unrelated third-party repo rather than a WebKit fork)."""
    result = subprocess.run(
        ["git", "merge-base", tip_local_ref, "HEAD"],
        cwd=root, capture_output=True, text=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def select_commits_to_reapply(commits):
    """Show the numbered commit list and let the user pick a subset (in
    original/chronological order), rather than an all-or-nothing choice."""
    say(f"  {len(commits)} commit(s) available to reapply (oldest first):")
    for index, (commit_hash, subject) in enumerate(commits, start=1):
        say(f"   {index:2d}. {commit_hash[:12]} {subject}")

    choice = prompt("  Which commits do you want to reapply? "
                    "(comma-separated numbers, 'a' for all, 'n' for none)",
                    default="a")
    choice = choice.strip().lower()
    if choice in ("n", "none"):
        return []
    if choice in ("a", "all"):
        return list(commits)

    chosen_indexes = set()
    for token in choice.split(","):
        token = token.strip()
        if not token:
            continue
        if token.isdigit() and 1 <= int(token) <= len(commits):
            chosen_indexes.add(int(token))
        else:
            say(f"  Ignoring invalid selection: {token}")

    return [commits[i - 1] for i in sorted(chosen_indexes)]


def conflicted_paths(root):
    result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=U"],
        cwd=root, capture_output=True, text=True, check=True,
    )
    return [line for line in result.stdout.splitlines() if line]


def commit_info(root, commit):
    """(author "Name <email>", full message) of `commit`."""
    result = subprocess.run(
        ["git", "log", "-1", "--format=%an <%ae>%x00%B", commit],
        cwd=root, capture_output=True, text=True, check=True,
    )
    author, _, message = result.stdout.partition("\0")
    return author, message.rstrip("\n")


def discard_pick_attempt(root, commit, dests):
    """Undo a failed/skipped attempt to apply `commit` under `dests`."""
    added = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=A", f"{commit}^", commit, "--", *dests],
        cwd=root, capture_output=True, text=True, check=True,
    ).stdout.splitlines()
    subprocess.run(["git", "reset", "-q", "HEAD", "--", *dests], cwd=root, capture_output=True)
    subprocess.run(["git", "checkout", "HEAD", "--", *dests], cwd=root, capture_output=True)
    for path in added:
        (root / path).unlink(missing_ok=True)


def apply_commit_to_dests(root, commit, dests):
    """Apply only the part of `commit` that touches `dests`, staged in the
    index. Anything the commit changes elsewhere is deliberately left
    out, so a resync of one library can never modify another. Returns
    True on a clean apply, False if there are conflicts to resolve."""
    patch = subprocess.run(
        ["git", "diff", "--binary", f"{commit}^", commit, "--", *dests],
        cwd=root, capture_output=True, check=True,
    ).stdout
    result = subprocess.run(
        ["git", "apply", "--3way", "--index"],
        cwd=root, input=patch, capture_output=True,
    )
    for output in (result.stdout, result.stderr):
        if output:
            text = output.decode(errors="replace")
            log(text)
            if result.returncode != 0:
                say(text, end="")
    return result.returncode == 0


def commit_picked(root, commit, dests):
    """Commit the staged result of apply_commit_to_dests with the original
    author and message (plus a `-x` style trailer). Returns False if nothing
    was staged (the change was already present)."""
    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=root).returncode == 0:
        say(f"  {commit[:12]} produced no changes under {', '.join(dests)}; skipping.")
        return False
    author, message = commit_info(root, commit)
    message += f"\n\n(cherry picked from commit {commit})"
    subprocess.run(
        ["git", "commit", "-q", "--author", author, "-F", "-"],
        cwd=root, input=message, text=True, check=True,
    )
    return True


def run_cherry_pick_interactively(root, commit_hashes, dests):
    """Reapply each commit's changes to `dests` only (never to other paths), one
    commit at a time, keeping the original author and message. If a commit
    conflicts, pause and let the user fix it (in their own shell/editor)
    before resuming -- rather than aborting the whole patch step. Returns
    True if every commit was eventually applied (or explicitly skipped),
    False if the user chose to abort."""
    for commit in commit_hashes:
        subject = commit_info(root, commit)[1].splitlines()[0]
        say(f"  Applying {commit[:12]} {subject}")
        clean = apply_commit_to_dests(root, commit, dests)

        while not clean:
            paths = conflicted_paths(root)
            say("\n  Applying this commit stopped on a conflict"
                + (f" in: {', '.join(paths)}" if paths else "") + ".")
            say(f"  Resolve it in another shell at {root}, `git add` the "
                f"resolved files, then come back here.")
            choice = prompt("  Type 'continue' once resolved, 'skip' to drop this "
                            "commit, or 'abort' to cancel the patch step",
                            default="continue")
            choice = (choice or "continue").strip().lower()

            if choice in ("skip", "abort"):
                discard_pick_attempt(root, commit, dests)
                if choice == "abort":
                    say("  Patch step aborted (commits already applied are kept, "
                        "but this branch is incomplete).")
                    return False
                break

            if conflicted_paths(root):
                say("  There are still unresolved paths.")
                continue
            clean = True

        if clean:
            commit_picked(root, commit, dests)
    return True


def commits_to_reapply(root, range_spec, dests):
    """Commits in `range_spec` (oldest first) that touch `dests`, split into
    (candidates, skipped). The branch may itself start from an earlier resync
    of this library (e.g. "Resync libwebrtc/boringssl up to M143") before
    layering WebKit fixes on top. That prior resync is already reflected in
    WebKit's history and must not be reapplied, so commits whose subject looks
    like a resync are returned as `skipped`."""
    output = subprocess.run(
        ["git", "rev-list", "--reverse", "--no-merges", "--format=%H\t%s", range_spec, "--", *dests],
        cwd=root, capture_output=True, text=True, check=True,
    ).stdout
    commits = []
    for line in output.splitlines():
        if line.startswith("commit "):
            continue
        commit_hash, _, subject = line.partition("\t")
        commits.append((commit_hash, subject))
    candidates = [(h, s) for h, s in commits if not RESYNC_SUBJECT_RE.match(s.strip())]
    skipped = [(h, s) for h, s in commits if RESYNC_SUBJECT_RE.match(s.strip())]
    return candidates, skipped


def print_commit_preview(candidates, skipped):
    if candidates:
        say(f"  {len(candidates)} commit(s) would be reapplied (oldest first):")
        for commit_hash, subject in candidates:
            say(f"   {commit_hash[:12]} {subject}")
    else:
        say("  No WebKit-side commits would be reapplied.")
    if skipped:
        say(f"  {len(skipped)} earlier-resync commit(s) excluded (already in WebKit history):")
        for commit_hash, subject in skipped:
            say(f"   - {commit_hash[:12]} {subject}")


def reapply_from_branch(root, remote, ref, key, dests):
    """Fetch `ref` from `remote` and reapply its WebKit-side commits, limited
    to the paths in `dests`. Returns False on failure or abort."""
    tip_local_ref = f"refs/tmp/resync-{key}-tip"
    base_local_ref = f"refs/tmp/resync-{key}-base"
    try:
        run(["git", "fetch", remote, f"{ref}:{tip_local_ref}"], cwd=root)

        detected_base = detect_branch_base(root, tip_local_ref)
        if detected_base:
            say(f"  Detected branch base (fork point from WebKit history): {detected_base[:12]}")
            print_commit_preview(*commits_to_reapply(
                root, f"{detected_base}..{tip_local_ref}", dests))
        else:
            say("  Could not auto-detect a branch base -- this branch shares no "
                "history with the current checkout.")
        base = prompt("  Base ref/commit the branch was built on top of "
                      "(leave blank to cherry-pick only the tip commit)",
                      default=detected_base)

        if not base:
            tip_hash = subprocess.run(
                ["git", "rev-parse", tip_local_ref],
                cwd=root, capture_output=True, text=True, check=True,
            ).stdout.strip()
            return run_cherry_pick_interactively(root, [tip_hash], dests)

        # `base` may be a hash we already resolved locally (detected_base) or
        # something the user typed that still needs fetching from `remote`.
        if base == detected_base:
            base_ref_for_range = base
        else:
            run(["git", "fetch", remote, f"{base}:{base_local_ref}"], cwd=root)
            base_ref_for_range = base_local_ref

        candidates, skipped = commits_to_reapply(
            root, f"{base_ref_for_range}..{tip_local_ref}", dests)
        if base != detected_base:
            print_commit_preview(candidates, skipped)

        if not candidates:
            say(f"  No commits to reapply between {base} and {ref}.")
            return True

        to_pick = select_commits_to_reapply(candidates)
        if not to_pick:
            say("  Skipping patch step.")
            return True

        return run_cherry_pick_interactively(root, [h for h, _ in to_pick], dests)
    except subprocess.CalledProcessError:
        say("  Fetch failed -- check the remote/ref and try again.")
        return False
    finally:
        for local_ref in (tip_local_ref, base_local_ref):
            subprocess.run(["git", "update-ref", "-d", local_ref], cwd=root, capture_output=True)


def remote_has_branch(root, remote, branch):
    result = subprocess.run(
        ["git", "ls-remote", "--heads", remote, f"refs/heads/{branch}"],
        cwd=root, capture_output=True, text=True,
    )
    return result.returncode == 0 and bool(result.stdout.strip())


# FIXME: This function is ~110 lines and does library discovery, base selection, branch
# choice, the copy and the patch/replay steps. Split out "choose the commits to replay" and
# "copy all libraries" so each phase can be tested on its own.
def resync_all_webrtc(root, chromium_path, chrome_version, previous_chrome_version, remote):
    """Resync every library on a single `all-webrtc-<version>` branch: copy
    each from Chromium (one commit each), reapply the WebKit-side commits
    from the previous `all-webrtc-<version>` branch, then replay the commits
    that changed Source/ThirdParty/libwebrtc/Source since the last webrtc
    resync, one by one, keeping their titles (only the part of each commit
    under that folder is applied).
    Returns the branch name, or None if the resync did not complete."""
    say(f"\n=== {ALL_WEBRTC_KEY}: every library, on a single branch ===")

    libs = []
    for lib in LIBRARIES:
        if resolve_source_root(lib, chromium_path) is None:
            say(f"  Skipping {lib.key}.")
            continue
        libs.append(lib)

    dirty = [lib.dest for lib in libs if has_changes(root, lib.dest)]
    if dirty:
        say(f"  Warning: uncommitted changes under: {', '.join(dirty)}")
        if not confirm("  Continue anyway?", default=False):
            say("  Skipping.")
            return None

    # The commits to replay only depend on committed history, so they are
    # listed (and chosen) now, before any new commit lands on the branch.
    base_commit, base_subject = find_last_resync_commit(root, WEBRTC.dest)
    if base_commit:
        say(f"  Last commit that looks like a webrtc resync: {base_commit[:12]} {base_subject}")
    else:
        say(f"  Could not find a prior resync commit for {WEBRTC.dest}.")
    recent = subprocess.run(["git", "log", "-8", "--format=%h %s (%cs)", "--", WEBRTC.dest],
                            cwd=root, capture_output=True, text=True).stdout.splitlines()
    say(f"  Recent commits touching {WEBRTC.dest}:")
    for line in recent:
        say(f"    {line}")
    chosen = prompt("  Base commit: the commit where the previous resync landed; the commits made "
                    "since are replayed (hash or ref; blank to replay none)", default=base_commit)
    if chosen and chosen != base_commit:
        base_commit = subprocess.run(
            ["git", "rev-parse", "--verify", f"{chosen}^{{commit}}"],
            cwd=root, capture_output=True, text=True,
        ).stdout.strip() or None
        if not base_commit:
            say(f"  Could not resolve '{chosen}'.")
    elif not chosen:
        base_commit = None

    to_replay = []
    if base_commit:
        say(f"\n  Commits since {base_commit[:12]} that changed {LIBWEBRTC_ROOT}:")
        candidates, skipped = commits_to_reapply(root, f"{base_commit}..HEAD", [LIBWEBRTC_ROOT])
        print_commit_preview(candidates, skipped)
        if candidates:
            to_replay = select_commits_to_reapply(candidates)
    else:
        say("  No usable base commit; no WebKit-side commits will be replayed.")

    starting_branch = current_branch(root)
    branch_name = ensure_branch(root, choose_branch_name(root, chrome_version, remote), assume_yes=True)
    if not branch_name:
        return None

    say("\n  Dry run of all copies:")
    for lib in libs:
        say(f"  {lib.key} ({lib.description}):")
        (root / lib.dest).mkdir(parents=True, exist_ok=True)
        copy_library(chromium_path, root, lib, dry_run=True)
    if not confirm("  Proceed with copying all libraries above?", default=True):
        say("  Skipping.")
        if branch_name != starting_branch:
            checkout_branch_if_safe(root, starting_branch)
        return None

    for lib in libs:
        say(f"\n  Copying {lib.key} ({lib.description})")
        source_root = chromium_path / lib.chromium_subpath
        copy_library(chromium_path, root, lib, dry_run=False)
        apply_renames(root / lib.dest, lib.renames)
        commit_copy_step(root, lib, chromium_label=str(source_root), ask=False)

    ref = f"{ALL_WEBRTC_KEY}-{sanitize_branch_component(previous_chrome_version)}"
    if confirm(f"\n  Apply WebKit commit(s) from '{ref}' on remote '{remote}'?", default=True):
        if not reapply_from_branch(root, remote, ref, ALL_WEBRTC_KEY,
                                   [lib.dest for lib in libs]):
            say(f"  Patch step did not complete; this branch is incomplete and "
                f"will not be offered for pushing.")
            return None

    if to_replay:
        say(f"\n  Replaying {len(to_replay)} commit(s) made since the last resync "
            f"(only their changes under {LIBWEBRTC_ROOT})")
        if not run_cherry_pick_interactively(root, [h for h, _ in to_replay], [LIBWEBRTC_ROOT]):
            say("  Replay did not complete; this branch is incomplete and will not be offered for pushing.")
            return None

    for lib in libs:
        if lib.notes:
            say(f"\n  Manual follow-up for {lib.key}:")
            for note in lib.notes:
                say(f"   - {note}")

    return branch_name


# ---------------------------------------------------------------------------
# update-chromium
# ---------------------------------------------------------------------------

def default_chromium_path(root):
    return root.parent / "chromium" / "src"


# FIXME: git_output() and local_branch_exists() live here but are used by code far above
# (choose_branch_name); move the small git helpers next to repo_root() at the top.
def git_output(args, cwd):
    return subprocess.run(["git"] + args, cwd=cwd, capture_output=True, text=True, check=True).stdout.strip()


def local_branch_exists(path, branch_name):
    return subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", branch_name],
        cwd=path, capture_output=True,
    ).returncode == 0


def cmd_update_chromium(args, root):
    """Fetch a Chromium release branch (refs/branch-heads/<number>), check it
    out as a local 'branch_<number>' branch, and run `gclient sync` so its
    dependencies (webrtc, sframe, ...) are up to date."""
    if not re.fullmatch(CHROME_VERSION_RE, args.chrome_version):
        say(f"The Chrome version must look like 'm155' (or 'm155-2'), got: {args.chrome_version}")
        return 1
    if not args.branch.isdigit():
        say(f"The Chromium branch must be a bare release-branch number (e.g. '7xyz'), got: {args.branch}")
        return 1

    src_path = Path(args.chromium_path or default_chromium_path(root)).expanduser().resolve()
    if not src_path.is_dir():
        say(f"No such directory: {src_path}")
        return 1
    if not (src_path / ".git").exists():
        say(f"{src_path} does not look like a git checkout (no .git found).")
        return 1

    status = subprocess.run(["git", "status", "--porcelain"], cwd=src_path,
                            capture_output=True, text=True, check=True).stdout
    if status.strip():
        say(f"{src_path} has local changes:\n{status}")
        if not confirm("Continue anyway? `git checkout` below will refuse to overwrite "
                       "modified tracked files, but review the above first.", default=False):
            say("Aborting.")
            return 1

    branch_head_ref = f"branch-heads/{args.branch}"
    local_branch = f"branch_{args.branch}"

    say(f"\n=== Fetching refs/branch-heads/{args.branch} from {args.remote} ===")
    run(["git", "fetch", args.remote, f"refs/branch-heads/{args.branch}:refs/remotes/{branch_head_ref}"],
        cwd=src_path, stream=True)

    say(f"\n=== Checking out {branch_head_ref} ===")
    run(["git", "checkout", branch_head_ref], cwd=src_path)

    if local_branch_exists(src_path, local_branch):
        say(f"\n  Local branch '{local_branch}' already exists -- checking it out as-is rather "
            f"than recreating it (it may be behind {branch_head_ref}; rebase/reset it yourself "
            f"if you want it to match exactly).")
        run(["git", "checkout", local_branch], cwd=src_path)
    else:
        say(f"\n=== Creating local branch '{local_branch}' ===")
        run(["git", "checkout", "-b", local_branch], cwd=src_path)

    checked_out = git_output(["rev-parse", "--short", "HEAD"], cwd=src_path)
    say(f"  Now on '{local_branch}' at {checked_out}.")

    say("\n=== Running gclient sync --with_branch_heads --with_tags ===")
    say("  (this can take a while the first time it pulls a new branch's deps)")
    run(["gclient", "sync", "--with_branch_heads", "--with_tags"], cwd=src_path, stream=True)

    save_state(root, chrome_version=args.chrome_version, chromium_path=str(src_path),
               chromium_branch=args.branch)
    say("\n=== Done ===")
    say(f"{src_path} is now on '{local_branch}' ({checked_out}), with dependencies synced.")
    say(f"Next: {Path(sys.argv[0]).name} resync")
    return 0


# ---------------------------------------------------------------------------
# validate: CMake file list check, WEBRTC_WEBKIT_BUILD report, build, tests
# ---------------------------------------------------------------------------

TOKEN_RE = re.compile(r'"([^"]*)"|([^\s()"]+)')
SET_RE = re.compile(r"^\s*set\(\s*(\w+)\s+([^\s()\"]+)\s*\)")
VARIABLE_RE = re.compile(r"\$\{(\w+)\}")
SOURCE_DIR_VARIABLE = "${CMAKE_CURRENT_SOURCE_DIR}/"
CMAKE_PATH_PREFIXES = ("Source/", "cmake/")

_listing_cache = {}


def strip_cmake_comment(line):
    return line.split("#", 1)[0]


def expand_cmake_variables(token, variables):
    """Expand known variables; returns None if anything unresolved remains."""
    token = token.replace(SOURCE_DIR_VARIABLE, "")
    for _ in range(5):
        if not VARIABLE_RE.search(token):
            break
        token = VARIABLE_RE.sub(lambda m: variables.get(m.group(1), m.group(0)), token)
    if "${" in token or "$<" in token or "*" in token:
        return None
    return token


def exists_case_sensitively(base, relative_path):
    current = base
    for component in Path(relative_path).parts:
        if current not in _listing_cache:
            try:
                _listing_cache[current] = set(os.listdir(current))
            except OSError:
                _listing_cache[current] = set()
        if component not in _listing_cache[current]:
            return False
        current = current / component
    return True


def find_missing_cmake_paths(cmake_file):
    """Returns (number checked, number skipped, {missing path: [line numbers]}).
    Only literal paths and `set(NAME path)` variables can be checked; paths
    built from other variables or generator expressions are skipped."""
    lines = cmake_file.read_text().splitlines()
    variables = {}
    for line in lines:
        match = SET_RE.match(strip_cmake_comment(line))
        if match and "${" not in match.group(2) and "$<" not in match.group(2):
            variables[match.group(1)] = match.group(2)

    base = cmake_file.parent
    checked = skipped = 0
    missing = {}
    _listing_cache.clear()
    for number, line in enumerate(lines, start=1):
        for match in TOKEN_RE.finditer(strip_cmake_comment(line)):
            raw = match.group(1) if match.group(1) is not None else match.group(2)
            for piece in raw.split(";"):
                path = expand_cmake_variables(piece, variables)
                if path is None:
                    if "/" in piece and ("${" in piece or "$<" in piece) \
                            and not piece.startswith(("${CMAKE_BINARY_DIR", "${CMAKE_CURRENT_BINARY_DIR")):
                        skipped += 1
                    continue
                if not path.startswith(CMAKE_PATH_PREFIXES):
                    continue
                checked += 1
                if not exists_case_sensitively(base, path):
                    missing.setdefault(path, []).append(number)
    return checked, skipped, missing


def check_cmake_files(root):
    """Every path listed in libwebrtc's CMakeLists.txt must exist in the tree,
    whatever platform or condition it is listed under. True if they all do."""
    checked, skipped, missing = find_missing_cmake_paths(root / CMAKE_FILE)
    say(f"  Checked {checked} path references in {CMAKE_FILE} "
        f"({skipped} built from unresolved variables were skipped).")
    if not missing:
        say("  Every listed path exists in the tree.")
        return True
    say(f"  {len(missing)} path(s) listed but not found in the tree:")
    for path in sorted(missing):
        say(f"    {path}   (line {', '.join(str(n) for n in missing[path])})")
    return False


def count_macro_lines(root, revision=None):
    """{path: number of lines mentioning the macro}, in `revision` or, if None,
    in the working tree (including untracked files, e.g. freshly copied ones)."""
    if revision:
        cmd = ["git", "grep", "-c", "-I", "-e", WEBKIT_BUILD_MACRO, revision, "--"] + WEBKIT_BUILD_PATHS
    else:
        cmd = ["git", "grep", "-c", "-I", "--untracked", "-e", WEBKIT_BUILD_MACRO, "--"] + WEBKIT_BUILD_PATHS
    result = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    if result.returncode not in (0, 1):
        raise RuntimeError(f"git grep failed: {result.stderr.strip()}")
    counts = {}
    prefix = f"{revision}:" if revision else ""
    for line in result.stdout.splitlines():
        path, _, count = line[len(prefix):].rpartition(":")
        counts[path] = int(count)
    return counts


# FIXME: The fork point is computed against the local `main_branch`, which may be stale and
# would make the comparison misleading. Prefer the remote's main (fetch it, or ask).
def report_webkit_build_removals(root, main_branch):
    """Report files that lost WEBRTC_WEBKIT_BUILD-guarded code compared with the
    fork point from `main_branch`: a resync overwrites files with upstream's,
    so fewer occurrences (or a vanished file) probably means WebKit-specific
    code that should be reapplied. Returns the list of findings (empty if none
    or if the check could not run)."""
    result = subprocess.run(["git", "merge-base", "HEAD", main_branch], cwd=root,
                            capture_output=True, text=True)
    base = result.stdout.strip()
    if result.returncode != 0 or not base:
        say(f"  Could not find the fork point with '{main_branch}'; skipping the {WEBKIT_BUILD_MACRO} check.")
        return []
    old_counts = count_macro_lines(root, base)
    new_counts = count_macro_lines(root)

    findings = []
    for path in sorted(old_counts):
        before, after = old_counts[path], new_counts.get(path, 0)
        if after >= before:
            continue
        if path in new_counts:
            status = "REDUCED"
        else:
            status = "STRIPPED" if (root / path).exists() else "REMOVED"
        findings.append((status, before, after, path))

    if not findings:
        say(f"  No file lost any occurrence of {WEBKIT_BUILD_MACRO} (compared with {base[:12]}).")
        return []
    say(f"  {len(findings)} file(s) lost occurrences of {WEBKIT_BUILD_MACRO} since {base[:12]} -- "
        f"check that this is intended (full diffs in {_log_path}):")
    say("    REDUCED = fewer occurrences   STRIPPED = none left   REMOVED = file no longer exists")
    for status, before, after, path in findings:
        say(f"    {status:<8} {before:>3} -> {after:<3} {path}")
    for _, _, _, path in findings:
        diff = subprocess.run(["git", "diff", base, "--", path], cwd=root, capture_output=True, text=True).stdout
        log(f"\n{'=' * 78}\n{path}\n{'=' * 78}\n{diff}")
    say(f"    (inspect one with: git diff {base[:12]} -- <path>)")
    return findings


# Ninja ("[123/4567] Building ...") and run-webkit-tests ("[12/345] test passed") progress lines.
PROGRESS_RE = re.compile(r"^\s*\[(\d+)/(\d+)\]")
PROGRESS_INTERVAL_SECONDS = 1.0
DOT_EVERY_N_LINES = 200


class ProgressDisplay:
    """Condensed progress on screen: a single updating "[done/total] N%" line on
    a terminal, or a new line every 10% otherwise (e.g. when piped)."""

    def __init__(self, label):
        self.label = label
        self.interactive = sys.stdout.isatty()
        self.last_update = 0.0
        self.last_decile = -1
        self.shown = False
        self.other_lines = 0
        self.dots_shown = False

    def update(self, line):
        match = PROGRESS_RE.match(line)
        if not match:
            # No counters (e.g. xcodebuild): a dot every DOT_EVERY_N_LINES lines of output.
            self.other_lines += 1
            if self.other_lines % DOT_EVERY_N_LINES == 0:
                print(".", end="", flush=True)
                self.dots_shown = True
            return
        done, total = int(match.group(1)), int(match.group(2))
        if not total:
            return
        percent = 100 * done // total
        if self.interactive:
            now = time.time()
            if now - self.last_update < PROGRESS_INTERVAL_SECONDS and done != total:
                return
            self.last_update = now
            print(f"\r  {self.label}: [{done}/{total}] {percent}%", end="", flush=True)
            self.shown = True
        elif percent // 10 != self.last_decile:
            self.last_decile = percent // 10
            print(f"  {self.label}: [{done}/{total}] {percent}%", flush=True)

    def finish(self):
        if self.shown or self.dots_shown:
            print(flush=True)


def run_with_progress(cmd, log_file, label, cwd=None):
    """Run a command with its full output written to `log_file`, showing
    progress on screen. Only what the failure report needs is kept in memory.
    Returns (exit status, last lines, first lines containing "error:")."""
    log_file.write(f"$ {' '.join(str(c) for c in cmd)}\n")
    log_file.flush()
    tail = collections.deque(maxlen=TAIL_LINES_ON_FAILURE)
    errors = []
    progress = ProgressDisplay(label)
    with subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, errors="replace") as process:
        for line in process.stdout:
            log_file.write(line)
            text = line.rstrip("\n")
            tail.append(text)
            if "error:" in text and len(errors) < ERROR_LINES_ON_FAILURE:
                errors.append(text)
            progress.update(line)
    status = process.returncode
    progress.finish()
    log_file.write(f"[exit status {status}]\n")
    log_file.flush()
    return status, list(tail), errors


def report_output(tail, errors, success):
    if not success:
        for line in errors:
            say(f"    {line}")
    for line in tail[-(TAIL_LINES_ON_SUCCESS if success else TAIL_LINES_ON_FAILURE):]:
        say(f"    {line}")


def extra_builds(root, options):
    """(description, command, cwd) for the optional builds besides the CMake
    one: the fuzzers, iOS simulator and iOS device (all Release). Each one not
    already requested with a command-line flag is asked about, once: the
    answer is kept for the validation retries."""
    choices = [
        ("fuzzers", "fuzzers (ASAN, libFuzzer)",
         ["make", "release", "ASAN=YES", "LIBFUZZER=YES", "SCHEME=All Fuzzers"], root),
        ("ios_simulator", "iOS simulator (Release)",
         [SCRIPTS_DIR / "build-webkit", "--release", "--ios-simulator"], None),
        ("ios_device", "iOS device (Release)",
         [SCRIPTS_DIR / "build-webkit", "--release", "--ios-device"], None),
    ]
    builds = []
    for option, description, command, cwd in choices:
        if getattr(options, option) is None:
            setattr(options, option, confirm(f"Also build {description}?", default=False))
        if getattr(options, option):
            builds.append((description, command, cwd))
    return builds


def build_and_test(root, options, build_log_path):
    """Build WebKit with CMake, then (after asking) the optional builds
    (fuzzers, iOS simulator, iOS device), then run the WebRTC layout tests.
    True only if all of them succeed; stops at the first failure."""
    configuration_flag = "--release" if options.release else "--debug"
    say(f"Full build/test output: {build_log_path}")
    with open(build_log_path, "a") as build_log:
        if not options.skip_build:
            steps = [(f"WebKit with CMake ({configuration_flag[2:]})",
                      [SCRIPTS_DIR / "build-webkit", "--cmake", configuration_flag,
                       f"--cmakeargs={CMAKE_LINKER_ARGS}"], None)]
            while steps:
                description, command, cwd = steps.pop(0)
                say(f"Building {description}...")
                started = time.time()
                status, tail, errors = run_with_progress(command, build_log, "Building", cwd=cwd)
                elapsed = int(time.time() - started)
                if status != 0:
                    say(f"BUILD OF {description.upper()} FAILED after {elapsed}s.")
                    report_output(tail, errors, success=False)
                    return False
                say(f"Built {description} in {elapsed}s.")
                if not steps and description.startswith("WebKit with CMake"):
                    steps = extra_builds(root, options)

        if options.build_only:
            say("Tests skipped (--build-only).")
            return True

        say("Running layout tests...")
        started = time.time()
        status, tail, errors = run_with_progress(
            [SCRIPTS_DIR / "run-webkit-tests", "--cmake", configuration_flag, "--no-build",
             "--no-show-results", "-f", "--child-processes=4"] + list(options.tests),
            build_log, "Testing")
        elapsed = int(time.time() - started)
        if status != 0:
            say(f"TESTS FAILED after {elapsed}s.")
            report_output(tail, errors, success=False)
            return False
        report_output(tail, errors, success=True)
        say(f"Tests passed ({elapsed}s).")
        return True


def run_validation(root, options, build_log_path):
    """One validation attempt: file lists, lost WebKit code (the user decides
    whether it is expected), build, tests. True only if all of them passed."""
    say("\n=== Validation ===")
    say("Checking the file lists in libwebrtc's CMakeLists.txt...")
    try:
        if not check_cmake_files(root):
            say("VALIDATION FAILED: CMakeLists.txt lists files missing from the tree.")
            return False
        while True:
            say(f"Looking for lost {WEBKIT_BUILD_MACRO} code...")
            if not report_webkit_build_removals(root, options.main_branch):
                break
            choice = prompt(f"  Some {WEBKIT_BUILD_MACRO} code disappeared. Check it before building: "
                            f"[r]echeck after fixing it in another shell, [c]ontinue with the build "
                            f"(the removals are expected), or [s]top", default="r").strip().lower()
            if choice in ("c", "continue"):
                break
            if choice not in ("r", "recheck", "retry"):
                say(f"VALIDATION STOPPED: lost {WEBKIT_BUILD_MACRO} code not accepted.")
                return False
    except (OSError, RuntimeError) as error:
        say(f"VALIDATION FAILED: {error}")
        return False

    if not build_and_test(root, options, build_log_path):
        say("VALIDATION FAILED.")
        return False
    say("VALIDATION PASSED.")
    return True


def push_branch(root, branch, remote):
    """Push `branch` to `remote` under the same name, asking before
    overwriting a branch that already exists there."""
    force = False
    if remote_has_branch(root, remote, branch):
        if not confirm(f"  Branch '{branch}' already exists on remote '{remote}'. "
                       f"Overwrite it with `git push -f`?", default=False):
            say(f"  Not pushing '{branch}'.")
            return False
        force = True
    cmd = ["git", "push"] + (["-f"] if force else []) + [remote, f"refs/heads/{branch}:refs/heads/{branch}"]
    if run(cmd, cwd=root, check=False).returncode != 0:
        say(f"  Push of '{branch}' failed; fix the problem and push it manually.")
        return False
    say(f"  Pushed '{branch}' to remote '{remote}'.")
    return True


def resolve_webkit_remote(root, options, state):
    """Name of the git remote, a clone of WebKit, holding the all-webrtc-<version>
    branches: --webkit-remote, else asked (suggesting the one used last time).
    Its URL is looked up from the remote. Returns None if unusable."""
    name = options.webkit_remote
    if not name:
        remotes = subprocess.run(["git", "remote", "-v"], cwd=root, capture_output=True, text=True).stdout
        say("Git remotes in this checkout:")
        for line in remotes.splitlines():
            if line.endswith("(fetch)"):
                say(f"  {line[:-len(' (fetch)')].replace(chr(9), '  ')}")
        name = prompt("Name of the git remote (a WebKit clone) to fetch the previous all-webrtc "
                      "branch from and push the new one to", default=state.get("webkit_remote"))
    if not name:
        say("A git remote name is required.")
        return None
    result = subprocess.run(["git", "remote", "get-url", name], cwd=root, capture_output=True, text=True)
    if result.returncode != 0:
        say(f"No git remote named '{name}' in {root}; add it with `git remote add {name} <url>`.")
        return None
    say(f"Using remote '{name}' ({result.stdout.strip()})")
    save_state(root, webkit_remote=name)
    return name


def validate_and_push(root, chrome_version, options, remote=None, branch=None):
    """Validate the current tree, letting the user fix it and retry, then push
    the all-webrtc-<version> branch once it is validated (or the user vouches
    for it). Never switches branches: if another branch is checked out, that
    is flagged, the validation still runs, and nothing is pushed. Returns the
    process exit status."""
    branch = branch or f"{ALL_WEBRTC_KEY}-{sanitize_branch_component(chrome_version)}"
    on_branch = current_branch(root) == branch
    if not on_branch:
        say(f"Warning: currently on '{current_branch(root)}', not '{branch}'. "
            f"Validating the current tree anyway; the branch is left alone and will not be pushed.")

    build_log_path = _log_path.with_name(f"{_log_path.stem}-build.log")
    while not run_validation(root, options, build_log_path):
        say(f"\nValidation failed for '{current_branch(root)}' (checked out at {root}).")
        say("You can fix the tree or add commits in another shell first.")
        choice = prompt("[r]etry validation, [s]kip pushing, or [o]k (results are fine, "
                        "proceed with pushing)", default="r").strip().lower()
        if choice in ("r", "retry"):
            continue
        if choice in ("o", "ok", "p", "proceed"):
            break
        say(f"Not pushing '{branch}'.")
        return 1

    if not on_branch:
        say(f"\nNot pushing '{branch}': it is not the branch that was validated.")
        return 0
    if options.no_push:
        say(f"\nNot pushing '{branch}' (--no-push).")
        return 0
    remote = remote or resolve_webkit_remote(root, options, load_state(root))
    if not remote:
        return 1
    return 0 if push_branch(root, branch, remote) else 1


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def resolve_chrome_version(root, args, state):
    """--chrome-version, else the version in the current all-webrtc-<version>
    branch name, else the one remembered by update-chromium, else ask."""
    if args.chrome_version:
        return args.chrome_version
    match = re.fullmatch(rf"{ALL_WEBRTC_KEY}-({CHROME_VERSION_RE})", current_branch(root))
    if match:
        return match.group(1)
    return prompt("Chrome version being resynced to (e.g. m155)", default=state.get("chrome_version"))


# FIXME: cmd_resync and cmd_validate both resolve the Chrome version, and the Chromium path
# prompt is awkward (args.chromium_path or state or default is computed, then prompted, then
# wrapped in Path again). Factor the "arguments, else remembered state, else ask" logic into
# one helper.
def cmd_resync(args, root):
    state = load_state(root)
    chrome_version = resolve_chrome_version(root, args, state)
    if not chrome_version:
        say("A Chrome version is required.")
        return 1

    chromium_path = Path(args.chromium_path or state.get("chromium_path") or default_chromium_path(root)).expanduser()
    chromium_path = Path(prompt("Path to the prepared Chromium checkout (src/)", default=str(chromium_path))).expanduser()
    if not chromium_path.is_dir():
        say(f"No such directory: {chromium_path}")
        return 1

    previous_versions = state.get("previous_versions", {})
    previous_chrome_version = args.previous_version or prompt(
        "Chrome version of the previous resync, whose all-webrtc branch should be reapplied (e.g. m152)",
        default=previous_versions.get(chrome_version))
    if not previous_chrome_version:
        say("A previous Chrome version is required.")
        return 1

    remote = resolve_webkit_remote(root, args, state)
    if not remote:
        return 1

    save_state(root, chrome_version=chrome_version, chromium_path=str(chromium_path),
               previous_versions={**previous_versions, chrome_version: previous_chrome_version})
    branch = resync_all_webrtc(root, chromium_path, chrome_version, previous_chrome_version, remote)
    if branch is None:
        say("\nThe resync did not complete.")
        return 1

    # The branch may carry a suffix (all-webrtc-m155-2) if m155 was resynced before.
    save_state(root, chrome_version=branch[len(ALL_WEBRTC_KEY) + 1:])
    say("\n=== Resync done ===")
    say(f"On branch '{branch}'. Remaining manual steps:")
    for note in GLOBAL_NOTES:
        say(f" - {note}")

    if args.no_validate:
        say(f"\nValidate later with: {Path(sys.argv[0]).name} validate")
        return 0
    return validate_and_push(root, chrome_version, args, remote, branch)


def cmd_validate(args, root):
    state = load_state(root)
    chrome_version = resolve_chrome_version(root, args, state)
    if not chrome_version:
        say("A Chrome version is required.")
        return 1
    return validate_and_push(root, chrome_version, args)


def add_validation_options(parser):
    parser.add_argument("--chrome-version",
                        help="Chrome version being resynced to, e.g. m155, or m155-2 to tell apart another "
                             "resync of the same version (default: from the current all-webrtc-<version> "
                             "branch, or remembered by update-chromium).")
    parser.add_argument("--release", action="store_true", help="Validate a Release build (default: Debug).")
    parser.add_argument("--skip-build", action="store_true", help="Assume WebKit is already built; only run tests.")
    parser.add_argument("--fuzzers", action="store_true", default=None,
                        help='Build the fuzzers without asking (make release ASAN=YES LIBFUZZER=YES SCHEME="All Fuzzers").')
    parser.add_argument("--ios-simulator", action="store_true", default=None,
                        help="Build for the iOS simulator (Release) without asking.")
    parser.add_argument("--ios-device", action="store_true", default=None,
                        help="Build for iOS devices (Release) without asking.")
    parser.add_argument("--build-only", action="store_true", help="Only build; do not run tests.")
    parser.add_argument("--tests", nargs="+", default=list(DEFAULT_TESTS),
                        help="Layout tests/directories to run instead of the defaults.")
    parser.add_argument("--main-branch", default=DEFAULT_MAIN_BRANCH,
                        help=f"Branch used to find where this branch forked, for the {WEBKIT_BUILD_MACRO} "
                             f"check (default: {DEFAULT_MAIN_BRANCH}).")
    parser.add_argument("--no-push", action="store_true", help="Do not push the branch after validation.")
    parser.add_argument("--webkit-remote",
                        help="Name of the git remote (a WebKit clone) to fetch the previous all-webrtc branch "
                             "from and push the new one to (default: asked, suggesting the last one used).")


def main():
    name = Path(sys.argv[0]).name
    overview = OVERVIEW.format(name=name, macro=WEBKIT_BUILD_MACRO)
    if len(sys.argv) == 1:
        print(overview)
        return 0

    parser = argparse.ArgumentParser(prog=name, description=overview,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    update = commands.add_parser(
        "update-chromium", help="Fetch a Chromium release branch and gclient sync.",
        description="Fetch a Chromium release branch into the Chromium checkout and run `gclient sync`. "
                    "This can take a long time; run it before 'resync'.")
    update.add_argument("chrome_version", help="Chrome version this branch corresponds to, e.g. 'm155'.")
    update.add_argument("branch", help="Chromium release branch number, e.g. '7xyz' (refs/branch-heads/7xyz).")
    update.add_argument("--chromium-path", help="Path to the Chromium checkout's src/ directory "
                                                "(default: ../chromium/src next to the WebKit checkout).")
    update.add_argument("--remote", default="origin", help="Git remote to fetch the branch from (default: origin).")

    resync = commands.add_parser(
        "resync", help="Resync all libraries onto all-webrtc-<version>, then validate.",
        description="Copy every library from the Chromium checkout prepared by 'update-chromium' onto "
                    "all-webrtc-<version>, reapply WebKit-side commits, replay the commits made since the last resync, "
                    "then validate.")
    resync.add_argument("--chromium-path", help="Chromium checkout's src/ directory (default: as given to update-chromium).")
    resync.add_argument("--previous-version", help="Chrome version of the previous resync, e.g. m152 "
                                                   "(default: asked).")
    resync.add_argument("--no-validate", action="store_true", help="Stop after the resync; validate later.")
    add_validation_options(resync)

    validate = commands.add_parser(
        "validate", help="Validate the current tree, then push the all-webrtc branch.",
        description="Check libwebrtc's CMakeLists.txt file lists, report lost WEBRTC_WEBKIT_BUILD code, build "
                    "WebKit with CMake (then ask about the fuzzers, iOS simulator and iOS device builds) and run the WebRTC layout tests. Push all-webrtc-<version> if it "
                    "passes (and it is the checked-out branch). Rerun after fixing the branch.")
    add_validation_options(validate)

    help_command = commands.add_parser(
        "help", help="Show this help, or the help of one command.",
        description="Show the same information as --help, or the help of one command.")
    help_command.add_argument("topic", nargs="?", choices=["update-chromium", "resync", "validate"],
                              help="Command to show the help of (default: the general help).")

    args = parser.parse_args()
    if args.command == "help":
        {"update-chromium": update, "resync": resync, "validate": validate}.get(args.topic, parser).print_help()
        return 0

    root = repo_root()
    log_path = open_log(root, args.command)
    say(f"WebKit repo root: {root}")
    say(f"Full log: {log_path}")

    handlers = {"update-chromium": cmd_update_chromium, "resync": cmd_resync, "validate": cmd_validate}
    try:
        return handlers[args.command](args, root)
    except subprocess.CalledProcessError as error:
        say(f"\nCommand failed (exit status {error.returncode}): "
            f"{' '.join(str(c) for c in error.cmd)}")
        return 1
    except (KeyboardInterrupt, EOFError):
        say("\nInterrupted.")
        return 130


if __name__ == "__main__":
    sys.exit(main())
