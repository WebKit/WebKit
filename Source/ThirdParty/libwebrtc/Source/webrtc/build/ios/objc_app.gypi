# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Include this .gypi in an ObjC target's definition to allow it to be
# used as an iOS or OS/X application.

{
  'variables': {
    'infoplist_file': './objc_app.plist',
  },
  'dependencies': [
    '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
    '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
  ],
  'mac_bundle': 1,
  'mac_bundle_resources': [
    '<(infoplist_file)',
  ],
  # The plist is listed above so that it appears in XCode's file list,
  # but we don't actually want to bundle it.
  'mac_bundle_resources!': [
    '<(infoplist_file)',
  ],
  'xcode_settings': {
    'CLANG_ENABLE_OBJC_ARC': 'YES',
    'INFOPLIST_FILE': '<(infoplist_file)',
  },
}
