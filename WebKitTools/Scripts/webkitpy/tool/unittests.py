# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Imports unit tests for webkitpy.tool."""

# This module is imported by the module that imports all webkitpy unit tests.

from webkitpy.tool.bot.patchcollection_unittest import *
from webkitpy.tool.bot.queueengine_unittest import *
from webkitpy.tool.commands.download_unittest import *
from webkitpy.tool.commands.early_warning_system_unittest import *
from webkitpy.tool.commands.openbugs_unittest import OpenBugsTest
from webkitpy.tool.commands.queries_unittest import *
from webkitpy.tool.commands.queues_unittest import *
from webkitpy.tool.commands.sheriffbot_unittest import *
from webkitpy.tool.commands.upload_unittest import *
from webkitpy.tool.grammar_unittest import *
from webkitpy.tool.multicommandtool_unittest import *
from webkitpy.tool.steps.closebugforlanddiff_unittest import *
from webkitpy.tool.steps.steps_unittest import *
from webkitpy.tool.steps.updatechangelogswithreview_unittests import *
