# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is a copy of the deleted build/filename_includes.gypi that
# has now been dropped from Chromium.
{
  'target_conditions': [
    ['OS!="win" or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_win(_browsertest|_unittest|_test)?\\.(h|cc)$'],
                    ['exclude', '(^|/)win/'],
                    ['exclude', '(^|/)win_[^/]*\\.(h|cc)$'] ],
    }],
    ['OS!="mac" or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_(cocoa|mac|mach)(_unittest|_test)?\\.(h|cc|c|mm?)$'],
                    ['exclude', '(^|/)(cocoa|mac|mach)/'] ],
    }],
    ['OS!="ios" or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_ios(_unittest|_test)?\\.(h|cc|mm?)$'],
                    ['exclude', '(^|/)ios/'] ],
    }],
    ['(OS!="mac" and OS!="ios") or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '\\.mm?$' ] ],
    }],
    # Do not exclude the linux files on *BSD since most of them can be
    # shared at this point.
    # In case a file is not needed, it is going to be excluded later on.
    # TODO(evan): the above is not correct; we shouldn't build _linux
    # files on non-linux.
    ['OS!="linux" and OS!="openbsd" and OS!="freebsd" or >(nacl_untrusted_build)==1', {
      'sources/': [
        ['exclude', '_linux(_unittest|_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)linux/'],
      ],
    }],
    ['OS!="android" or _toolset=="host" or >(nacl_untrusted_build)==1', {
      'sources/': [
        ['exclude', '_android(_unittest|_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)android/'],
      ],
    }],
    ['OS=="win" and >(nacl_untrusted_build)==0', {
      'sources/': [
        ['exclude', '_posix(_unittest|_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)posix/'],
      ],
    }],
    ['<(chromeos)!=1 or >(nacl_untrusted_build)==1', {
      'sources/': [
        ['exclude', '_chromeos(_unittest|_test)?\\.(h|cc)$'],
        ['exclude', '(^|/)chromeos/'],
      ],
    }],
    ['>(nacl_untrusted_build)==0', {
      'sources/': [
        ['exclude', '_nacl(_unittest)?\\.(h|cc)$'],
      ],
    }],
    ['OS!="linux" and OS!="openbsd" and OS!="freebsd" or >(nacl_untrusted_build)==1', {
      'sources/': [
        ['exclude', '_xdg(_unittest)?\\.(h|cc)$'],
      ],
    }],
    ['<(use_x11)!=1 or >(nacl_untrusted_build)==1', {
      'sources/': [
        ['exclude', '_(x|x11)(_interactive_uitest|_unittest)?\\.(h|cc)$'],
        ['exclude', '(^|/)x11_[^/]*\\.(h|cc)$'],
        ['exclude', '(^|/)x11/'],
        ['exclude', '(^|/)x/'],
      ],
    }],
    ['<(toolkit_views)==0 or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_views(_browsertest|_unittest)?\\.(h|cc)$'] ]
    }],
    ['<(use_aura)==0 or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_aura(_browsertest|_unittest)?\\.(h|cc)$'],
                    ['exclude', '(^|/)aura/'],
                    ['exclude', '_ash(_browsertest|_unittest)?\\.(h|cc)$'],
                    ['exclude', '(^|/)ash/'],
      ]
    }],
    ['<(use_aura)==0 or <(use_x11)==0 or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_aurax11(_browsertest|_unittest)?\\.(h|cc)$'] ]
    }],
    ['<(use_aura)==0 or OS!="win" or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_aurawin\\.(h|cc)$'],
                    ['exclude', '_ashwin\\.(h|cc)$']
      ]
    }],
    ['<(use_aura)==0 or OS!="linux" or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_auralinux\\.(h|cc)$'] ]
    }],
    ['<(use_ozone)==0 or >(nacl_untrusted_build)==1', {
      'sources/': [ ['exclude', '_ozone(_browsertest|_unittest)?\\.(h|cc)$'] ]
    }],
    ['<(use_pango)==0', {
      'sources/': [ ['exclude', '(^|_)pango(_util|_browsertest|_unittest)?\\.(h|cc)$'], ],
    }],
  ]
}

