# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import objc

from webkitscmpy import AutoInstall, Package, Version


def _import_modules():
    # Modules from pyobjc-framework-Cocoa and pyobjc-framework-Quartz
    # Note, the module (`import_name`) provided to `Package()`
    # must be imported first. This triggers the package install if necessary.
    return (
        __import__('Cocoa'),
        __import__('AppKit'),
        __import__('CoreFoundation'),
        __import__('Foundation'),
        __import__('Quartz')
    )


try:
    Cocoa, AppKit, CoreFoundation, Foundation, Quartz = _import_modules()
except Exception as e:
    AutoInstall.log('Import failed with exception {}'.format(e))
    AutoInstall.log('Autoinstalling...')
    pyobjc_core_version = Version.from_string(objc.__version__)
    AutoInstall.register(Package('Cocoa', pyobjc_core_version, pypi_name='pyobjc-framework-Cocoa', wheel=True))
    AutoInstall.register(Package('Quartz', pyobjc_core_version, pypi_name='pyobjc-framework-Quartz', wheel=True))
    Cocoa, AppKit, CoreFoundation, Foundation, Quartz = _import_modules()
