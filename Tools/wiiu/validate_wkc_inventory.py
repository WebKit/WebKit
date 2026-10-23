#!/usr/bin/env python3
"""Inventory WKC platform trees and verify that the Wii U build accounts for them.

This is an audit, not a blanket compile-all switch: obsolete adapters that target
removed WebKit APIs must remain explicitly excluded until ported to the modern API.
Every WKC source tree is listed in CI so new/unaccounted roots cannot silently go
unnoticed.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Source"
CMAKE = ROOT / "Source/WebKit/WKC/CMakeLists.txt"

# The selectors below are the source-selection rules in the Wii U CMake build.
# Keep this table aligned with CMakeLists.txt; paths are repository-relative.
SELECTORS = {
    "Source/WebCore/platform/WKC": ("platform/WKC/*.cpp",),
    "Source/WebCore/platform/graphics/WKC": ("platform/graphics/WKC/*.cpp",),
    "Source/WebCore/platform/sql/WKC": ("platform/sql/WKC/*.cpp",),
    "Source/WebCore/platform/text/WKC": ("platform/text/WKC/*.cpp",),
    "Source/WebCore/platform/audio/WKC": ("platform/audio/WKC/*.cpp",),
    "Source/WebCore/platform/mediastream/WKC": ("platform/mediastream/WKC/*.cpp",),
    "Source/WebCore/platform/network/wkc": ("platform/network/wkc/*.cpp",),
    "Source/WebKit/WKC": ("webkit/*.cpp", "WebCoreSupport/*.cpp", "helpers/*.cpp", "accessor/*.cpp"),
}

def fail(message):
    print(f"::error::{message}" if "GITHUB_ACTIONS" in __import__("os").environ else f"ERROR: {message}")
    return 1

def main():
    if not CMAKE.is_file():
        return fail(f"Missing WKC build definition: {CMAKE.relative_to(ROOT)}")

    cmake = CMAKE.read_text(encoding="utf-8")
    discovered = []
    for path in SOURCE.rglob("*"):
        if path.is_dir() and path.name.casefold() == "wkc":
            rel = path.relative_to(ROOT).as_posix()
            # Ignore nested WKC directories when a known parent WKC tree already
            # owns them; inventory roots are the top-level WKC platform trees.
            if any(rel.startswith(parent + "/") for parent in discovered):
                continue
            discovered.append(rel)

    errors = []
    print(f"WKC inventory: {len(discovered)} root directories")
    for rel in sorted(discovered):
        print(f"\n[{rel}]")
        base = ROOT / rel
        files = sorted(p for p in base.rglob("*") if p.is_file())
        print(f"  files: {len(files)}")
        for file in files:
            print(f"  {file.relative_to(ROOT).as_posix()}")

        selectors = SELECTORS.get(rel)
        if selectors is None:
            errors.append(f"No source-selection audit rule for WKC root: {rel}")
            continue
        for selector in selectors:
            if selector not in cmake:
                errors.append(f"{rel}: expected CMake source selector is missing: {selector}")

    # This legacy adapter is intentionally not compiled: it targets the removed
    # GraphicsContext3D API. WebGL must use the modern GraphicsContextGL backend.
    legacy = "Source/WebCore/platform/graphics/WKC/GraphicsContext3DWKC.cpp"
    if (ROOT / legacy).is_file():
        if legacy not in cmake or "list(REMOVE_ITEM WEBCORE_PLATFORM_SOURCES" not in cmake:
            errors.append(f"{legacy}: legacy exclusion must remain explicit in CMake")
        else:
            print(f"\n[explicitly excluded legacy adapter] {legacy}")

    if errors:
        for error in errors:
            print(f"::error::{error}" if "GITHUB_ACTIONS" in __import__("os").environ else f"ERROR: {error}")
        return 1
    print("\nWKC inventory audit passed: every discovered root has an explicit build-selection rule.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
