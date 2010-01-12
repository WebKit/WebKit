# Copyright (C) 2010 Google Inc. All rights reserved.
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

# FIXME: Is this the right way to do this?
from webkitpy.steps.applypatch import ApplyPatch
from webkitpy.steps.applypatchwithlocalcommit import ApplyPatchWithLocalCommit
from webkitpy.steps.build import Build
from webkitpy.steps.checkstyle import CheckStyle
from webkitpy.steps.cleanworkingdirectory import CleanWorkingDirectory
from webkitpy.steps.cleanworkingdirectorywithlocalcommits import CleanWorkingDirectoryWithLocalCommits
from webkitpy.steps.closebug import CloseBug
from webkitpy.steps.closebugforlanddiff import CloseBugForLandDiff
from webkitpy.steps.closepatch import ClosePatch
from webkitpy.steps.commit import Commit
from webkitpy.steps.completerollout import CompleteRollout
from webkitpy.steps.confirmdiff import ConfirmDiff
from webkitpy.steps.createbug import CreateBug
from webkitpy.steps.editchangelog import EditChangeLog
from webkitpy.steps.ensurebuildersaregreen import EnsureBuildersAreGreen
from webkitpy.steps.ensurelocalcommitifneeded import EnsureLocalCommitIfNeeded
from webkitpy.steps.obsoletepatches import ObsoletePatches
from webkitpy.steps.options import Options
from webkitpy.steps.postdiff import PostDiff
from webkitpy.steps.postdiffforcommit import PostDiffForCommit
from webkitpy.steps.preparechangelogforrevert import PrepareChangeLogForRevert
from webkitpy.steps.preparechangelog import PrepareChangeLog
from webkitpy.steps.promptforbugortitle import PromptForBugOrTitle
from webkitpy.steps.revertrevision import RevertRevision
from webkitpy.steps.runtests import RunTests
from webkitpy.steps.updatechangelogswithreviewer import UpdateChangeLogsWithReviewer
from webkitpy.steps.update import Update
