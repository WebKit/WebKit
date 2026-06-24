#  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import unittest
from swap_gtest_args import is_constant, split_args, find_macro_calls


class TestSwapGtestArgs(unittest.TestCase):

    def test_is_constant(self):
        # Numeric literals
        self.assertTrue(is_constant("0"))
        self.assertTrue(is_constant("123"))
        self.assertTrue(is_constant("0U"))
        self.assertTrue(is_constant("-1"))
        self.assertTrue(is_constant("0x10"))
        self.assertTrue(is_constant("0.5"))
        self.assertTrue(is_constant("1234"))

        # String literals
        self.assertTrue(is_constant('"foo"'))
        self.assertTrue(is_constant('""'))

        # Boolean literals
        self.assertTrue(is_constant("true"))
        self.assertTrue(is_constant("false"))

        # Nulls
        self.assertTrue(is_constant("nullptr"))
        self.assertTrue(is_constant("NULL"))
        self.assertTrue(is_constant("std::nullopt"))

        # k-prefixed
        self.assertTrue(is_constant("kAutoBandwidth"))
        self.assertTrue(is_constant("kAudioMid"))

        # Qualified enums
        self.assertTrue(is_constant("webrtc::MediaType::AUDIO"))
        self.assertTrue(is_constant("MediaProtocolType::kRtp"))
        self.assertTrue(is_constant("RtpTransceiverDirection::kRecvOnly"))

        # ALL_CAPS
        self.assertTrue(is_constant("ICE_UFRAG_LENGTH"))
        self.assertTrue(is_constant("GROUP_TYPE_BUNDLE"))

        # Casts
        self.assertTrue(is_constant("static_cast<size_t>(0)"))
        self.assertTrue(is_constant("static_cast<int>(kFoo)"))
        self.assertTrue(is_constant("size_t{kFoo}"))

        # Non-constants
        self.assertFalse(is_constant("acd->type()"))
        self.assertFalse(is_constant("ti_audio->description.ice_ufrag"))
        self.assertFalse(is_constant("vcd->codecs().size()"))
        self.assertFalse(is_constant("a + b"))

    def test_split_args(self):
        self.assertEqual(split_args("a, b"), ["a", " b"])
        self.assertEqual(split_args("func(a, b), c"), ["func(a, b)", " c"])
        self.assertEqual(split_args("std::vector<int>{1, 2}, d"),
                         ["std::vector<int>{1, 2}", " d"])
        self.assertEqual(split_args("\"string ending in backslash\\\""),
                         ["\"string ending in backslash\\\""])

    def test_find_macro_calls(self):
        content = 'EXPECT_EQ(0, x); ASSERT_NE(kFoo, y->bar());'
        calls = find_macro_calls(content)
        # find_macro_calls returns in reverse order of position
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0][2], "ASSERT_NE")
        self.assertEqual(calls[1][2], "EXPECT_EQ")

    def test_complex_cases(self):
        content = """
        EXPECT_EQ(webrtc::MediaType::AUDIO, acd->type());
        EXPECT_EQ(0U, acd->first_ssrc());
        EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
        EXPECT_EQ("opus", preferences[0].name);
        EXPECT_EQ(static_cast<size_t>(ICE_UFRAG_LENGTH),
                  ti_audio->description.ice_ufrag.size());
        // Should not swap
        EXPECT_EQ(x, y);
        EXPECT_EQ(0, 1);
        EXPECT_GT(x, kFive);
        """

        from swap_gtest_args import process_content
        updated = process_content(content)
        self.assertIn("EXPECT_EQ(acd->type(), webrtc::MediaType::AUDIO)",
                      updated)
        self.assertIn("EXPECT_EQ(acd->first_ssrc(), 0U)", updated)
        self.assertIn("EXPECT_EQ(acd->bandwidth(), kAutoBandwidth)", updated)
        self.assertIn("EXPECT_EQ(preferences[0].name, \"opus\")", updated)
        self.assertIn(
            "EXPECT_EQ(ti_audio->description.ice_ufrag.size(), "
            "static_cast<size_t>(ICE_UFRAG_LENGTH))", updated)
        self.assertIn("EXPECT_EQ(x, y)", updated)
        self.assertIn("EXPECT_EQ(0, 1)", updated)
        # Argument order dependent operators are not swapped.
        self.assertIn("EXPECT_GT(x, kFive)", updated)


if __name__ == "__main__":
    unittest.main()
