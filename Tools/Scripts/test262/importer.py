#!/usr/bin/env python3

# Copyright (C) 2018 Bocoup LLC. All rights reserved.
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

"""Test262 importer for WebKit's JavaScriptCore.

Imports the tc39/test262 suite into JSTests/test262, either from a local
checkout (--src) or by shallow-cloning a remote (--remote/--branch). After
copying the harness/ and test/ trees into place it records the imported
revision in test262-Revision.txt and a summary of the changed files in
latest-changes-summary.txt (which the runner's --latest-import mode reads).
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import List, NoReturn, Optional, Tuple

# --- Path Constants ---

_MODULE_DIR = Path(__file__).resolve().parent
_SCRIPTS_DIR = _MODULE_DIR.parent
_WEBKIT_ROOT = _SCRIPTS_DIR.parent.parent
_TEST262_DIR = str(_WEBKIT_ROOT / "JSTests" / "test262")
_REVISION_FILE = str(_WEBKIT_ROOT / "JSTests" / "test262" / "test262-Revision.txt")
_SUMMARY_FILE = str(_WEBKIT_ROOT / "JSTests" / "test262" / "latest-changes-summary.txt")

_DEFAULT_REMOTE = "https://github.com/tc39/test262.git"
_DEFAULT_BRANCH = "main"

_REVISION_RE = re.compile(r"test262 revision: (\w*)")


# --- Utilities ---


def _die(msg: str) -> NoReturn:
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(1)


def _git(args: List[str], cwd: Optional[str] = None) -> str:
    """Run git and return its stripped stdout, dying on failure."""
    try:
        proc = subprocess.run(
            ["git", *args],
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except FileNotFoundError:
        _die("git executable not found on PATH.")
    if proc.returncode != 0:
        _die(f"git {' '.join(args)} failed: {(proc.stderr or '').strip()}")
    return proc.stdout.strip()


# --- Revision Tracking ---


def _read_revision(revision_file: str) -> Optional[str]:
    """Return the last imported revision recorded in test262-Revision.txt, or
    None if the file is absent or does not name a revision (e.g. a first
    import)."""
    if not os.path.exists(revision_file):
        return None
    with open(revision_file, "r", encoding="utf-8") as f:
        contents = f.read()
    m = _REVISION_RE.search(contents)
    return m.group(1) if m else None


def _get_new_revision(source_dir: str) -> Tuple[str, str]:
    """Return (revision, tracking_url) describing source_dir's checked-out HEAD."""
    tracking = _git(["-C", source_dir, "ls-remote", "--get-url"])
    branch = _git(["-C", source_dir, "rev-parse", "--abbrev-ref", "HEAD"])
    revision = _git(["-C", source_dir, "rev-parse", "HEAD"])

    print(f"New tracking: {tracking}")
    print(f"From branch: {branch}")
    print(f"New revision: {revision}\n")

    if not revision or not tracking or not branch:
        _die("Something is wrong in the source git.")

    return revision, tracking


def _write_revision(revision_file: str, revision: str, tracking: str) -> None:
    with open(revision_file, "w", encoding="utf-8") as f:
        f.write(f"test262 remote url: {tracking}\n")
        f.write(f"test262 revision: {revision}\n")


# --- Change Summary ---


def _normalize_summary(raw: str, test262_dir: str, source_dir: str) -> str:
    """Strip the absolute test262/source path prefixes from git's --name-status
    output, leaving `<status> <relative path>` lines."""
    summary = raw.rstrip("\n\r")
    summary = re.sub(r"\s+" + re.escape(test262_dir) + "/", " ", summary)
    summary = re.sub(r"\s+" + re.escape(source_dir) + "/", " ", summary)
    return summary


def _get_summary(test262_dir: str, source_dir: str) -> str:
    """Diff the incoming harness/ and test/ trees against the current ones and
    return a normalized `<status> <path>` summary of added/deleted/renamed/
    modified files."""
    raw = ""
    for subdir in ("harness", "test"):
        # --no-index makes git diff two arbitrary trees; it exits non-zero when
        # they differ, which is the expected case here, so the status is ignored.
        proc = subprocess.run(
            [
                "git", "diff", "--no-index", "--name-status",
                "--diff-filter=ADRM", "--",
                os.path.join(test262_dir, subdir),
                os.path.join(source_dir, subdir),
            ],
            stdout=subprocess.PIPE,
            text=True,
        )
        raw += proc.stdout
    return _normalize_summary(raw, test262_dir, source_dir)


