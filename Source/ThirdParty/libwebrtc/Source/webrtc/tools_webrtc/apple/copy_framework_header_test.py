#!/usr/bin/env vpython3

#  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest
from copy_framework_header import _ReplaceDoubleQuote


class TestCopyFramework(unittest.TestCase):
  def testReplaceDoubleQuote(self):
    self.assertEqual(_ReplaceDoubleQuote("""#import "RTCMacros.h\""""),
                     """#import <WebRTC/RTCMacros.h>""")
    self.assertEqual(_ReplaceDoubleQuote("""#import "RTCMacros.h\"\n"""),
                     """#import <WebRTC/RTCMacros.h>\n""")
    self.assertEqual(
        _ReplaceDoubleQuote("""#import "UIDevice+RTCDevice.h\"\n"""),
        """#import <WebRTC/UIDevice+RTCDevice.h>\n""")
    self.assertEqual(
        _ReplaceDoubleQuote("#import \"components/video_codec/" +
                            "RTCVideoDecoderFactoryH264.h\"\n"),
        """#import <WebRTC/RTCVideoDecoderFactoryH264.h>\n""")
    self.assertEqual(
        _ReplaceDoubleQuote(
            """@property(atomic, strong) RTC_OBJC_TYPE(RTCVideoFrame) *\n"""),
        """@property(atomic, strong) RTC_OBJC_TYPE(RTCVideoFrame) *\n""")


if __name__ == '__main__':
  unittest.main()
