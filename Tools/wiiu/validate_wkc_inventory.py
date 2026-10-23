#!/usr/bin/env python3
"""Inventory every tracked WKC tree and audit Wii U source-selection coverage.

Use Git's tree rather than the working directory so sparse checkout cannot hide
legacy WKC trees elsewhere in Source/. This intentionally reports old adapters as
gaps until they are either ported into the modern build or explicitly dispositioned.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CMAKE = ROOT / "Source/WebKit/WKC/CMakeLists.txt"

# CMake selectors currently used by the Wii U build. Extend this map only after
# verifying how a tree is handled; do not blanket-compile old API implementations.
SELECTORS = {
    # Reviewed legacy trees are intentionally excluded from the modern WebCore
    # target; CMake documents their disposition and PORTING_STATUS.md tracks ports.
    "Source/WebCore/html/shadow/WKC": (),
    "Source/WebCore/loader/archive/mhtml/WKC": (),
    "Source/WebCore/page/WKC": (),
    "Source/WebCore/platform/WKC": ("platform/WKC/*.cpp",),
    "Source/WebCore/platform/graphics/WKC": ("platform/graphics/WKC/*.cpp",),
    "Source/WebCore/platform/sql/WKC": ("platform/sql/WKC/*.cpp",),
    "Source/WebCore/platform/text/WKC": ("platform/text/WKC/*.cpp",),
    "Source/WebCore/platform/audio/WKC": ("platform/audio/WKC/*.cpp",),
    "Source/WebCore/platform/mediastream/WKC": ("platform/mediastream/WKC/*.cpp",),
    "Source/WebCore/platform/network/wkc": ("platform/network/wkc/*.cpp",),
    "Source/WebKit/WKC": ("webkit/*.cpp", "WebCoreSupport/*.cpp", "helpers/*.cpp", "accessor/*.cpp"),
}

def error(message):
    prefix = "::error::" if os.environ.get("GITHUB_ACTIONS") else "ERROR: "
    print(f"{prefix}{message}")

def main():
    if not CMAKE.is_file():
        error(f"Missing WKC build definition: {CMAKE.relative_to(ROOT)}")
        return 1

    result = subprocess.run(
        ["git", "-c", f"safe.directory={ROOT}", "ls-tree", "-r", "--name-only", "HEAD", "--", "Source"],
        cwd=ROOT, check=False, capture_output=True, text=True, encoding="utf-8",
    )
    if result.returncode:
        # actions/checkout uses sparse checkout here. In partial clones Git may
        # be unable to walk every tree object even though the complete tracked
        # path index is available. Fall back to the index, not the worktree, so
        # excluded WKC directories are still included in the inventory.
        result = subprocess.run(
            ["git", "-c", f"safe.directory={ROOT}", "ls-files", "--cached", "--full-name", "--", "Source"],
            cwd=ROOT, check=False, capture_output=True, text=True, encoding="utf-8",
        )
        if result.returncode:
            error("Cannot enumerate tracked Source files from either Git tree or index: "
                + result.stderr.strip())
            return 1
        print("Git tree walk unavailable; using the complete tracked index for sparse-checkout inventory.")
    tracked_files = sorted(line for line in result.stdout.splitlines() if line)
    if not tracked_files:
        error("Tracked Source inventory is empty; refusing to treat the WKC audit as passed.")
        return 1
    wkc_roots = set()
    for file_path in tracked_files:
        parts = file_path.split("/")
        for index, part in enumerate(parts[:-1]):
            if part.casefold() == "wkc":
                wkc_roots.add("/".join(parts[:index + 1]))

    # Keep only top-level WKC roots; any nested WKC directory belongs to its
    # ancestor's complete inventory and must not be counted as a separate root.
    roots = sorted(
        candidate for candidate in wkc_roots
        if not any(candidate.startswith(parent + "/") for parent in wkc_roots if parent != candidate)
    )
    cmake = CMAKE.read_text(encoding="utf-8")
    errors = []
    print(f"Tracked WKC inventory: {len(roots)} root directories across the full Git tree")
    for root in roots:
        files = [path for path in tracked_files if path.startswith(root + "/")]
        print(f"\n[{root}] ({len(files)} tracked files)")
        for path in files:
            print(f"  {path}")

        selectors = SELECTORS.get(root)
        if selectors is None:
            errors.append(f"No reviewed source-selection/disposition rule for WKC root: {root}")
            continue
        for selector in selectors:
            if selector not in cmake:
                errors.append(f"{root}: expected CMake source selector is missing: {selector}")

    if errors:
        for message in errors:
            error(message)
        print(f"WKC inventory audit failed: {len(errors)} unreviewed or mismatched item(s).")
        return 1

    print("\nWKC inventory audit passed: every tracked root has an explicit build-selection rule.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
