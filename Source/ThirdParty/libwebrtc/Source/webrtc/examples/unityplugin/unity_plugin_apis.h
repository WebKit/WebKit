/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file provides an example of unity native plugin APIs.

#ifndef EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_
#define EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_

#include <stdint.h>

// Definitions of callback functions.
typedef void (*I420FRAMEREADY_CALLBACK)(const uint8_t* data_y,
                                        const uint8_t* data_u,
                                        const uint8_t* data_v,
                                        const uint8_t* data_a,
                                        int stride_y,
                                        int stride_u,
                                        int stride_v,
                                        int stride_a,
                                        uint32_t width,
                                        uint32_t height);
typedef void (*LOCALDATACHANNELREADY_CALLBACK)();
typedef void (*DATAFROMEDATECHANNELREADY_CALLBACK)(const char* msg);
typedef void (*FAILURE_CALLBACK)(const char* msg);
typedef void (*LOCALSDPREADYTOSEND_CALLBACK)(const char* type, const char* sdp);
typedef void (*ICECANDIDATEREADYTOSEND_CALLBACK)(const char* candidate,
                                                 int sdp_mline_index,
                                                 const char* sdp_mid);
typedef void (*AUDIOBUSREADY_CALLBACK)(const void* audio_data,
                                       int bits_per_sample,
                                       int sample_rate,
                                       int number_of_channels,
                                       int number_of_frames);

#if defined(WEBRTC_WIN)
#define WEBRTC_PLUGIN_API __declspec(dllexport)
#elif defined(WEBRTC_ANDROID)
#define WEBRTC_PLUGIN_API __attribute__((visibility("default")))
#endif
extern "C" {
// Create a peerconnection and return a unique peer connection id.
WEBRTC_PLUGIN_API int CreatePeerConnection(const char** turn_urls,
                                           int no_of_urls,
                                           const char* username,
                                           const char* credential,
                                           bool mandatory_receive_video);
// Close a peerconnection.
WEBRTC_PLUGIN_API bool ClosePeerConnection(int peer_connection_id);
// Add a audio stream. If audio_only is true, the stream only has an audio
// track and no video track.
WEBRTC_PLUGIN_API bool AddStream(int peer_connection_id, bool audio_only);
// Add a data channel to peer connection.
WEBRTC_PLUGIN_API bool AddDataChannel(int peer_connection_id);
// Create a peer connection offer.
WEBRTC_PLUGIN_API bool CreateOffer(int peer_connection_id);
// Create a peer connection answer.
WEBRTC_PLUGIN_API bool CreateAnswer(int peer_connection_id);
// Send data through data channel.
WEBRTC_PLUGIN_API bool SendDataViaDataChannel(int peer_connection_id,
                                              const char* data);
// Set audio control. If is_mute=true, no audio will playout. If is_record=true,
// AUDIOBUSREADY_CALLBACK will be called every 10 ms.
WEBRTC_PLUGIN_API bool SetAudioControl(int peer_connection_id,
                                       bool is_mute,
                                       bool is_record);
// Set remote sdp.
WEBRTC_PLUGIN_API bool SetRemoteDescription(int peer_connection_id,
                                            const char* type,
                                            const char* sdp);
// Add ice candidate.
WEBRTC_PLUGIN_API bool AddIceCandidate(int peer_connection_id,
                                       const char* candidate,
                                       int sdp_mlineindex,
                                       const char* sdp_mid);

// Register callback functions.
WEBRTC_PLUGIN_API bool RegisterOnLocalI420FrameReady(
    int peer_connection_id,
    I420FRAMEREADY_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnRemoteI420FrameReady(
    int peer_connection_id,
    I420FRAMEREADY_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnLocalDataChannelReady(
    int peer_connection_id,
    LOCALDATACHANNELREADY_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnDataFromDataChannelReady(
    int peer_connection_id,
    DATAFROMEDATECHANNELREADY_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnFailure(int peer_connection_id,
                                         FAILURE_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnAudioBusReady(int peer_connection_id,
                                               AUDIOBUSREADY_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnLocalSdpReadytoSend(
    int peer_connection_id,
    LOCALSDPREADYTOSEND_CALLBACK callback);
WEBRTC_PLUGIN_API bool RegisterOnIceCandidateReadytoSend(
    int peer_connection_id,
    ICECANDIDATEREADYTOSEND_CALLBACK callback);
}

#endif  // EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_
