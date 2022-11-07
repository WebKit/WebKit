#!/usr/bin/env python3
#
# Copyright (C) 2022 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
import subprocess
import sys

API_SINGLE_HEADER_CHECK = {
    "gtk4": '''#if !defined(__WEBKIT_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit/webkit.h> can be included directly.\"
#endif''',
    "gtk": '''#if !defined(__WEBKIT2_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit2/webkit2.h> can be included directly.\"
#endif''',
    "wpe": '''#if !defined(__WEBKIT_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <wpe/webkit.h> can be included directly.\"
#endif'''
}

INJECTED_BUNDLE_API_SINGLE_HEADER_CHECK = {
    "gtk4": '''#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit/webkit-web-extension.h> can be included directly.\"
#endif''',
    "gtk": '''#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit2/webkit-web-extension.h> can be included directly.\"
#endif''',
    "wpe": '''#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <wpe/webkit-web-extension.h> can be included directly.\"
#endif'''
}

SHARED_API_SINGLE_HEADER_CHECK = {
    "gtk4": '''#if !defined(__WEBKIT_H_INSIDE__) && !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit/webkit.h> can be included directly.\"
#endif''',
    "gtk": '''#if !defined(__WEBKIT2_H_INSIDE__) && !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <webkit2/webkit2.h> can be included directly.\"
#endif''',
    "wpe": '''#if !defined(__WEBKIT_H_INSIDE__) && !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error \"Only <wpe/webkit.h> can be included directly.\"
#endif'''
}

API_INCLUDE_PREFIX = {
    "gtk4": "webkit",
    "gtk": "webkit",
    "wpe": "wpe"
}


def expand_ifdefs(line):
    s = re.sub(r"(ENABLE|USE)\(([A-Z0-9_]+)\)", r"(defined(\1_\2) && \1_\2)", line)
    return re.sub(r"(PLATFORM)\(([A-Z0-9_]+)\)", r"(defined(WTF_\1_\2) && WTF_\1_\2)", s)


def main(args):
    port = args[0].lower()
    input = args[1]
    output = args[2]
    unifdef = args[3]
    unifdef_args = args[4:]

    if port == "gtk" and "-DUSE_GTK4=1" in unifdef_args:
        port = "gtk4"

    input_data = ""
    with open(input, "r", encoding="utf-8") as fd:
        for line in fd.readlines():
            if line[0] == "@":
                if line == "@API_SINGLE_HEADER_CHECK@\n":
                    input_data += API_SINGLE_HEADER_CHECK[port] + "\n"
                elif line == "@INJECTED_BUNDLE_API_SINGLE_HEADER_CHECK@\n":
                    input_data += INJECTED_BUNDLE_API_SINGLE_HEADER_CHECK[port] + "\n"
                elif line == "@SHARED_API_SINGLE_HEADER_CHECK@\n":
                    input_data += SHARED_API_SINGLE_HEADER_CHECK[port] + "\n"
            elif line.startswith("#include <@"):
                input_data += line.replace("@API_INCLUDE_PREFIX@", API_INCLUDE_PREFIX[port])
            else:
                input_data += expand_ifdefs(line)

    command = [unifdef, "-x1", "-o%s" % output] + unifdef_args + ["-"]
    p = subprocess.Popen(command, stdin=subprocess.PIPE, text=True, encoding="utf-8")
    p.communicate(input_data)

    # unifdef returns 1 when no changes were made and output file is unmodified.
    if p.returncode != 0:
        with open(output, "w", encoding="utf-8") as fd:
            fd.write(input_data)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
