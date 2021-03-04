# Copyright (C) 2021 Apple Inc. All rights reserved.
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
#     * Neither the name of Apple Inc. nor the names of its
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


class Test(object):
    """Data about a test and its expectations.

    Note that this is inherently platform specific, as expectations are platform specific."""

    def __init__(
        self,
        test_path,
        expected_text_path=None,
        expected_image_path=None,
        expected_checksum_path=None,
        expected_audio_path=None,
        reference_files=None,
    ):
        self.test_path = test_path
        self.expected_text_path = expected_text_path
        self.expected_image_path = expected_image_path
        self.expected_checksum_path = expected_checksum_path
        self.expected_audio_path = expected_audio_path
        self.reference_files = reference_files

    def __repr__(self):
        return (
            "Test(%r, "
            "expected_text_path=%r, "
            "expected_image_path=%r, "
            "expected_checksum_path=%r, "
            "expected_audio_path=%r, "
            "reference_files=%r)"
        ) % (
            self.test_path,
            self.expected_text_path,
            self.expected_image_path,
            self.expected_checksum_path,
            self.expected_audio_path,
            self.reference_files,
        )

    def __eq__(self, other):
        return (
            self.test_path == other.test_path
            and self.expected_text_path == other.expected_text_path
            and self.expected_image_path == other.expected_image_path
            and self.expected_checksum_path == other.expected_checksum_path
            and self.expected_audio_path == other.expected_audio_path
            and self.reference_files == other.reference_files
        )
