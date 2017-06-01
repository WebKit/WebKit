# Copyright (C) 2017 Apple Inc. All rights reserved.
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

import logging
import re

from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options

_log = logging.getLogger(__name__)


class CheckPatchRelevance(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.group,
            Options.quiet,
        ]

    bindings_paths = [
        "Source/WebCore",
        "Tools",
    ]

    jsc_paths = [
        "JSTests/",
        "Source/JavaScriptCore/",
        "Source/WTF/",
        "Source/bmalloc/",
        "Makefile",
        "Makefile.shared",
        "Source/Makefile",
        "Source/Makefile.shared",
        "Tools/Scripts/build-webkit",
        "Tools/Scripts/build-jsc",
        "Tools/Scripts/jsc-stress-test-helpers/",
        "Tools/Scripts/run-jsc",
        "Tools/Scripts/run-jsc-benchmarks",
        "Tools/Scripts/run-jsc-stress-tests",
        "Tools/Scripts/run-javascriptcore-tests",
        "Tools/Scripts/run-layout-jsc",
        "Tools/Scripts/update-javascriptcore-test-results",
        "Tools/Scripts/webkitdirs.pm",
    ]

    group_to_paths_mapping = {
        'bindings': bindings_paths,
        'jsc': jsc_paths,
    }

    def _changes_are_relevant(self, changed_files):
        # In the default case, all patches are relevant
        if self._options.group not in self.group_to_paths_mapping:
            return True

        patterns = self.group_to_paths_mapping[self._options.group]

        for changed_file in changed_files:
            for pattern in patterns:
                if re.search(pattern, changed_file, re.IGNORECASE):
                    return True

        return False

    def run(self, state):
        _log.info("Checking relevance of patch")

        change_list = self._tool.scm().changed_files()

        if self._changes_are_relevant(change_list):
            return True

        _log.debug("This patch does not have relevant changes.")
        raise ScriptError(message="This patch does not have relevant changes.")
