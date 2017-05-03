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

"""Unit test for jstest.py."""

import unittest

from jstest import map_functions_to_dict


class JSTestCheckerTestCase(unittest.TestCase):
    """TestCase for jstest.py"""

    def test_map_functions_to_dict(self):
        """Tests map_functions_to_dict().

        This also implicitly tests strip_trailing_blank_lines_and_comments().
        """
        file1 = """
function shouldBe() {}

// Same as !shouldBe(), but output makes more sense.
function shouldNotBe() {}

function shouldNotThrow() {
    return;
}

// See also shouldNotThrow().
function shouldThrow() {
}

"""
        result1 = map_functions_to_dict(file1)

        file2 = """function shouldBe() {}
function shouldNotBe() {}
function shouldThrow() {
}
function shouldNotThrow() {
    return;
}
"""
        result2 = map_functions_to_dict(file2)

        self.assertDictEqual(result1, result2)
