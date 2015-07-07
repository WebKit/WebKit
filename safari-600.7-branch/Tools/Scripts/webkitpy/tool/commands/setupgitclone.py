# Copyright (c) 2013 Apple Inc. All rights reserved.
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

from webkitpy.tool.multicommandtool import Command
from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.system.executive import ScriptError

class SetupGitClone(Command):
    name = "setup-git-clone"
    help_text = "Configures a new Git clone for the WebKit development"

    def execute(self, options, args, tool):
        if not isinstance(tool.scm(), Git):
            print "This command only works inside a Git checkout."
            return

        if tool.scm().has_working_directory_changes():
            print "There are local changes; aborting the command."
            return

        username, email = self._get_username_and_email(tool)

        # FIXME: We shouldn't be using a privatd method
        run_git = tool.scm()._run_git
        run_git(["pull"])
        run_git(["svn", "init", "--prefix=origin/", "-T", "trunk", "http://svn.webkit.org/repository/webkit"])
        run_git(["config", "--replace", "svn-remote.svn.fetch", "trunk:refs/remotes/origin/master"])
        run_git(["svn", "fetch"])

        run_git(["config", "user.name", username])
        run_git(["config", "user.email", email])

        # Better Objective-C diff.
        run_git(["config", "diff.objcpp.xfuncname", "^[-+@a-zA-Z_].*$"])
        run_git(["config", "diff.objcppheader.xfuncname", "^[@a-zA-Z_].*$"])

        if tool.user.confirm("Do you want to auto-color status, diff, and branch? (y/n)"):
            run_git(["config", "color.status", "auto"])
            run_git(["config", "color.diff", "auto"])
            run_git(["config", "color.branch", "auto"])

        if tool.user.confirm("Do you use a rebase-based workflow? (y=yes; n=no, I use a merge-based workflow)"):
            run_git(["config", "merge.changelog.driver", "perl Tools/Scripts/resolve-ChangeLogs --fix-merged --merge-driver %O %A %B"])
        else:
            run_git(["config", "merge.changelog.driver", "perl Tools/Scripts/resolve-ChangeLogs --fix-merged --merge-driver %O %B %A"])

        if tool.user.confirm("Do you want to append the git branch name to every build? (e.g. WebKitBuild/mybranch/; y/n)"):
            run_git(["config", "core.webKitBranchBuild", "true"])
            print "You can override this option via git config branch.$branchName.webKitBranchBuild (true|false)"

        print "Done"

    def _get_username_and_email(self, tool):
        try:
            original_path = tool.filesystem.abspath(".")

            tool.filesystem.chdir("Tools/Scripts/")
            username = tool.executive.run_and_throw_if_fail(["perl", "-e", "use VCSUtils; print STDOUT changeLogName();"], quiet=True)
            if not username:
                username = tool.user.prompt("Your name:")

            email = tool.executive.run_and_throw_if_fail(["perl", "-e", "use VCSUtils; print STDOUT changeLogEmailAddress();"], quiet=True)
            if not email:
                email = tool.user.prompt("Your email address:")

            tool.filesystem.chdir(original_path)

            return (username, email)
        except ScriptError as error:
            # VCSUtils prints useful error messages on failure, we shouldn't hide these
            if error.output:
                print error.output
            raise
