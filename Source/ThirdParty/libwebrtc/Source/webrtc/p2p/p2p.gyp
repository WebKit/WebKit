# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_p2p',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
        '<(webrtc_root)/common.gyp:webrtc_common',
      ],
      'sources': [
        'base/asyncstuntcpsocket.cc',
        'base/asyncstuntcpsocket.h',
        'base/basicpacketsocketfactory.cc',
        'base/basicpacketsocketfactory.h',
        'base/candidate.h',
        'base/common.h',
        'base/dtlstransport.h',
        'base/dtlstransportchannel.cc',
        'base/dtlstransportchannel.h',
        'base/p2pconstants.cc',
        'base/p2pconstants.h',
        'base/p2ptransport.cc',
        'base/p2ptransport.h',
        'base/p2ptransportchannel.cc',
        'base/p2ptransportchannel.h',
        'base/packettransportinterface.h',
        'base/packetsocketfactory.h',
        'base/port.cc',
        'base/port.h',
        'base/portallocator.cc',
        'base/portallocator.h',
        'base/portinterface.h',
        'base/pseudotcp.cc',
        'base/pseudotcp.h',
        'base/relayport.cc',
        'base/relayport.h',
        'base/session.cc',
        'base/session.h',
        'base/sessiondescription.cc',
        'base/sessiondescription.h',
        'base/stun.cc',
        'base/stun.h',
        'base/stunport.cc',
        'base/stunport.h',
        'base/stunrequest.cc',
        'base/stunrequest.h',
        'base/tcpport.cc',
        'base/tcpport.h',
        'base/transport.cc',
        'base/transport.h',
        'base/transportchannel.cc',
        'base/transportchannel.h',
        'base/transportchannelimpl.h',
        'base/transportcontroller.cc',
        'base/transportcontroller.h',
        'base/transportdescription.cc',
        'base/transportdescription.h',
        'base/transportdescriptionfactory.cc',
        'base/transportdescriptionfactory.h',
        'base/transportinfo.h',
        'base/turnport.cc',
        'base/turnport.h',
        'base/udpport.h',
        'client/basicportallocator.cc',
        'client/basicportallocator.h',
        'client/socketmonitor.cc',
        'client/socketmonitor.h',
      ],
      'direct_dependent_settings': {
        'defines': [
          'FEATURE_ENABLE_VOICEMAIL',
        ],
      },
      'conditions': [
        ['build_with_chromium==0', {
          'sources': [
            'base/relayserver.cc',
            'base/relayserver.h',
            'base/stunserver.cc',
            'base/stunserver.h',
            'base/turnserver.cc',
            'base/turnserver.h',
          ],
          'defines': [
            'FEATURE_ENABLE_VOICEMAIL',
            'FEATURE_ENABLE_PSTN',
          ],
        }],
        ['use_quic==1', {
      	  'dependencies': [
      	    '<(DEPTH)/third_party/libquic/libquic.gyp:libquic',
      	  ],
          'sources': [
            'quic/quicconnectionhelper.cc',
            'quic/quicconnectionhelper.h',
            'quic/quicsession.cc',
            'quic/quicsession.h',
            'quic/quictransport.cc',
            'quic/quictransport.h',
            'quic/quictransportchannel.cc',
            'quic/quictransportchannel.h',
            'quic/reliablequicstream.cc',
            'quic/reliablequicstream.h',
          ],
      	  'export_dependent_settings': [
      	    '<(DEPTH)/third_party/libquic/libquic.gyp:libquic',
      	  ],
        }],
      ],
    },
    {
      'target_name': 'libstunprober',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
        '<(webrtc_root)/common.gyp:webrtc_common',
      ],
      'sources': [
        'stunprober/stunprober.cc',
      ],
    },
    {
      'target_name': 'stun_prober',
      'type': 'executable',
      'dependencies': [
        'libstunprober',
        'rtc_p2p'
      ],
      'sources': [
        'stunprober/main.cc',
      ],
    },
  ],  # targets
}

