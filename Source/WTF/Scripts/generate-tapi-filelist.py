#!/usr/bin/env python3

from fnmatch import fnmatch
import argparse
import json
import os
import sys

parser = argparse.ArgumentParser(usage='Run from an Xcode build phase to generate a tapi-installapi(1) compatible file list from XCBuild data.')
parser.add_argument(
    '--from',
    dest='source_headers',
    required=True,
    help="Path to a header VFS file (all-product-headers.yaml) OR a directory containing headers. Prefer using a VFS "
    "file when the headers directory may contain stale headers from previous builds."
)
parser.add_argument(
    '--install-dir',
    required=True,
    help="Directory that this target's headers will be installed to at the time tapi is run."
)
parser.add_argument(
    '--relative-to',
    help="Instead of assuming flattened header paths inside of install_dir, compute paths from the header's source "
    "location relative to this one."
)
opts = parser.parse_args()
excluded_source_file_patterns = os.environ.get('EXCLUDED_SOURCE_FILE_NAMES', '').split()

headers = []
filelist = {'headers': [], 'version': '3'}

# Assume source_headers is an all-product-headers.yaml VFS file. In installhdrs
# builds, all-product-headers.yaml is not available, so walk the headers directory and prepare a file list
# using the headers that were just installed.
try:
    vfs_fd = open(opts.source_headers)
except IsADirectoryError:
    for dirpath, dirnames, filenames in os.walk(opts.source_headers):
        headers.extend((name, os.path.join(dirpath, name)) for name in filenames)
    # all-product-headers.yaml includes headers in the order they appear in the Xcode project, not in
    # lexicographic order. But bmalloc and WTF's headers *are* declared in lexicographic order.
    headers.sort()
else:
    # By convention, all-product-headers.yaml is written in the JSON-compatible subset of YAML.
    # Since PyYAML is not installed by default (rdar://107338320), first try parsing the file as
    # JSON, and fall back to PyYAML as needed.
    try:
        product_headers = json.load(vfs_fd)
    except json.JSONDecodeError:
        import yaml
        vfs_fd.seek(0)
        product_headers = yaml.safe_load(vfs_fd)
    for root in product_headers['roots']:
        headers.extend((entry['name'], entry['external-contents']) for entry in root['contents'])

for name, source_path in headers:
    if not name.endswith('.h') or any(fnmatch(source_path, f'*{pattern}') for pattern in excluded_source_file_patterns):
        continue
    if opts.relative_to:
        relative_source_path = os.path.relpath(os.path.realpath(source_path), os.path.realpath(opts.relative_to))
        assert not relative_source_path.startswith('../'), \
            f"The header at {os.path.realpath(source_path)} is not relative to {os.path.realpath(opts.relative_to)}, bailing out to avoid unexpected paths in the TAPI filelist"
        installed_path = os.path.normpath(os.path.join(opts.install_dir, relative_source_path))
    else:
        installed_path = os.path.join(opts.install_dir, name)
    filelist['headers'].append({
        'path': installed_path,
        'type': 'private'
    })

json.dump(filelist, sys.stdout, indent=2)
