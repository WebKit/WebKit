# Copyright (C) 2009 Google Inc. All rights reserved.
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

from modules.logging import error
from modules.webkitlandingscripts import WebKitLandingScripts
from modules.webkitport import WebKitPort

class BuildSteps:
    def clean_working_directory(self, scm, options, allow_local_commits=False):
        os.chdir(scm.checkout_root)
        if not allow_local_commits:
            scm.ensure_no_local_commits(options.force_clean)
        if options.clean:
            scm.ensure_clean_working_directory(force_clean=options.force_clean)

    def run_tests(self, launch_safari, fail_fast=False, quiet=False, port=WebKitPort):
        args = port.run_webkit_tests_command()
        if not launch_safari:
            args.append("--no-launch-safari")
        if quiet:
            args.append("--quiet")
        if fail_fast:
            args.append("--exit-after-n-failures=1")
        WebKitLandingScripts.run_and_throw_if_fail(args)

    def ensure_builders_are_green(self, buildbot, options):
        if not options.check_builders or buildbot.core_builders_are_green():
            return
        error("Builders at %s are red, please do not commit.  Pass --ignore-builders to bypass this check." % (buildbot.buildbot_host))
