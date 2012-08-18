#!/usr/bin/env python
#
# Copyright (C) 2011 Google Inc. All rights reserved.
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

# This script replaces calls to importScripts with script sources
# in input script file and dumps result into output script file.

from cStringIO import StringIO

import jsmin
import os.path
import re
import sys


def main(argv):

    if len(argv) < 3:
        print('usage: %s input_file imports_dir output_file' % argv[0])
        return 1

    input_file_name = argv[1]
    imports_dir = argv[2]
    output_file_name = argv[3]

    input_file = open(input_file_name, 'r')
    input_script = input_file.read()
    input_file.close()

    def replace(match):
        import_file_name = match.group(1)
        full_path = os.path.join(imports_dir, import_file_name)
        if not os.access(full_path, os.F_OK):
            raise Exception('File %s referenced in %s not found on any source paths, '
                            'check source tree for consistency' %
                            (import_file_name, input_file_name))
        import_file = open(full_path, 'r')
        import_script = import_file.read()
        import_file.close()
        return import_script

    output_script = re.sub(r'importScripts\([\'"]([^\'"]+)[\'"]\)', replace, input_script)

    output_file = open(output_file_name, 'w')
    output_file.write(jsmin.jsmin(output_script))
    output_file.close()

    # Touch output file directory to make sure that Xcode will copy
    # modified resource files.
    if sys.platform == 'darwin':
        output_dir_name = os.path.dirname(output_file_name)
        os.utime(output_dir_name, None)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
