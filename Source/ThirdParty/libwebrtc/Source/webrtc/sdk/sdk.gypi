# Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'target_defaults': {
    'configurations': {
      'Profile': {
        'xcode_settings': {
          'DEBUG_INFORMARTION_FORMAT': 'dwarf-with-dsym',
          # We manually strip using strip -S and strip -x. We need to run
          # dsymutil ourselves so we need symbols around before that.
          'DEPLOYMENT_POSTPROCESSING': 'NO',
          'GCC_OPTIMIZATION_LEVEL': 's',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
          'STRIP_INSTALLED_PRODUCT': 'NO',
          'USE_HEADERMAP': 'YES',
        },
      },
    },
  },
}
