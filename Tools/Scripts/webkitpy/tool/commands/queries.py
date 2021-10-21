# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# Copyright (c) 2012 Intel Corporation. All rights reserved.
# Copyright (c) 2013 University of Szeged. All rights reserved.
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

from __future__ import print_function
import fnmatch
import logging
import re

from datetime import datetime
from optparse import make_option

from webkitcorepy.string_utils import pluralize

from webkitpy.tool import steps

import webkitpy.common.config.urls as config_urls
from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.net.bugzilla import Bugzilla
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.regressionwindow import RegressionWindow
from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.system.user import User
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import LayoutTestFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.port import platform_options, configuration_options
from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.tool.multicommandtool import Command

_log = logging.getLogger(__name__)


class SuggestReviewers(AbstractSequencedCommand):
    name = "suggest-reviewers"
    help_text = "Suggest reviewers for a patch based on recent changes to the modified files."
    steps = [
        steps.SuggestReviewers,
    ]

    def _prepare_state(self, options, args, tool):
        options.suggest_reviewers = True
