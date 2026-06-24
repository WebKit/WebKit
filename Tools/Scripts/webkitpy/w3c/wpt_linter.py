# Copyright (c) 2018, Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
 This script is a wrapper for the web-platform-tests linter
"""

import json
import logging
import os
import subprocess
import tempfile

_log = logging.getLogger(__name__)


class WPTLinter(object):
    def __init__(self, repository_directory):
        self.wpt_path = repository_directory

    def lint(self, paths=None):
        """Yield each ``./wpt lint --json`` error dict."""
        cmd = ['./wpt', 'lint', '--json', '--repo-root', '.']

        if paths is not None:
            paths_file = None
            try:
                with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
                    paths_file = f.name
                    for path in paths:
                        f.write(path + '\n')
                cmd.extend(['--paths-file', paths_file])
                yield from self._run_lint(cmd)
            finally:
                if paths_file and os.path.exists(paths_file):
                    os.unlink(paths_file)
        else:
            yield from self._run_lint(cmd)

    def _run_lint(self, cmd):
        _log.debug('Running WPT linter: %s (cwd=%s)', ' '.join(cmd), self.wpt_path)
        try:
            result = subprocess.run(
                cmd,
                cwd=self.wpt_path,
                capture_output=True,
                check=True,
            )
        except subprocess.CalledProcessError as e:
            # lint exits with 1 when there's lint errors; any other exit code is an
            # actual failure.
            if e.returncode != 1:
                raise RuntimeError(
                    'WPT linter failed:\n' + e.stderr.decode('utf-8', 'replace')
                ) from e
            result = e
        for line in result.stdout.splitlines():
            if not line.strip():
                continue
            yield json.loads(line)
