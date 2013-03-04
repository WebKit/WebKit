#!/usr/bin/env python
#
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import os.path
import sys


def generate_include_tag(resource_path):
    (dir_name, file_name) = os.path.split(resource_path)
    if (file_name.endswith('.js')):
        return '    <script type="text/javascript" src="%s"></script>\n' % file_name
    elif (file_name.endswith('.css')):
        return '    <link rel="stylesheet" type="text/css" href="%s">\n' % file_name
    else:
        assert resource_path


def write_devtools_html(inspector_file, devtools_file, debug, debug_files):
    for line in inspector_file:
        if not debug and '<script ' in line:
            continue
        if not debug and '<link ' in line:
            continue
        if '</head>' in line:
            if debug:
                for resource in debug_files:
                    devtools_file.write(generate_include_tag(resource))
            else:
                devtools_file.write(generate_include_tag("devTools.css"))
                devtools_file.write(generate_include_tag("DevTools.js"))
        devtools_file.write(line)
        if '<head>' in line:
            devtools_file.write(generate_include_tag("buildSystemOnly.js"))


def main(argv):

    if len(argv) < 4:
        print('usage: %s inspector_html devtools_html debug'
              ' css_and_js_files_list' % argv[0])
        return 1

    # The first argument is ignored. We put 'webkit.gyp' in the inputs list
    # for this script, so every time the list of script gets changed, our html
    # file is rebuilt.
    inspector_html_name = argv[1]
    devtools_html_name = argv[2]
    debug = argv[3] != '0'
    inspector_html = open(inspector_html_name, 'r')
    devtools_html = open(devtools_html_name, 'w')

    write_devtools_html(inspector_html, devtools_html, debug, argv[4:])

    devtools_html.close()
    inspector_html.close()

    # Touch output file directory to make sure that Xcode will copy
    # modified resource files.
    if sys.platform == 'darwin':
        output_dir_name = os.path.dirname(devtools_html_name)
        os.utime(output_dir_name, None)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
