# Copyright (C) 2025 Apple Inc. All rights reserved.
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

import re

"""Enforces rules for .xcconfig files."""


class XcconfigChecker(object):
    categories = set([
        'xcconfig/sdk-conditional-prefer-deployment-target',
        'xcconfig/sdk-conditional-prefer-wk-platform',
    ])

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines):
        # Pattern to match any SDK conditional
        sdk_conditional_pattern = re.compile(r'^\s*([^=]+)\[sdk=([^\]]+)\]\s*=')

        for line_number, line in enumerate(lines, start=1):
            # Skip comments and empty lines
            stripped_line = line.strip()
            if not stripped_line or stripped_line.startswith('//'):
                continue

            # Check for SDK conditionals
            match = sdk_conditional_pattern.match(line)
            if not match:
                continue
            sdk_condition = match.group(2)

            if any(c.isdigit() for c in sdk_condition):
                # Versioned SDK conditional - recommend WebKitTargetConditionals.xcconfig
                self._handle_style_error(
                    line_number, 'xcconfig/sdk-conditional-prefer-deployment-target', 3,
                    ('Prefer deployment-target-based build settings from WebKitTargetConditionals.xcconfig '
                     'instead of [sdk={}]. SDK conditions apply to older OS versions '
                     'when back-deploying, so the version number is only valid when checking for behavior in the SDK or toolchain, '
                     'not controlling a behavior on the target OS.').format(sdk_condition))
            else:
                # Unversioned SDK conditional - recommend WK_PLATFORM_NAME
                self._handle_style_error(
                    line_number, 'xcconfig/sdk-conditional-prefer-wk-platform', 4,
                    ('Use $(WK_PLATFORM_NAME) to conditionalize a setting for different platforms instead of [sdk={}]. '
                     'SDK conditionals are misleading on Catalyst, and are inherited across platforms in some '
                     'embedded SDKs.').format(sdk_condition))
