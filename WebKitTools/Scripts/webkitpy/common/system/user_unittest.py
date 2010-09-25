# Copyright (C) 2010 Research in Motion Ltd. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Research in Motion Ltd. nor the names of its
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

import unittest

from webkitpy.common.system.user import User

class UserTest(unittest.TestCase):

    example_user_response = "example user response"

    def test_prompt_repeat(self):
        self.repeatsRemaining = 2
        def mock_raw_input(message):
            self.repeatsRemaining -= 1
            if not self.repeatsRemaining:
                return UserTest.example_user_response
            return None
        self.assertEqual(User.prompt("input", repeat=self.repeatsRemaining, raw_input=mock_raw_input), UserTest.example_user_response)

    def test_prompt_when_exceeded_repeats(self):
        self.repeatsRemaining = 2
        def mock_raw_input(message):
            self.repeatsRemaining -= 1
            return None
        self.assertEqual(User.prompt("input", repeat=self.repeatsRemaining, raw_input=mock_raw_input), None)

    def test_prompt_with_list(self):
        def run_prompt_test(options, inputs, expected_result, can_choose_multiple=False):
            def mock_raw_input(message):
                return inputs.pop(0)
            self.assertEqual(User.prompt_with_list("title", options, can_choose_multiple=can_choose_multiple, raw_input=mock_raw_input), expected_result)
            self.assertEqual(len(inputs), 0)

        run_prompt_test(["foo", "bar"], ["1"], "foo")
        run_prompt_test(["foo", "bar"], ["badinput", "2"], "bar")

        run_prompt_test(["foo", "bar"], ["1,2"], ["foo", "bar"], can_choose_multiple=True)
        run_prompt_test(["foo", "bar"], ["  1,  2   "], ["foo", "bar"], can_choose_multiple=True)
        run_prompt_test(["foo", "bar"], ["all"], ["foo", "bar"], can_choose_multiple=True)
        run_prompt_test(["foo", "bar"], [""], ["foo", "bar"], can_choose_multiple=True)
        run_prompt_test(["foo", "bar"], ["  "], ["foo", "bar"], can_choose_multiple=True)
        run_prompt_test(["foo", "bar"], ["badinput", "all"], ["foo", "bar"], can_choose_multiple=True)

if __name__ == '__main__':
    unittest.main()