def _write_summary(summary_file: str, summary: str) -> None:
    with open(summary_file, "w", encoding="utf-8") as f:
        f.write(summary)


# --- File Transfer ---


def _transfer_files(test262_dir: str, source_dir: str) -> None:
    for subdir in ("harness", "test"):
        dest = os.path.join(test262_dir, subdir)
        if os.path.exists(dest):
            print(f"Removing {dest}")
            shutil.rmtree(dest)
        src = os.path.join(source_dir, subdir)
        print(f"Moving {src} -> {dest}")
        shutil.move(src, dest)


# --- Remote Cloning ---


def _clone_remote(remote_url: str, branch: str) -> str:
    """Shallow-clone remote_url@branch into a fresh temp directory and return it.
    The caller is responsible for removing the directory when done."""
    source_dir = tempfile.mkdtemp(prefix="test262-import-")
    print("Importing Test262 from git")
    cmd = ["git", "clone", "-b", branch, "--depth=1", remote_url, source_dir]
    print(f"> {' '.join(cmd)}\n")
    proc = subprocess.run(cmd)
    if proc.returncode != 0:
        shutil.rmtree(source_dir, ignore_errors=True)
        _die(f"Failed to clone {remote_url} (branch {branch}).")
    return source_dir


# --- Source Validation ---


def _validate_source_dir(source_dir: str, test262_dir: str) -> None:
    if not (
        os.path.isdir(source_dir)
        and os.path.isdir(os.path.join(source_dir, ".git"))
        and os.path.isdir(os.path.join(source_dir, "test"))
        and os.path.isdir(os.path.join(source_dir, "harness"))
    ):
        _die(f"{source_dir} does not exist or is not a valid Test262 folder.")

    if os.path.abspath(source_dir) == os.path.abspath(test262_dir):
        _die(f"{source_dir} cannot be the same as the current Test262 folder.")


# --- CLI ---


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import the tc39/test262 suite into JSTests/test262.",
        epilog=(
            "Examples:\n"
            "  test262-import\n"
            "  test262-import -s ../test262\n"
            "  test262-import -r https://github.com/tc39/test262\n"
            "  test262-import -r https://github.com/tc39/test262 -b es6\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    location = parser.add_mutually_exclusive_group()
    location.add_argument(
        "-s", "--src", help="Path to a local Test262 repository to import from."
    )
    location.add_argument(
        "-r",
        "--remote",
        help=f"Remote Test262 repository to clone. Defaults to {_DEFAULT_REMOTE}.",
    )
    parser.add_argument(
        "-b",
        "--branch",
        help=f"Branch to clone from a remote Test262. Defaults to '{_DEFAULT_BRANCH}'.",
    )
    return parser.parse_args()


# --- Main ---


def main() -> None:
    args = _parse_args()
    start_time = time.monotonic()

    source_dir: Optional[str] = os.path.abspath(args.src) if args.src else None
    remote_url: Optional[str] = args.remote
    branch = args.branch

    # A local checkout and a remote are mutually exclusive; argparse enforces
    # that. With neither, default to cloning the canonical upstream repository.
    if source_dir:
        _validate_source_dir(source_dir, _TEST262_DIR)
    else:
        remote_url = remote_url or _DEFAULT_REMOTE
        branch = branch or _DEFAULT_BRANCH

    print("Settings:")
    if source_dir:
        print(f"Source: {source_dir}")
    if remote_url:
        print(f"Remote: {remote_url}")
        print(f"Branch: {branch}")
    print("--------------------------------------------------------\n")

    if remote_url:
        source_dir = _clone_remote(remote_url, branch)

    assert source_dir is not None

    try:
        revision = _read_revision(_REVISION_FILE)

        if not remote_url:
            dirty = _git(["-C", source_dir, "status", "--porcelain"])
            if dirty:
                _die(f"Test262 at {source_dir} has unstaged/uncommitted changes.")

        new_revision, new_tracking = _get_new_revision(source_dir)
        if revision is not None and new_revision == revision:
            print("Same revision, no need to import.")
            return

        summary = _get_summary(_TEST262_DIR, source_dir)
        if summary:
            print(f"Summary of changes:\n{summary}\n")

        _transfer_files(_TEST262_DIR, source_dir)
        _write_revision(_REVISION_FILE, new_revision, new_tracking)
        _write_summary(_SUMMARY_FILE, summary)
    finally:
        if remote_url:
            shutil.rmtree(source_dir, ignore_errors=True)

    elapsed = time.monotonic() - start_time
    print(f"\nDone in {elapsed:.2f} seconds!")


if __name__ == "__main__":
    main()
