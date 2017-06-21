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

#ifndef WEBRTC_EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_
#define WEBRTC_EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_

#include <stdint.h>

// Defintions of callback functions.
typedef void (*VIDEOFRAMEREADY_CALLBACK)(uint8_t* buffer,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride);
typedef void (*LOCALDATACHANNELREADY_CALLBACK)();
typedef void (*DATAFROMEDATECHANNELREADY_CALLBACK)(const char* msg);
typedef void (*FAILURE_CALLBACK)(const char* msg);
typedef void (*LOCALSDPREADYTOSEND_CALLBACK)(const char* msg);
typedef void (*ICECANDIDATEREADYTOSEND_CALLBACK)(const char* msg);
typedef void (*AUDIOBUSREADY_CALLBACK)(const void* audio_data,
                                       int bits_per_sample,
                                       int sample_rate,
                                       int number_of_channels,
                                       int number_of_frames);

#define WEBRTC_PLUGIN_API __declspec(dllexport)
extern "C" {
// Create a peerconnection and return a unique peer connection id.
WEBRTC_PLUGIN_API int CreatePeerConnection();
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

// Register callback functions.
WEBRTC_PLUGIN_API bool RegisterOnVideoFramReady(
    int peer_connection_id,
    VIDEOFRAMEREADY_CALLBACK callback);
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
WEBRTC_PLUGIN_API bool RegisterOnIceCandiateReadytoSend(
    int peer_connection_id,
    ICECANDIDATEREADYTOSEND_CALLBACK callback);
WEBRTC_PLUGIN_API int ReceivedSdp(int peer_connection_id, const char* sdp);
WEBRTC_PLUGIN_API bool ReceivedIceCandidate(int peer_connection_id,
                                            const char* ice_candidate);
}

#endif  // WEBRTC_EXAMPLES_UNITYPLUGIN_UNITY_PLUGIN_APIS_H_
