# Copyright (c) 2012 The WebRTC Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
{
  'conditions': [
    ['OS=="linux" or OS=="win"', {
      'targets': [
        {
         'target_name': 'relayserver',
         'type': 'executable',
         'dependencies': [
           '<(webrtc_root)/base/base.gyp:rtc_base_approved',
           '<(webrtc_root)/pc/pc.gyp:rtc_pc',
         ],
         'sources': [
           'examples/relayserver/relayserver_main.cc',
         ],
       },  # target relayserver
       {
         'target_name': 'stunserver',
         'type': 'executable',
         'dependencies': [
           '<(webrtc_root)/base/base.gyp:rtc_base_approved',
           '<(webrtc_root)/pc/pc.gyp:rtc_pc',
         ],
         'sources': [
           'examples/stunserver/stunserver_main.cc',
         ],
       },  # target stunserver
       {
         'target_name': 'turnserver',
         'type': 'executable',
         'dependencies': [
           '<(webrtc_root)/base/base.gyp:rtc_base_approved',
           '<(webrtc_root)/pc/pc.gyp:rtc_pc',
         ],
         'sources': [
           'examples/turnserver/turnserver_main.cc',
         ],
       },  # target turnserver
       {
         'target_name': 'peerconnection_server',
         'type': 'executable',
         'sources': [
           'examples/peerconnection/server/data_socket.cc',
           'examples/peerconnection/server/data_socket.h',
           'examples/peerconnection/server/main.cc',
           'examples/peerconnection/server/peer_channel.cc',
           'examples/peerconnection/server/peer_channel.h',
           'examples/peerconnection/server/utils.cc',
           'examples/peerconnection/server/utils.h',
         ],
         'dependencies': [
           '<(webrtc_root)/base/base.gyp:rtc_base_approved',
           '<(webrtc_root)/common.gyp:webrtc_common',
           '<(webrtc_root)/tools/internal_tools.gyp:command_line_parser',
         ],
         # TODO(ronghuawu): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4309, ],
       }, # target peerconnection_server
       {
          'target_name': 'peerconnection_client',
          'type': 'executable',
          'sources': [
            'examples/peerconnection/client/conductor.cc',
            'examples/peerconnection/client/conductor.h',
            'examples/peerconnection/client/defaults.cc',
            'examples/peerconnection/client/defaults.h',
            'examples/peerconnection/client/peer_connection_client.cc',
            'examples/peerconnection/client/peer_connection_client.h',
          ],
          'dependencies': [
            'api/api.gyp:libjingle_peerconnection',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
          ],
          'conditions': [
            ['build_json==1', {
              'dependencies': [
                '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
              ],
            }],
            # TODO(ronghuawu): Move these files to a win/ directory then they
            # can be excluded automatically.
            ['OS=="win"', {
              'sources': [
                'examples/peerconnection/client/flagdefs.h',
                'examples/peerconnection/client/main.cc',
                'examples/peerconnection/client/main_wnd.cc',
                'examples/peerconnection/client/main_wnd.h',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                 'SubSystem': '2',  # Windows
                },
              },
            }],  # OS=="win"
            ['OS=="win" and clang==1', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    # Disable warnings failing when compiling with Clang on Windows.
                    # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                    '-Wno-reorder',
                    '-Wno-unused-function',
                  ],
                },
              },
            }], # OS=="win" and clang==1
            ['OS=="linux"', {
              'sources': [
                'examples/peerconnection/client/linux/main.cc',
                'examples/peerconnection/client/linux/main_wnd.cc',
                'examples/peerconnection/client/linux/main_wnd.h',
              ],
              'cflags': [
                '<!@(pkg-config --cflags glib-2.0 gobject-2.0 gtk+-2.0)',
              ],
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other glib-2.0'
                      ' gobject-2.0 gthread-2.0 gtk+-2.0)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l glib-2.0 gobject-2.0'
                      ' gthread-2.0 gtk+-2.0)',
                  '-lX11',
                  '-lXcomposite',
                  '-lXext',
                  '-lXrender',
                ],
              },
            }],  # OS=="linux"
            ['OS=="linux" and target_arch=="ia32"', {
              'cflags': [
                '-Wno-sentinel',
              ],
            }],  # OS=="linux" and target_arch=="ia32"
          ],  # conditions
        },  # target peerconnection_client
      ], # targets
    }],  # OS=="linux" or OS=="win"

    ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
      'targets': [
        {
          'target_name': 'apprtc_common',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_common_objc',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
          ],
          'sources': [
            'examples/objc/AppRTCMobile/common/ARDUtilities.h',
            'examples/objc/AppRTCMobile/common/ARDUtilities.m',
          ],
          'include_dirs': [
            'examples/objc/AppRTCMobile/common',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCMobile/common',
            ],
          },
          'conditions': [
            ['OS=="ios"', {
              'xcode_settings': {
                'WARNING_CFLAGS':  [
                  # Suppress compiler warnings about deprecated that triggered
                  # when moving from ios_deployment_target 7.0 to 9.0.
                  # See webrtc:5549 for more details.
                  '-Wno-deprecated-declarations',
                ],
              },
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework QuartzCore',
              ],
            },
          },
        },
        {
          'target_name': 'apprtc_signaling',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_peerconnection_objc',
            'apprtc_common',
            'socketrocket',
          ],
          'sources': [
            'examples/objc/AppRTCMobile/ARDAppClient.h',
            'examples/objc/AppRTCMobile/ARDAppClient.m',
            'examples/objc/AppRTCMobile/ARDAppClient+Internal.h',
            'examples/objc/AppRTCMobile/ARDAppEngineClient.h',
            'examples/objc/AppRTCMobile/ARDAppEngineClient.m',
            'examples/objc/AppRTCMobile/ARDBitrateTracker.h',
            'examples/objc/AppRTCMobile/ARDBitrateTracker.m',
            'examples/objc/AppRTCMobile/ARDCEODTURNClient.h',
            'examples/objc/AppRTCMobile/ARDCEODTURNClient.m',
            'examples/objc/AppRTCMobile/ARDJoinResponse.h',
            'examples/objc/AppRTCMobile/ARDJoinResponse.m',
            'examples/objc/AppRTCMobile/ARDJoinResponse+Internal.h',
            'examples/objc/AppRTCMobile/ARDMessageResponse.h',
            'examples/objc/AppRTCMobile/ARDMessageResponse.m',
            'examples/objc/AppRTCMobile/ARDMessageResponse+Internal.h',
            'examples/objc/AppRTCMobile/ARDRoomServerClient.h',
            'examples/objc/AppRTCMobile/ARDSDPUtils.h',
            'examples/objc/AppRTCMobile/ARDSDPUtils.m',
            'examples/objc/AppRTCMobile/ARDSignalingChannel.h',
            'examples/objc/AppRTCMobile/ARDSignalingMessage.h',
            'examples/objc/AppRTCMobile/ARDSignalingMessage.m',
            'examples/objc/AppRTCMobile/ARDStatsBuilder.h',
            'examples/objc/AppRTCMobile/ARDStatsBuilder.m',
            'examples/objc/AppRTCMobile/ARDTURNClient.h',
            'examples/objc/AppRTCMobile/ARDWebSocketChannel.h',
            'examples/objc/AppRTCMobile/ARDWebSocketChannel.m',
            'examples/objc/AppRTCMobile/RTCIceCandidate+JSON.h',
            'examples/objc/AppRTCMobile/RTCIceCandidate+JSON.m',
            'examples/objc/AppRTCMobile/RTCIceServer+JSON.h',
            'examples/objc/AppRTCMobile/RTCIceServer+JSON.m',
            'examples/objc/AppRTCMobile/RTCMediaConstraints+JSON.h',
            'examples/objc/AppRTCMobile/RTCMediaConstraints+JSON.m',
            'examples/objc/AppRTCMobile/RTCSessionDescription+JSON.h',
            'examples/objc/AppRTCMobile/RTCSessionDescription+JSON.m',
          ],
          'include_dirs': [
            'examples/objc/AppRTCMobile',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCMobile',
            ],
          },
          'export_dependent_settings': [
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_peerconnection_objc',
          ],
          'conditions': [
            ['OS=="ios"', {
              'xcode_settings': {
                'WARNING_CFLAGS':  [
                  # Suppress compiler warnings about deprecated that triggered
                  # when moving from ios_deployment_target 7.0 to 9.0.
                  # See webrtc:5549 for more details.
                  '-Wno-deprecated-declarations',
                ],
              },
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
              },
            }],
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
        },
        {
          'target_name': 'AppRTCMobile',
          'type': 'executable',
          'product_name': 'AppRTCMobile',
          'mac_bundle': 1,
          'dependencies': [
            'apprtc_common',
            'apprtc_signaling',
          ],
          'conditions': [
            ['OS=="ios"', {
              'mac_bundle_resources': [
                'examples/objc/AppRTCMobile/ios/resources/Roboto-Regular.ttf',
                'examples/objc/AppRTCMobile/ios/resources/iPhone5@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/iPhone6@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/iPhone6p@3x.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_call_end_black_24dp.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_call_end_black_24dp@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_clear_black_24dp.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_clear_black_24dp@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_surround_sound_black_24dp.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_surround_sound_black_24dp@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_switch_video_black_24dp.png',
                'examples/objc/AppRTCMobile/ios/resources/ic_switch_video_black_24dp@2x.png',
                'examples/objc/AppRTCMobile/ios/resources/mozart.mp3',
                'examples/objc/Icon.png',
              ],
              'sources': [
                'examples/objc/AppRTCMobile/ios/ARDAppDelegate.h',
                'examples/objc/AppRTCMobile/ios/ARDAppDelegate.m',
                'examples/objc/AppRTCMobile/ios/ARDMainView.h',
                'examples/objc/AppRTCMobile/ios/ARDMainView.m',
                'examples/objc/AppRTCMobile/ios/ARDMainViewController.h',
                'examples/objc/AppRTCMobile/ios/ARDMainViewController.m',
                'examples/objc/AppRTCMobile/ios/ARDStatsView.h',
                'examples/objc/AppRTCMobile/ios/ARDStatsView.m',
                'examples/objc/AppRTCMobile/ios/ARDVideoCallView.h',
                'examples/objc/AppRTCMobile/ios/ARDVideoCallView.m',
                'examples/objc/AppRTCMobile/ios/ARDVideoCallViewController.h',
                'examples/objc/AppRTCMobile/ios/ARDVideoCallViewController.m',
                'examples/objc/AppRTCMobile/ios/AppRTCMobile-Prefix.pch',
                'examples/objc/AppRTCMobile/ios/UIImage+ARDUtilities.h',
                'examples/objc/AppRTCMobile/ios/UIImage+ARDUtilities.m',
                'examples/objc/AppRTCMobile/ios/main.m',
              ],
              'xcode_settings': {
                'INFOPLIST_FILE': 'examples/objc/AppRTCMobile/ios/Info.plist',
                'WARNING_CFLAGS':  [
                  # Suppress compiler warnings about deprecated that triggered
                  # when moving from ios_deployment_target 7.0 to 9.0.
                  # See webrtc:5549 for more details.
                  '-Wno-deprecated-declarations',
                ],
              },
            }],
            ['OS=="mac"', {
              'sources': [
                'examples/objc/AppRTCMobile/mac/APPRTCAppDelegate.h',
                'examples/objc/AppRTCMobile/mac/APPRTCAppDelegate.m',
                'examples/objc/AppRTCMobile/mac/APPRTCViewController.h',
                'examples/objc/AppRTCMobile/mac/APPRTCViewController.m',
                'examples/objc/AppRTCMobile/mac/main.m',
              ],
              'xcode_settings': {
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'INFOPLIST_FILE': 'examples/objc/AppRTCMobile/mac/Info.plist',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
                'OTHER_LDFLAGS': [
                  '-framework AVFoundation',
                ],
              },
            }],
            ['target_arch=="ia32"', {
              'dependencies' : [
                '<(DEPTH)/testing/iossim/iossim.gyp:iossim#host',
              ],
            }],
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
          },
        },  # target AppRTCMobile
        {
          # TODO(tkchin): move this into the real third party location and
          # have it mirrored on chrome infra.
          'target_name': 'socketrocket',
          'type': 'static_library',
          'sources': [
            'examples/objc/AppRTCMobile/third_party/SocketRocket/SRWebSocket.h',
            'examples/objc/AppRTCMobile/third_party/SocketRocket/SRWebSocket.m',
          ],
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                # SocketRocket autosynthesizes some properties. Disable the
                # warning so we can compile successfully.
                'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'NO',
                'MACOSX_DEPLOYMENT_TARGET' : '10.8',
                # SRWebSocket.m uses code with partial availability.
                # https://code.google.com/p/webrtc/issues/detail?id=4695
                'WARNING_CFLAGS!': [
                  '-Wpartial-availability',
                ],
              },
            }],
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'examples/objc/AppRTCMobile/third_party/SocketRocket',
            ],
          },
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'WARNING_CFLAGS': [
              '-Wno-deprecated-declarations',
              '-Wno-nonnull',
              # Hide the warning for SecRandomCopyBytes(), till we update
              # to upstream.
              # https://bugs.chromium.org/p/webrtc/issues/detail?id=6396
              '-Wno-unused-result',
            ],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework CFNetwork',
                '-licucore',
              ],
            },
          }
        },  # target socketrocket
      ],  # targets
    }],  # OS=="ios" or (OS=="mac" and target_arch!="ia32")

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'AppRTCMobile',
          'type': 'none',
          'dependencies': [
            'api/api_java.gyp:libjingle_peerconnection_java',
          ],
          'variables': {
            'apk_name': 'AppRTCMobile',
            'java_in_dir': 'examples/androidapp',
            'has_java_resources': 1,
            'resource_dir': 'examples/androidapp/res',
            'R_package': 'org.appspot.apprtc',
            'R_package_relpath': 'org/appspot/apprtc',
            'input_jars_paths': [
              'examples/androidapp/third_party/autobanh/lib/autobanh.jar',
             ],
            'library_dexed_jars_paths': [
              'examples/androidapp/third_party/autobanh/lib/autobanh.jar',
             ],
            'native_lib_target': 'libjingle_peerconnection_so',
            'add_to_dependents_classpaths':1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # target AppRTCMobile

        {
          # AppRTCMobile creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # AppRTCMobile_apk. Dependents of AppRTCMobile_apk receive its
          # jar path in the variable 'apk_output_jar_path'.
          # This target should only be used by targets which instrument
          # AppRTCMobile_apk.
          'target_name': 'AppRTCMobile_apk',
          'type': 'none',
          'dependencies': [
             'AppRTCMobile',
           ],
           'includes': [ '../build/apk_fake_jar.gypi' ],
        },  # target AppRTCMobile_apk

        {
          'target_name': 'AppRTCMobileTest',
          'type': 'none',
          'dependencies': [
            'AppRTCMobile_apk',
           ],
          'variables': {
            'apk_name': 'AppRTCMobileTest',
            'java_in_dir': 'examples/androidtests',
            'is_test_apk': 1,
            'test_type': 'instrumentation',
            'test_runner_path': '<(DEPTH)/webrtc/build/android/test_runner.py',
          },
          'includes': [
            '../build/java_apk.gypi',
            '../build/android/test_runner.gypi',
          ],
        },
      ],  # targets
    }],  # OS=="android"
  ],
}
