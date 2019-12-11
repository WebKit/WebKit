#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2019 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Class for representing SDK information, in the form of Platform, Version, and
# whether or not it's an Internal SDK. This class also contains a factory
# method for producing an SDK instance from a "specification" string such as
# "macOS15.3Internal", like you would get from 'xcodebuild -showsdks".


import re

from webkitpy.common.system.executive import Executive


test_output = None


class SDK(object):
    __slots__ = ["xcode_specification", "platform", "version", "internal"]

    def __init__(self, xcode_specification, platform, version, internal):
        super(SDK, self).__init__()
        self.xcode_specification = xcode_specification
        self.platform = platform
        self.version = version
        self.internal = internal

    def __repr__(self):
        return "SDK({}, {}, {}, {})".format(
                self.xcode_specification,
                self.platform,
                self.version,
                self.internal)

    def as_xcode_specification(self):
        return "{}.internal".format(self.platform) if self.internal else self.platform

    @classmethod
    def get_preferred_sdk_for_platform(cls, platform, executive=None):

        preferred_sdk = cls._parse_sdk(platform)

        executive = executive or Executive()
        stdout = executive.run_command(["xcodebuild", "-showsdks"])

        for line in stdout.splitlines():
            m = re.match(r".*-sdk (.*)", line)
            if m:
                this_sdk = cls._parse_sdk(m.group(1))

                if preferred_sdk.platform != this_sdk.platform:
                    continue

                # Both have version information, so compare versions. If the
                # versions are equal, prefer the Internal sdk. Otherwise, keep
                # the one with the higher version.

                if preferred_sdk.version and this_sdk.version:
                    if float(preferred_sdk.version) == float(this_sdk.version):
                        if this_sdk.internal:
                            preferred_sdk = this_sdk
                    elif float(preferred_sdk.version) < float(this_sdk.version):
                        preferred_sdk = this_sdk

                # The preferred sdk does not have a version but the prospective
                # one does, so keep the prospective one.

                elif not preferred_sdk.version and this_sdk.version:
                    preferred_sdk = this_sdk

                # The preferred sdk has a version but the prospective one does
                # not, so keep the preferred one.

                elif preferred_sdk.version and not this_sdk.version:
                    pass

                # Neither has version information; prefer the current one if
                # it's internal.

                elif not preferred_sdk.version and not this_sdk.version:
                    if this_sdk.internal:
                        preferred_sdk = this_sdk

        return preferred_sdk

    @classmethod
    def _parse_sdk(cls, xcode_specification):
        # letters-and-dots, followed by optional numbers-and-dots, followed by
        # an optional "internal"
        m = re.match(r"([a-zA-Z.]+[a-zA-Z])([0-9.]+[0-9])?\.?(internal)?", xcode_specification)
        if not m:
            return SDK(xcode_specification, None, None, None)

        platform = m.group(1)
        version = m.group(2)
        internal = m.group(3)

        return SDK(xcode_specification, platform, version, internal)
