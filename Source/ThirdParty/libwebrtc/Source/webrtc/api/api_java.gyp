# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file exists only because there's no other way to avoid errors in the
# Chromium build due to the inclusion of build/java.gypi. GYP unfortunately
# processes all 'includes' for all .gyp files, ignoring conditions. This
# processing takes place early in the cycle, before it's possible to use
# variables. It will go away when WebRTC has fully migrated to GN.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          # |libjingle_peerconnection_java| builds a jar file with name
          # libjingle_peerconnection_java.jar using Chrome's build system.
          # It includes all Java files needed to setup a PeeerConnection call
          # from Android.
          'target_name': 'libjingle_peerconnection_java',
          'type': 'none',
          'dependencies': [
            '<(webrtc_root)/api/api.gyp:libjingle_peerconnection_so',
          ],
          'variables': {
            # Designate as Chromium code and point to our lint settings to
            # enable linting of the WebRTC code (this is the only way to make
            # lint_action invoke the Android linter).
            'android_manifest_path': '<(webrtc_root)/build/android/AndroidManifest.xml',
            'suppressions_file': '<(webrtc_root)/build/android/suppressions.xml',
            'chromium_code': 1,
            'java_in_dir': 'android/java',
            'webrtc_base_dir': '<(webrtc_root)/base',
            'webrtc_modules_dir': '<(webrtc_root)/modules',
            'additional_src_dirs' : [
              '<(webrtc_base_dir)/java',
              '<(webrtc_modules_dir)/audio_device/android/java/src',
            ],
          },
          'includes': ['../../build/java.gypi'],
        },
      ],  # targets
    }],  # OS=="android"
  ],  # conditions
}
