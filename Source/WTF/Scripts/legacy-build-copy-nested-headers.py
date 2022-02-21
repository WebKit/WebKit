#!/usr/bin/env python3
"""
Xcode's legacy build system lacks a way to create *nested* header directories, i.e. copying headers to a build product
while preserving the directory hierarchy of those headers. This script compensates by copying headers from a "flattened"
directory (denoted by ${HEADER_OUTPUT_DIR}) to a "nested" directory hierarchy (starting at
${WK_NESTED_HEADER_OUTPUT_DIR}).
"""

import os
import shutil
import sys
from pathlib import Path

if os.environ.get('WK_USE_NEW_BUILD_SYSTEM') == 'YES':
    # XCBuild uses build rules to do this natively; this script is for backwards
    # compatibility only.
    sys.exit(0)

flattened_headers_folder = Path('{HEADER_OUTPUT_DIR}'.format_map(os.environ))
nested_headers_folder = Path('{WK_NESTED_HEADER_OUTPUT_DIR}'.format_map(os.environ))
header_srcroot = Path('{WK_HEADER_SRCROOT}'.format_map(os.environ))

for header in header_srcroot.rglob('*.h'):
    flattened = flattened_headers_folder / header.name
    nested = nested_headers_folder / header.relative_to(header_srcroot)
    if not flattened.exists():
        # This header exists in the project's sources, but isn't being copied by
        # Xcode. Skip it.
        continue
    if nested.exists() and os.path.getmtime(nested) >= os.path.getmtime(header):
        # This header has already been copied. Skip it.
        continue

    # Copy the header out of SRCROOT, *NOT* the flattened headers folder.
    # Headers with the same basename might have overwritten each other in the
    # flattened directory, so there's no guarantee that a flattened header is
    # actually the header we want.
    print(header, '->', nested)
    nested.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(header, nested)
