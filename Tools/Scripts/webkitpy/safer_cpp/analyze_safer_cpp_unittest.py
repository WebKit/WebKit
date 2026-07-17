# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import os
import subprocess
import sys
import tempfile
import unittest

ANALYZE_SAFER_CPP = os.path.realpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'analyze-safer-cpp'))


@unittest.skipIf(sys.platform.startswith('win'), 'fake clang stub is a POSIX shell script')
class AnalyzeSaferCppTest(unittest.TestCase):
    def test_compile_failure_is_not_reported_as_a_clean_pass(self):
        # A file that fails to compile (e.g. a fatal-error missing header) must not be
        # reported as analyzed cleanly: clang's non-zero exit has to fail the run, even
        # though a "fatal error:" line is not one of the checker diagnostics the tool parses.
        with tempfile.TemporaryDirectory() as directory:
            fakeClang = os.path.join(directory, 'fake-clang')
            with open(fakeClang, 'w') as f:
                f.write('#!/bin/sh\n'
                        'echo "$0:1:10: fatal error: \'nope.h\' file not found" 1>&2\n'
                        'exit 1\n')
            os.chmod(fakeClang, 0o755)

            source = os.path.join(directory, 'repro.cpp')
            with open(source, 'w') as f:
                f.write('#include "nope.h"\nint main() { return 0; }\n')

            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': source,
                            'command': 'clang++ -c {} -o {}.o'.format(source, source)}], f)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source,
                 '--compile-commands', compileCommands, '--clang', fakeClang],
                capture_output=True, text=True)

            output = proc.stdout + proc.stderr
            self.assertNotEqual(proc.returncode, 0,
                                'a compile failure must not exit 0:\n' + output)
            self.assertIn('did not compile', output)


if __name__ == '__main__':
    unittest.main()
