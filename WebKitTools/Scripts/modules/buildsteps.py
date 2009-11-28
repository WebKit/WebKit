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

from optparse import make_option

from modules.logging import log, error
from modules.processutils import run_and_throw_if_fail
from modules.webkitport import WebKitPort

class BuildSteps:
    # FIXME: The options should really live on each "Step" object.
    @staticmethod
    def cleaning_options():
        return [
            make_option("--force-clean", action="store_true", dest="force_clean", default=False, help="Clean working directory before applying patches (removes local changes and commits)"),
            make_option("--no-clean", action="store_false", dest="clean", default=True, help="Don't check if the working directory is clean before applying patches"),
        ]

    @staticmethod
    def build_options():
        return [
            make_option("--ignore-builders", action="store_false", dest="check_builders", default=True, help="Don't check to see if the build.webkit.org builders are green before landing."),
            make_option("--quiet", action="store_true", dest="quiet", default=False, help="Produce less console output."),
            make_option("--non-interactive", action="store_true", dest="non_interactive", default=False, help="Never prompt the user, fail as fast as possible."),
            make_option("--parent-command", action="store", dest="parent_command", default=None, help="(Internal) The command that spawned this instance."),
        ] + WebKitPort.port_options()

    @staticmethod
    def land_options():
        return [
            make_option("--no-update", action="store_false", dest="update", default=True, help="Don't update the working directory."),
            make_option("--no-build", action="store_false", dest="build", default=True, help="Commit without building first, implies --no-test."),
            make_option("--no-test", action="store_false", dest="test", default=True, help="Commit without running run-webkit-tests."),
            make_option("--no-close", action="store_false", dest="close_bug", default=True, help="Leave bug open after landing."),
        ]

    def _run_script(cls, script_name, quiet=False, port=WebKitPort):
        log("Running %s" % script_name)
        run_and_throw_if_fail(port.script_path(script_name), quiet)

    def prepare_changelog(self):
        self.run_script("prepare-ChangeLog")

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
        run_and_throw_if_fail(args)

    def ensure_builders_are_green(self, buildbot, options):
        if not options.check_builders or buildbot.core_builders_are_green():
            return
        error("Builders at %s are red, please do not commit.  Pass --ignore-builders to bypass this check." % (buildbot.buildbot_host))

    def build_webkit(self, quiet=False, port=WebKitPort):
        log("Building WebKit")
        run_and_throw_if_fail(port.build_webkit_command(), quiet)

    def check_style(self):
        self._run_script("check-webkit-style")

