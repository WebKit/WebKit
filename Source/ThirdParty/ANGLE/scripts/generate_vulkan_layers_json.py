#!/usr/bin/env python
#
# Copyright 2016 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate copies of the Vulkan layers JSON files, with no paths, forcing
Vulkan to use the default search path to look for layers."""

from __future__ import print_function

import argparse
import glob
import json
import os
import platform
import sys


def glob_slash(dirname):
    """Like regular glob but replaces \ with / in returned paths."""
    return [s.replace('\\', '/') for s in glob.glob(dirname)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--icd', action='store_true')
    parser.add_argument('source_dir')
    parser.add_argument('target_dir')
    parser.add_argument('version_header', help='path to vulkan_core.h')
    parser.add_argument('json_files', nargs='*')
    args = parser.parse_args()

    source_dir = args.source_dir
    target_dir = args.target_dir

    json_files = [j for j in args.json_files if j.endswith('.json')]
    json_in_files = [j for j in args.json_files if j.endswith('.json.in')]

    data_key = 'ICD' if args.icd else 'layer'

    if not os.path.isdir(source_dir):
        print(source_dir + ' is not a directory.', file=sys.stderr)
        return 1

    if not os.path.exists(target_dir):
        os.makedirs(target_dir)

    # Copy the *.json files from source dir to target dir
    if (set(glob_slash(os.path.join(source_dir, '*.json'))) != set(json_files)):
        print(glob.glob(os.path.join(source_dir, '*.json')))
        print('.json list in gn file is out-of-date', file=sys.stderr)
        return 1

    for json_fname in json_files:
        if not json_fname.endswith('.json'):
            continue
        with open(json_fname) as infile:
            data = json.load(infile)

        # Update the path.
        if not data_key in data:
            raise Exception(
                "Could not find '%s' key in %s" % (data_key, json_fname))

        # The standard validation layer has no library path.
        if 'library_path' in data[data_key]:
            prev_name = os.path.basename(data[data_key]['library_path'])
            data[data_key]['library_path'] = prev_name

        target_fname = os.path.join(target_dir, os.path.basename(json_fname))
        with open(target_fname, 'wb') as outfile:
            json.dump(data, outfile)

    # Get the Vulkan version from the vulkan_core.h file
    vk_header_filename = args.version_header
    vk_version = None
    with open(vk_header_filename) as vk_header_file:
        for line in vk_header_file:
            if line.startswith('#define VK_HEADER_VERSION'):
                vk_version = line.split()[-1]
                break
    if not vk_version:
        print('failed to extract vk_version', file=sys.stderr)
        return 1

    # Set json file prefix and suffix for generating files, default to Linux.
    relative_path_prefix = '../lib'
    file_type_suffix = '.so'
    if platform.system() == 'Windows':
        relative_path_prefix = r'..\\'  # json-escaped, hence two backslashes.
        file_type_suffix = '.dll'

    # For each *.json.in template files in source dir generate actual json file
    # in target dir
    if (set(glob_slash(os.path.join(source_dir, '*.json.in'))) !=
            set(json_in_files)):
        print('.json.in list in gn file is out-of-date', file=sys.stderr)
        return 1
    for json_in_name in json_in_files:
        if not json_in_name.endswith('.json.in'):
            continue
        json_in_fname = os.path.basename(json_in_name)
        layer_name = json_in_fname[:-len('.json.in')]
        layer_lib_name = layer_name + file_type_suffix
        json_out_fname = os.path.join(target_dir, json_in_fname[:-len('.in')])
        with open(json_out_fname,'w') as json_out_file, \
             open(json_in_name) as infile:
            for line in infile:
                line = line.replace('@RELATIVE_LAYER_BINARY@',
                                    relative_path_prefix + layer_lib_name)
                line = line.replace('@VK_VERSION@', '1.1.' + vk_version)
                json_out_file.write(line)

if __name__ == '__main__':
    sys.exit(main())
