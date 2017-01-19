# Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# ObjC target common prefix header.

{
  'variables': {
    'objc_prefix_file': '../sdk/objc/WebRTC-Prefix.pch',
  },
  'xcode_settings': {
    'CLANG_ENABLE_OBJC_ARC': 'YES',
    'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'YES',
    'GCC_PREFIX_HEADER': '<(objc_prefix_file)',
    'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'
  },
}
