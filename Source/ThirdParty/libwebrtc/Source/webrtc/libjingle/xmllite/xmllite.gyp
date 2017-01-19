# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_xmllite',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
      ],
      'conditions': [
        ['build_expat==1', {
          'dependencies': [
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/expat/expat.gyp:expat',
          ],
        }],
      ],
      'sources': [
        'qname.cc',
        'qname.h',
        'xmlbuilder.cc',
        'xmlbuilder.h',
        'xmlconstants.cc',
        'xmlconstants.h',
        'xmlelement.cc',
        'xmlelement.h',
        'xmlnsstack.cc',
        'xmlnsstack.h',
        'xmlparser.cc',
        'xmlparser.h',
        'xmlprinter.cc',
        'xmlprinter.h',
      ],
    },
  ],  # targets
}
