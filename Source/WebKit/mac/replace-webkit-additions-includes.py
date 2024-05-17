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

import os
import re
import sys


# Enable this if `FeatureNeededForHeaderReplacement` should be taken into account.
should_restrict_header_replacement_based_on_feature = True


def read_content_from_webkit_additions(built_products_directory, sdk_root_directory, filename):
    additions_path = os.path.join("usr/local/include/WebKitAdditions", filename)
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
    if not argv:
        argv = sys.argv

    if len(argv) != 3:
        print("Usage: replace-webkit-additions-includes.py <built_products_directory> <sdk_root_directory>", file=sys.stderr)
        return 1

    built_products_directory = argv[1]
    sdk_root_directory = argv[2]

    if not len(built_products_directory):
        print("(%s): built products directory unspecified" % argv[0], file=sys.stderr)
        return 1

    if not len(sdk_root_directory):
        print("(%s): SDK root directory unspecified" % argv[0], file=sys.stderr)
        return 1

    should_do_replacement = check_should_do_replacement(built_products_directory, sdk_root_directory)

    additions_import_pattern = re.compile(r"\#if 0 // API_WEBKIT_ADDITIONS_REPLACEMENT\n#import <WebKitAdditions/(.*)>\n#endif")
    header_contents = sys.stdin.read()
    match = additions_import_pattern.search(header_contents)
    while match:
        if should_do_replacement:
            header_contents = header_contents[:match.start()] + read_content_from_webkit_additions(built_products_directory, sdk_root_directory, match.groups()[0]) + header_contents[match.end():]
        else:
            header_contents = header_contents[:match.start()] + header_contents[match.end():]
        match = additions_import_pattern.search(header_contents)
    sys.stdout.write(header_contents)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
