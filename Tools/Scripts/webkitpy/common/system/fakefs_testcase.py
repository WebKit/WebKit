# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Mixins that pin the OS emulated by pyfakefs.

pyfakefs emulates the host OS by default. Mix one of the concrete mixins
below into a unittest.TestCase to run its tests under a specific OS, ahead
of the TestCase in the bases so that its setUpPyfakefs() is the one called.

Switching OS resets the fake filesystem, so it happens inside
setUpPyfakefs(), before the test creates any files or a FileSystem (which
reads os.sep when constructed).
"""

import abc

from pyfakefs.fake_filesystem import OSType
from pyfakefs.fake_filesystem_unittest import TestCaseMixin


class PyFakefsOSTestCaseMixin(TestCaseMixin, abc.ABC):
    @property
    @abc.abstractmethod
    def fs_os(self):
        ...

    def setUpPyfakefs(self, *args, **kwargs):
        super().setUpPyfakefs(*args, **kwargs)
        self.fs.os = self.fs_os


class PyFakefsLinuxTestCaseMixin(PyFakefsOSTestCaseMixin):
    fs_os = OSType.LINUX


class PyFakefsWindowsTestCaseMixin(PyFakefsOSTestCaseMixin):
    fs_os = OSType.WINDOWS


class PyFakefsMacOSTestCaseMixin(PyFakefsOSTestCaseMixin):
    fs_os = OSType.MACOS
