# Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../build/common.gypi',
    'sdk.gypi',
  ],
  'conditions': [
    ['OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7")', {
      'targets': [
        {
          'target_name': 'rtc_sdk_common_objc',
          'type': 'static_library',
          'includes': [ '../build/objc_common.gypi' ],
          'dependencies': [
            '../base/base.gyp:rtc_base',
          ],
          'include_dirs': [
            'objc/Framework/Classes',
            'objc/Framework/Headers',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'objc/Framework/Classes',
              'objc/Framework/Headers',
            ],
          },
          'sources': [
            'objc/Framework/Classes/NSString+StdString.h',
            'objc/Framework/Classes/NSString+StdString.mm',
            'objc/Framework/Classes/RTCDispatcher.m',
            'objc/Framework/Classes/RTCFieldTrials.mm',
            'objc/Framework/Classes/RTCLogging.mm',
            'objc/Framework/Classes/RTCMetrics.mm',
            'objc/Framework/Classes/RTCMetricsSampleInfo+Private.h',
            'objc/Framework/Classes/RTCMetricsSampleInfo.mm',
            'objc/Framework/Classes/RTCSSLAdapter.mm',
            'objc/Framework/Classes/RTCTracing.mm',
            'objc/Framework/Headers/WebRTC/RTCDispatcher.h',
            'objc/Framework/Headers/WebRTC/RTCFieldTrials.h',
            'objc/Framework/Headers/WebRTC/RTCLogging.h',
            'objc/Framework/Headers/WebRTC/RTCMacros.h',
            'objc/Framework/Headers/WebRTC/RTCMetrics.h',
            'objc/Framework/Headers/WebRTC/RTCMetricsSampleInfo.h',
            'objc/Framework/Headers/WebRTC/RTCSSLAdapter.h',
            'objc/Framework/Headers/WebRTC/RTCTracing.h',
          ],
          'conditions': [
            ['OS=="ios"', {
              'sources': [
                'objc/Framework/Classes/RTCCameraPreviewView.m',
                'objc/Framework/Classes/RTCUIApplication.h',
                'objc/Framework/Classes/RTCUIApplication.mm',
                'objc/Framework/Classes/UIDevice+RTCDevice.mm',
                'objc/Framework/Headers/WebRTC/RTCCameraPreviewView.h',
                'objc/Framework/Headers/WebRTC/UIDevice+RTCDevice.h',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework AVFoundation',
                  ],
                },
              },
            }],  # OS=="ios"
            ['build_with_chromium==0', {
              'sources': [
                'objc/Framework/Classes/RTCFileLogger.mm',
                'objc/Framework/Headers/WebRTC/RTCFileLogger.h',
              ],
            }],
          ],
        },
        {
          'target_name': 'rtc_sdk_peerconnection_objc',
          'type': 'static_library',
          'includes': [ '../build/objc_common.gypi' ],
          'dependencies': [
            '<(webrtc_root)/api/api.gyp:libjingle_peerconnection',
            'rtc_sdk_common_objc',
          ],
          'include_dirs': [
            'objc/Framework/Classes',
            'objc/Framework/Headers',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'objc/Framework/Classes',
              'objc/Framework/Headers',
            ],
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework AVFoundation',
              ],
            },
            'libraries': [
              '-lstdc++',
            ],
          }, # link_settings
          'sources': [
            'objc/Framework/Classes/RTCAVFoundationVideoSource+Private.h',
            'objc/Framework/Classes/RTCAVFoundationVideoSource.mm',
            'objc/Framework/Classes/RTCAudioSource+Private.h',
            'objc/Framework/Classes/RTCAudioSource.mm',
            'objc/Framework/Classes/RTCAudioTrack+Private.h',
            'objc/Framework/Classes/RTCAudioTrack.mm',
            'objc/Framework/Classes/RTCConfiguration+Private.h',
            'objc/Framework/Classes/RTCConfiguration.mm',
            'objc/Framework/Classes/RTCDataChannel+Private.h',
            'objc/Framework/Classes/RTCDataChannel.mm',
            'objc/Framework/Classes/RTCDataChannelConfiguration+Private.h',
            'objc/Framework/Classes/RTCDataChannelConfiguration.mm',
            'objc/Framework/Classes/RTCI420Shader.mm',
            'objc/Framework/Classes/RTCIceCandidate+Private.h',
            'objc/Framework/Classes/RTCIceCandidate.mm',
            'objc/Framework/Classes/RTCIceServer+Private.h',
            'objc/Framework/Classes/RTCIceServer.mm',
            'objc/Framework/Classes/RTCLegacyStatsReport+Private.h',
            'objc/Framework/Classes/RTCLegacyStatsReport.mm',
            'objc/Framework/Classes/RTCMediaConstraints+Private.h',
            'objc/Framework/Classes/RTCMediaConstraints.mm',
            'objc/Framework/Classes/RTCMediaSource+Private.h',
            'objc/Framework/Classes/RTCMediaSource.mm',
            'objc/Framework/Classes/RTCMediaStream+Private.h',
            'objc/Framework/Classes/RTCMediaStream.mm',
            'objc/Framework/Classes/RTCMediaStreamTrack+Private.h',
            'objc/Framework/Classes/RTCMediaStreamTrack.mm',
            'objc/Framework/Classes/RTCOpenGLDefines.h',
            'objc/Framework/Classes/RTCOpenGLVideoRenderer.h',
            'objc/Framework/Classes/RTCOpenGLVideoRenderer.mm',
            'objc/Framework/Classes/RTCPeerConnection+DataChannel.mm',
            'objc/Framework/Classes/RTCPeerConnection+Private.h',
            'objc/Framework/Classes/RTCPeerConnection+Stats.mm',
            'objc/Framework/Classes/RTCPeerConnection.mm',
            'objc/Framework/Classes/RTCPeerConnectionFactory+Private.h',
            'objc/Framework/Classes/RTCPeerConnectionFactory.mm',
            'objc/Framework/Classes/RTCRtpCodecParameters+Private.h',
            'objc/Framework/Classes/RTCRtpCodecParameters.mm',
            'objc/Framework/Classes/RTCRtpEncodingParameters+Private.h',
            'objc/Framework/Classes/RTCRtpEncodingParameters.mm',
            'objc/Framework/Classes/RTCRtpParameters+Private.h',
            'objc/Framework/Classes/RTCRtpParameters.mm',
            'objc/Framework/Classes/RTCRtpReceiver+Private.h',
            'objc/Framework/Classes/RTCRtpReceiver.mm',
            'objc/Framework/Classes/RTCRtpSender+Private.h',
            'objc/Framework/Classes/RTCRtpSender.mm',
            'objc/Framework/Classes/RTCSessionDescription+Private.h',
            'objc/Framework/Classes/RTCSessionDescription.mm',
            'objc/Framework/Classes/RTCShader+Private.h',
            'objc/Framework/Classes/RTCShader.h',
            'objc/Framework/Classes/RTCShader.mm',
            'objc/Framework/Classes/RTCVideoFrame+Private.h',
            'objc/Framework/Classes/RTCVideoFrame.mm',
            'objc/Framework/Classes/RTCVideoRendererAdapter+Private.h',
            'objc/Framework/Classes/RTCVideoRendererAdapter.h',
            'objc/Framework/Classes/RTCVideoRendererAdapter.mm',
            'objc/Framework/Classes/RTCVideoSource+Private.h',
            'objc/Framework/Classes/RTCVideoSource.mm',
            'objc/Framework/Classes/RTCVideoTrack+Private.h',
            'objc/Framework/Classes/RTCVideoTrack.mm',
            'objc/Framework/Classes/avfoundationvideocapturer.h',
            'objc/Framework/Classes/avfoundationvideocapturer.mm',
            'objc/Framework/Headers/WebRTC/RTCAVFoundationVideoSource.h',
            'objc/Framework/Headers/WebRTC/RTCAudioSource.h',
            'objc/Framework/Headers/WebRTC/RTCAudioTrack.h',
            'objc/Framework/Headers/WebRTC/RTCConfiguration.h',
            'objc/Framework/Headers/WebRTC/RTCDataChannel.h',
            'objc/Framework/Headers/WebRTC/RTCDataChannelConfiguration.h',
            'objc/Framework/Headers/WebRTC/RTCIceCandidate.h',
            'objc/Framework/Headers/WebRTC/RTCIceServer.h',
            'objc/Framework/Headers/WebRTC/RTCLegacyStatsReport.h',
            'objc/Framework/Headers/WebRTC/RTCMediaConstraints.h',
            'objc/Framework/Headers/WebRTC/RTCMediaSource.h',
            'objc/Framework/Headers/WebRTC/RTCMediaStream.h',
            'objc/Framework/Headers/WebRTC/RTCMediaStreamTrack.h',
            'objc/Framework/Headers/WebRTC/RTCPeerConnection.h',
            'objc/Framework/Headers/WebRTC/RTCPeerConnectionFactory.h',
            'objc/Framework/Headers/WebRTC/RTCRtpCodecParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpEncodingParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpReceiver.h',
            'objc/Framework/Headers/WebRTC/RTCRtpSender.h',
            'objc/Framework/Headers/WebRTC/RTCSessionDescription.h',
            'objc/Framework/Headers/WebRTC/RTCVideoFrame.h',
            'objc/Framework/Headers/WebRTC/RTCVideoRenderer.h',
            'objc/Framework/Headers/WebRTC/RTCVideoSource.h',
            'objc/Framework/Headers/WebRTC/RTCVideoTrack.h',
          ], # sources
          'conditions': [
            ['build_libyuv==1', {
              'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv'],
            }],
            ['OS=="ios"', {
              'sources': [
                'objc/Framework/Classes/RTCEAGLVideoView.m',
                'objc/Framework/Classes/RTCNativeNV12Shader.mm',
                'objc/Framework/Headers/WebRTC/RTCEAGLVideoView.h',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework CoreGraphics',
                    '-framework GLKit',
                    '-framework OpenGLES',
                    '-framework QuartzCore',
                  ],
                },
              },  # link_settings
            }],  # OS=="ios"
            ['OS=="mac"', {
              'sources': [
                'objc/Framework/Classes/RTCNSGLVideoView.m',
                'objc/Framework/Headers/WebRTC/RTCNSGLVideoView.h',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework CoreMedia',
                    '-framework OpenGL',
                  ],
                },
              },
            }],
          ],  # conditions
        },  # rtc_sdk_peerconnection_objc
        {
          'target_name': 'rtc_sdk_framework_objc',
          'type': 'shared_library',
          'product_name': 'WebRTC',
          'mac_bundle': 1,
          'includes': [ '../build/objc_common.gypi' ],
          # Slightly hacky, but we need to re-declare files here that are C
          # interfaces because otherwise they will be dead-stripped during
          # linking (ObjC classes cannot be dead-stripped). We might consider
          # just only using ObjC interfaces.
          'sources': [
            'objc/Framework/Classes/RTCFieldTrials.mm',
            'objc/Framework/Classes/RTCLogging.mm',
            'objc/Framework/Classes/RTCMetrics.mm',
            'objc/Framework/Classes/RTCSSLAdapter.mm',
            'objc/Framework/Classes/RTCTracing.mm',
            'objc/Framework/Headers/WebRTC/RTCFieldTrials.h',
            'objc/Framework/Headers/WebRTC/RTCLogging.h',
            'objc/Framework/Headers/WebRTC/RTCSSLAdapter.h',
            'objc/Framework/Headers/WebRTC/RTCTracing.h',
            'objc/Framework/Headers/WebRTC/WebRTC.h',
            'objc/Framework/Modules/module.modulemap',
          ],
          'mac_framework_headers': [
            'objc/Framework/Headers/WebRTC/RTCAudioSource.h',
            'objc/Framework/Headers/WebRTC/RTCAudioTrack.h',
            'objc/Framework/Headers/WebRTC/RTCAVFoundationVideoSource.h',
            'objc/Framework/Headers/WebRTC/RTCCameraPreviewView.h',
            'objc/Framework/Headers/WebRTC/RTCConfiguration.h',
            'objc/Framework/Headers/WebRTC/RTCDataChannel.h',
            'objc/Framework/Headers/WebRTC/RTCDataChannelConfiguration.h',
            'objc/Framework/Headers/WebRTC/RTCDispatcher.h',
            'objc/Framework/Headers/WebRTC/RTCEAGLVideoView.h',
            'objc/Framework/Headers/WebRTC/RTCFieldTrials.h',
            'objc/Framework/Headers/WebRTC/RTCFileLogger.h',
            'objc/Framework/Headers/WebRTC/RTCIceCandidate.h',
            'objc/Framework/Headers/WebRTC/RTCIceServer.h',
            'objc/Framework/Headers/WebRTC/RTCLegacyStatsReport.h',
            'objc/Framework/Headers/WebRTC/RTCLogging.h',
            'objc/Framework/Headers/WebRTC/RTCMacros.h',
            'objc/Framework/Headers/WebRTC/RTCMediaConstraints.h',
            'objc/Framework/Headers/WebRTC/RTCMediaSource.h',
            'objc/Framework/Headers/WebRTC/RTCMediaStream.h',
            'objc/Framework/Headers/WebRTC/RTCMediaStreamTrack.h',
            'objc/Framework/Headers/WebRTC/RTCMetrics.h',
            'objc/Framework/Headers/WebRTC/RTCMetricsSampleInfo.h',
            'objc/Framework/Headers/WebRTC/RTCNSGLVideoView.h',
            'objc/Framework/Headers/WebRTC/RTCPeerConnection.h',
            'objc/Framework/Headers/WebRTC/RTCPeerConnectionFactory.h',
            'objc/Framework/Headers/WebRTC/RTCRtpCodecParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpEncodingParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpParameters.h',
            'objc/Framework/Headers/WebRTC/RTCRtpReceiver.h',
            'objc/Framework/Headers/WebRTC/RTCRtpSender.h',
            'objc/Framework/Headers/WebRTC/RTCSessionDescription.h',
            'objc/Framework/Headers/WebRTC/RTCSSLAdapter.h',
            'objc/Framework/Headers/WebRTC/RTCTracing.h',
            'objc/Framework/Headers/WebRTC/RTCVideoFrame.h',
            'objc/Framework/Headers/WebRTC/RTCVideoRenderer.h',
            'objc/Framework/Headers/WebRTC/RTCVideoSource.h',
            'objc/Framework/Headers/WebRTC/RTCVideoTrack.h',
            'objc/Framework/Headers/WebRTC/UIDevice+RTCDevice.h',
            'objc/Framework/Headers/WebRTC/WebRTC.h',
          ],
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            'rtc_sdk_peerconnection_objc',
          ],
          'xcode_settings': {
            'CODE_SIGNING_REQUIRED': 'NO',
            'CODE_SIGN_IDENTITY': '',
            'DEFINES_MODULE': 'YES',
            'INFOPLIST_FILE': 'objc/Framework/Info.plist',
            'LD_DYLIB_INSTALL_NAME': '@rpath/WebRTC.framework/WebRTC',
            'MODULEMAP_FILE': '<(webrtc_root)/sdk/Framework/Modules/module.modulemap',
          },
          'link_settings': {
            'xcode_settings': {
              'OTHER_LDFLAGS': [
                '-framework AVFoundation',
                '-framework AudioToolbox',
                '-framework CoreGraphics',
                '-framework CoreMedia',
                '-framework GLKit',
                '-framework VideoToolbox',
              ],
            },
          },  # link_settings
          'conditions': [
            # TODO(tkchin): Generate WebRTC.h based off of
            # mac_framework_headers instead of hard-coding. Ok for now since we
            # only really care about dynamic lib on iOS outside of chromium.
            ['OS!="mac"', {
              'mac_framework_headers!': [
                'objc/Framework/Headers/WebRTC/RTCNSGLVideoView.h',
              ],
            }],
            ['build_with_chromium==1', {
              'mac_framework_headers!': [
                'objc/Framework/Headers/WebRTC/RTCFileLogger.h',
              ],
            }],
          ],  # conditions
        }, # rtc_sdk_framework_objc
      ],  # targets
    }],  # OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7")
  ],
}
