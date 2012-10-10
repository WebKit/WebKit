#!/usr/bin/python
#
# Copyright (C) 2009 Motorola Mobility Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Motorola Mobility Inc. nor the names of its
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
# Usage: see action_preprocessgrammar.py --help
#

import errno
import getopt
import itertools
import os
import os.path
import shlex
import subprocess
import sys
from StringIO import StringIO


def exit_with_usage(exit_code):
    print "Usage:\n\t%s [-h|--help] [--output-dir <path>] [--defines <space-separated-feature-flags>] <filename.y.in> [<filename.y.includes>]"
    exit(exit_code)

options, arguments = getopt.getopt(sys.argv[1:], "h", ["help", "output-dir=", "defines="])

output_dir = "."
defines = ""
for option, argument in options:
    if option in ("-h", "--help"):
        exit_with_usage(0)
    elif option == "--output-dir":
        output_dir = argument
    elif option == "--defines":
        defines = argument

if len(arguments) == 0:
    exit_with_usage(1)

input_file = arguments[0]
includes_file = arguments[1] if len(arguments) == 2 else None

feature_flags = []
if defines:
    features = shlex.split(defines)
    # Make a list with -D intercalated between each feature, for use in the preprocessor command,
    # like such: ["-D", "ENABLE_FEATURE1=1", "-D", "ENABLE_FEATURE2=0", ...]
    feature_flags = list(itertools.chain(*zip(["-D"] * len(features), features)))

gcc_exe = "gcc"
if "CC" in os.environ:
    gcc_exe = os.environ["CC"]

input_root, input_ext = os.path.splitext(input_file)
assert input_ext == ".in"

p = subprocess.Popen([gcc_exe, "-E", "-P", "-x", "c", input_file] + feature_flags,
                     stdout=subprocess.PIPE)
processed_result = p.communicate()[0]
assert p.returncode == 0

output_file = open(os.path.join(output_dir, os.path.basename(input_root)), "w")

if includes_file:
    print >> output_file, open(includes_file).read()

print >> output_file, processed_result

output_file.close()
