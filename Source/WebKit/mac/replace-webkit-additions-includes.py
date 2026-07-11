#!/usr/bin/env python3
#
# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#

import argparse
import os
import re
import sys


# Enable this if `FeatureNeededForHeaderReplacement` should be taken into account.
should_restrict_header_replacement_based_on_feature = True


def read_content_from_webkit_additions(built_products_directory, sdk_root_directory, filename):
    library_headers_folder_path = os.environ.get('WK_LIBRARY_HEADERS_FOLDER_PATH', '').removeprefix('/')
    additions_path = os.path.join(library_headers_folder_path, "WebKitAdditions", filename)
    try:
        file_in_build_directory = open(os.path.join(built_products_directory, additions_path), "r")
        return file_in_build_directory.read()
    except Exception as ex:
        try:
            file_in_sdk_root = open(os.path.join(sdk_root_directory, additions_path), "r")
            return file_in_sdk_root.read()
        except Exception as ex:
            return ""


def check_should_do_replacement(built_products_directory, sdk_root_directory):
    if not should_restrict_header_replacement_based_on_feature:
        return True
    feature_name = read_content_from_webkit_additions(built_products_directory, sdk_root_directory, 'FeatureNeededForHeaderReplacement').strip()
    return os.environ.get(feature_name) == '1'


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('built_products_dir')
    parser.add_argument('sdk_root_dir')
    parser.add_argument('src', nargs='?')
    parser.add_argument('dst', nargs='?')
    args = parser.parse_args(argv)

    if not args.src:
        src = sys.stdin
    else:
        src = open(args.src, 'r')

    if not args.dst:
        dst = sys.stdout
    else:
        if os.path.exists(args.dst) and os.path.islink(args.dst):
            # Migrate from a CMake build that used symlinks to source headers.
            os.unlink(args.dst)
        dst = open(args.dst, 'w')

    built_products_directory = args.built_products_dir
    sdk_root_directory = args.sdk_root_dir

    should_do_replacement = check_should_do_replacement(built_products_directory, sdk_root_directory)

    additions_import_pattern = re.compile(r"\#if 0 // API_WEBKIT_ADDITIONS_REPLACEMENT\n#import <WebKitAdditions/(.*)>\n#endif")
    header_contents = src.read()
    match = additions_import_pattern.search(header_contents)
    while match:
        if should_do_replacement:
            header_contents = header_contents[:match.start()] + read_content_from_webkit_additions(built_products_directory, sdk_root_directory, match.groups()[0]) + header_contents[match.end():]
        else:
            header_contents = header_contents[:match.start()] + header_contents[match.end():]
        match = additions_import_pattern.search(header_contents)
    dst.write(header_contents)

if __name__ == "__main__":
    main()
