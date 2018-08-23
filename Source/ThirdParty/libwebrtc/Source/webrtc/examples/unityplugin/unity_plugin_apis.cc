/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/unityplugin/unity_plugin_apis.h"

#include <map>
#include <string>

#include "examples/unityplugin/simple_peer_connection.h"

namespace {
static int g_peer_connection_id = 1;
static std::map<int, rtc::scoped_refptr<SimplePeerConnection>>
    g_peer_connection_map;
}  // namespace

int CreatePeerConnection(const char** turn_urls,
                         const int no_of_urls,
                         const char* username,
                         const char* credential,
                         bool mandatory_receive_video) {
  g_peer_connection_map[g_peer_connection_id] =
      new rtc::RefCountedObject<SimplePeerConnection>();

  if (!g_peer_connection_map[g_peer_connection_id]->InitializePeerConnection(
          turn_urls, no_of_urls, username, credential, mandatory_receive_video))
    return -1;

  return g_peer_connection_id++;
}

bool ClosePeerConnection(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->DeletePeerConnection();
  g_peer_connection_map.erase(peer_connection_id);
  return true;
}

bool AddStream(int peer_connection_id, bool audio_only) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->AddStreams(audio_only);
  return true;
}

bool AddDataChannel(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateDataChannel();
}

bool CreateOffer(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateOffer();
}

bool CreateAnswer(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateAnswer();
}

bool SendDataViaDataChannel(int peer_connection_id, const char* data) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  std::string s(data);
  g_peer_connection_map[peer_connection_id]->SendDataViaDataChannel(s);

  return true;
}

bool SetAudioControl(int peer_connection_id, bool is_mute, bool is_record) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->SetAudioControl(is_mute,
                                                             is_record);
  return true;
}

bool SetRemoteDescription(int peer_connection_id,
                          const char* type,
                          const char* sdp) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->SetRemoteDescription(type,
                                                                         sdp);
}

bool AddIceCandidate(const int peer_connection_id,
                     const char* candidate,
                     const int sdp_mlineindex,
                     const char* sdp_mid) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->AddIceCandidate(
      candidate, sdp_mlineindex, sdp_mid);
}

// Register callback functions.
bool RegisterOnLocalI420FrameReady(int peer_connection_id,
                                   I420FRAMEREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalI420FrameReady(
      callback);
  return true;
}

bool RegisterOnRemoteI420FrameReady(int peer_connection_id,
                                    I420FRAMEREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnRemoteI420FrameReady(
      callback);
  return true;
}

bool RegisterOnLocalDataChannelReady(int peer_connection_id,
                                     LOCALDATACHANNELREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalDataChannelReady(
      callback);
  return true;
}

bool RegisterOnDataFromDataChannelReady(
    int peer_connection_id,
    DATAFROMEDATECHANNELREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnDataFromDataChannelReady(
      callback);
  return true;
}

bool RegisterOnFailure(int peer_connection_id, FAILURE_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnFailure(callback);
  return true;
}

bool RegisterOnAudioBusReady(int peer_connection_id,
                             AUDIOBUSREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnAudioBusReady(callback);
  return true;
}

// Singnaling channel related functions.
bool RegisterOnLocalSdpReadytoSend(int peer_connection_id,
                                   LOCALSDPREADYTOSEND_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalSdpReadytoSend(
      callback);
  return true;
}

bool RegisterOnIceCandiateReadytoSend(
    int peer_connection_id,
    ICECANDIDATEREADYTOSEND_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnIceCandiateReadytoSend(
      callback);
  return true;
}
