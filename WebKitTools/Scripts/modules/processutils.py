# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

import os
import StringIO
import subprocess
import sys

from modules.logging import tee
from modules.scm import ScriptError

def run_command_with_teed_output(args, teed_output):
    child_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    # Use our own custom wait loop because Popen ignores a tee'd stderr/stdout.
    # FIXME: This could be improved not to flatten output to stdout.
    while True:
        output_line = child_process.stdout.readline()
        if output_line == "" and child_process.poll() != None:
            return child_process.poll()
        teed_output.write(output_line)

def run_and_throw_if_fail(args, quiet=False):
    # Cache the child's output locally so it can be used for error reports.
    child_out_file = StringIO.StringIO()
    if quiet:
        dev_null = open(os.devnull, "w")
    child_stdout = tee(child_out_file, dev_null if quiet else sys.stdout)
    exit_code = run_command_with_teed_output(args, child_stdout)
    if quiet:
        dev_null.close()

    child_output = child_out_file.getvalue()
    child_out_file.close()

    if exit_code:
        raise ScriptError(script_args=args, exit_code=exit_code, output=child_output)
