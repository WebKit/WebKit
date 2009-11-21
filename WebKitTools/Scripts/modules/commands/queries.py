#!/usr/bin/env python
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

# FIXME: Trim down this import list once we have unit tests.
import os
import re
import StringIO
import subprocess
import sys
import time

from datetime import datetime, timedelta
from optparse import make_option

from modules.bugzilla import Bugzilla, parse_bug_id
from modules.buildbot import BuildBot
from modules.changelogs import ChangeLog
from modules.comments import bug_comment_from_commit_text
from modules.grammar import pluralize
from modules.landingsequence import LandingSequence, ConditionalLandingSequence
from modules.logging import error, log, tee
from modules.multicommandtool import MultiCommandTool, Command
from modules.patchcollection import PatchCollection
from modules.scm import CommitMessage, detect_scm_system, ScriptError, CheckoutNeedsUpdate
from modules.statusbot import StatusBot
from modules.webkitlandingscripts import WebKitLandingScripts, commit_message_for_this_commit
from modules.webkitport import WebKitPort
from modules.workqueue import WorkQueue, WorkQueueDelegate

class BugsToCommit(Command):
    name = "bugs-to-commit"
    def __init__(self):
        Command.__init__(self, "List bugs in the commit-queue")

    def execute(self, options, args, tool):
        bug_ids = tool.bugs.fetch_bug_ids_from_commit_queue()
        for bug_id in bug_ids:
            print "%s" % bug_id


class PatchesToCommit(Command):
    name = "patches-to-commit"
    def __init__(self):
        Command.__init__(self, "List patches in the commit-queue")

    def execute(self, options, args, tool):
        patches = tool.bugs.fetch_patches_from_commit_queue()
        log("Patches in commit queue:")
        for patch in patches:
            print "%s" % patch["url"]


class ReviewedPatches(Command):
    name = "reviewed-patches"
    def __init__(self):
        Command.__init__(self, "List r+'d patches on a bug", "BUGID")

    def execute(self, options, args, tool):
        bug_id = args[0]
        patches_to_land = tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
        for patch in patches_to_land:
            print "%s" % patch["url"]


class TreeStatus(Command):
    name = "tree-status"
    def __init__(self):
        Command.__init__(self, "Print the status of the %s buildbots" % BuildBot.default_host)

    def execute(self, options, args, tool):
        for builder in tool.buildbot.builder_statuses():
            status_string = "ok" if builder["is_green"] else "FAIL"
            print "%s : %s" % (status_string.ljust(4), builder["name"])
