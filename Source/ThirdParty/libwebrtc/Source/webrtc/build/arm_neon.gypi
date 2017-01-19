# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file sets correct neon flags. Include it if you want to build
# source with neon intrinsics.
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_lib',
#   'type': 'static_library',
#   'sources': [
#     'foo.c',
#     'bar.cc',
#   ],
#   'includes': ['path/to/this/gypi/file'],
# }

{
  'cflags!': [
    '-mfpu=vfpv3-d16',
  ],
  'conditions': [
    # "-mfpu=neon" is not required for arm64 in GCC.
    ['target_arch!="arm64"', {
      'cflags': [
        '-mfpu=neon',
       ],
    }],
    # Disable GCC LTO on NEON targets due to compiler bug.
    # TODO(fdegans): Enable this. See crbug.com/408997.
    ['clang==0 and use_lto==1', {
      'cflags!': [
        '-flto',
        '-ffat-lto-objects',
      ],
    }],
  ],
}
