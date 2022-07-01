# Copyright (C) 2018 Apple Inc. All rights reserved.
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


"""Supports ensuring equality of SDKVariant.xcconfig files."""

import os

from webkitpy.common.system.systemhost import SystemHost

SDK_VARIANT_FILES = [
    "PerformanceTests/MediaTime/Configurations/SDKVariant.xcconfig",
    "Tools/ImageDiff/cg/Configurations/SDKVariant.xcconfig",
    "Tools/WebEditingTester/Configurations/SDKVariant.xcconfig",
    "Tools/MiniBrowser/Configurations/SDKVariant.xcconfig",
    "Tools/lldb/lldbWebKitTester/Configurations/SDKVariant.xcconfig",
    "Tools/DumpRenderTree/mac/Configurations/SDKVariant.xcconfig",
    "Tools/ContentExtensionTester/Configurations/SDKVariant.xcconfig",
    "Tools/WebKitTestRunner/Configurations/SDKVariant.xcconfig",
    "Tools/MobileMiniBrowser/Configurations/SDKVariant.xcconfig",
    "Tools/TestWebKitAPI/Configurations/SDKVariant.xcconfig",
    "Source/bmalloc/Configurations/SDKVariant.xcconfig",
    "Source/WTF/Configurations/SDKVariant.xcconfig",
    "Source/WebKitLegacy/mac/Configurations/SDKVariant.xcconfig",
    "Source/WebInspectorUI/Configurations/SDKVariant.xcconfig",
    "Source/WebKit/Configurations/SDKVariant.xcconfig",
    "Source/JavaScriptCore/Configurations/SDKVariant.xcconfig",
    "Source/WebCore/Configurations/SDKVariant.xcconfig",
    "Source/WebCore/PAL/Configurations/SDKVariant.xcconfig",
    "Source/ThirdParty/ANGLE/Configurations/SDKVariant.xcconfig",
    "Source/ThirdParty/gtest/xcode/Config/SDKVariant.xcconfig",
    "Source/ThirdParty/libwebrtc/Configurations/SDKVariant.xcconfig",
    "Source/WebGPU/Configurations/SDKVariant.xcconfig",
]


class SDKVariantChecker(object):
    categories = set(['SDKVariant/new', 'SDKVariant/equality'])

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()
        self._host = SystemHost.get_default()
        self._fs = self._host.filesystem

    def check(self, inline=None):
        if self._file_path not in SDK_VARIANT_FILES:
            self._handle_style_error(0, 'SDKVariant/new', 5, "Patch introduces a new SDKVariant.xcconfig, which check-webkit-style doesn't know about. Please add it to the list in SDKVariant.py.")
            return

        with self._fs.open_binary_file_for_reading(self._file_path) as filehandle:
            baseline_content = filehandle.read()

        other_SDK_VARIANTs_files = SDK_VARIANT_FILES
        other_SDK_VARIANTs_files.remove(self._file_path)

        for path in other_SDK_VARIANTs_files:
            with self._fs.open_binary_file_for_reading(path) as filehandle:
                test_content = filehandle.read()
                if baseline_content != test_content:
                    self._handle_style_error(0, 'SDKVariant/equality', 5, "Any changes made to SDKVariant should be made to all of them (changed file does not match {0}).".format(path))
