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

from modules.comments import bug_comment_from_commit_text
from modules.logging import log, error
from modules.webkitport import WebKitPort


class CommandOptions(object):
    force_clean = make_option("--force-clean", action="store_true", dest="force_clean", default=False, help="Clean working directory before applying patches (removes local changes and commits)")
    clean = make_option("--no-clean", action="store_false", dest="clean", default=True, help="Don't check if the working directory is clean before applying patches")
    check_builders = make_option("--ignore-builders", action="store_false", dest="check_builders", default=True, help="Don't check to see if the build.webkit.org builders are green before landing.")
    quiet = make_option("--quiet", action="store_true", dest="quiet", default=False, help="Produce less console output.")
    non_interactive = make_option("--non-interactive", action="store_true", dest="non_interactive", default=False, help="Never prompt the user, fail as fast as possible.")
    parent_command = make_option("--parent-command", action="store", dest="parent_command", default=None, help="(Internal) The command that spawned this instance.")
    update = make_option("--no-update", action="store_false", dest="update", default=True, help="Don't update the working directory.")
    build = make_option("--no-build", action="store_false", dest="build", default=True, help="Commit without building first, implies --no-test.")
    test = make_option("--no-test", action="store_false", dest="test", default=True, help="Commit without running run-webkit-tests.")
    close_bug = make_option("--no-close", action="store_false", dest="close_bug", default=True, help="Leave bug open after landing.")
    port = make_option("--port", action="store", dest="port", default=None, help="Specify a port (e.g., mac, qt, gtk, ...).")


class AbstractStep(object):
    def __init__(self, tool, options, patch=None):
        self._tool = tool
        self._options = options
        self._patch = patch
        self._port = None

    def _run_script(self, script_name, quiet=False, port=WebKitPort):
        log("Running %s" % script_name)
        self._tool.executive.run_and_throw_if_fail(port.script_path(script_name), quiet)

    # FIXME: The port should live on the tool.
    def port(self):
        if self._port:
            return self._port
        self._port = WebKitPort.port(self._options.port)
        return self._port

    @classmethod
    def options(cls):
        return []

    def run(self, tool):
        raise NotImplementedError, "subclasses must implement"


class PrepareChangelogStep(AbstractStep):
    def run(self):
        self._run_script("prepare-ChangeLog")


class CleanWorkingDirectoryStep(AbstractStep):
    def __init__(self, tool, options, patch=None, allow_local_commits=False):
        AbstractStep.__init__(self, tool, options, patch)
        self._allow_local_commits = allow_local_commits

    @classmethod
    def options(cls):
        return [
            CommandOptions.force_clean,
            CommandOptions.clean,
        ]

    def run(self):
        os.chdir(self._tool.scm().checkout_root)
        if not self._allow_local_commits:
            self._tool.scm().ensure_no_local_commits(self._options.force_clean)
        if self._options.clean:
            self._tool.scm().ensure_clean_working_directory(force_clean=self._options.force_clean)


class UpdateStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.update,
            CommandOptions.port,
        ]

    def run(self):
        if not self._options.update:
            return
        log("Updating working directory")
        self._tool.executive.run_and_throw_if_fail(self.port().update_webkit_command())


class ApplyPatchStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.non_interactive,
        ]

    def run(self):
        log("Processing patch %s from bug %s." % (self._patch["id"], self._patch["bug_id"]))
        self._tool.scm().apply_patch(self._patch, force=self._options.non_interactive)


class EnsureBuildersAreGreenStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.check_builders,
        ]

    def run(self):
        if not self._options.check_builders:
            return
        red_builders_names = self._tool.buildbot.red_core_builders_names()
        if not red_builders_names:
            return
        red_builders_names = map(lambda name: "\"%s\"" % name, red_builders_names) # Add quotes around the names.
        error("Builders [%s] are red, please do not commit.\nSee http://%s.\nPass --ignore-builders to bypass this check." % (", ".join(red_builders_names), self._tool.buildbot.buildbot_host))


class BuildStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.build,
            CommandOptions.quiet,
        ]

    def run(self):
        if not self._options.build:
            return
        log("Building WebKit")
        self._tool.executive.run_and_throw_if_fail(self.port().build_webkit_command(), self._options.quiet)


class CheckStyleStep(AbstractStep):
    def run(self):
        self._run_script("check-webkit-style")


class RunTestsStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.build,
            CommandOptions.test,
            CommandOptions.non_interactive,
            CommandOptions.quiet,
            CommandOptions.port,
        ]

    def run(self):
        if not self._options.build:
            return
        if not self._options.test:
            return
        args = self.port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.append("--no-launch-safari")
            args.append("--exit-after-n-failures=1")
        if self._options.quiet:
            args.append("--quiet")
        self._tool.executive.run_and_throw_if_fail(args)


class CommitStep(AbstractStep):
    def run(self):
        commit_message = self._tool.scm().commit_message_for_this_commit()
        return self._tool.scm().commit_with_message(commit_message.message())


class ClosePatchStep(AbstractStep):
    def run(self, commit_log):
        comment_text = bug_comment_from_commit_text(self._tool.scm(), commit_log)
        self._tool.bugs.clear_attachment_flags(self._patch["id"], comment_text)


class CloseBugStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.close_bug,
        ]

    def run(self):
        if not self._options.close_bug:
            return
        # Check to make sure there are no r? or r+ patches on the bug before closing.
        # Assume that r- patches are just previous patches someone forgot to obsolete.
        patches = self._tool.bugs.fetch_patches_from_bug(self._patch["bug_id"])
        for patch in patches:
            review_flag = patch.get("review")
            if review_flag == "?" or review_flag == "+":
                log("Not closing bug %s as attachment %s has review=%s.  Assuming there are more patches to land from this bug." % (patch["bug_id"], patch["id"], review_flag))
                return
        self._tool.bugs.close_bug_as_fixed(self._patch["bug_id"], "All reviewed patches have been landed.  Closing bug.")


# FIXME: This class is a dinosaur and should be extinct soon.
class BuildSteps:
    # FIXME: The options should really live on each "Step" object.
    @staticmethod
    def cleaning_options():
        return [
            CommandOptions.force_clean,
            CommandOptions.clean,
        ]

    # FIXME: These distinctions are bogus.  We need a better model for handling options.
    @staticmethod
    def build_options():
        return [
            CommandOptions.check_builders,
            CommandOptions.quiet,
            CommandOptions.non_interactive,
            CommandOptions.parent_command,
            CommandOptions.port,
        ]

    @staticmethod
    def land_options():
        return [
            CommandOptions.update,
            CommandOptions.build,
            CommandOptions.test,
            CommandOptions.close_bug,
        ]

