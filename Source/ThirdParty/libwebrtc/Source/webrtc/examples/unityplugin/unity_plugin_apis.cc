/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/unityplugin/unity_plugin_apis.h"

#include <map>
#include <string>

#include "webrtc/examples/unityplugin/simple_peer_connection.h"

namespace {
static int g_peer_connection_id = 1;
static std::map<int, rtc::scoped_refptr<SimplePeerConnection>>
    g_peer_connection_map;
}  // namespace

int CreatePeerConnection() {
  g_peer_connection_map[g_peer_connection_id] =
      new rtc::RefCountedObject<SimplePeerConnection>();

  if (!g_peer_connection_map[g_peer_connection_id]->InitializePeerConnection(
          false))
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

// Register callback functions.
bool RegisterOnVideoFramReady(int peer_connection_id,
                              VIDEOFRAMEREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnVideoFramReady(callback);
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

int ReceivedSdp(int peer_connection_id, const char* sdp) {
  // Create a peer_connection if no one exists.
  int id = -1;
  if (g_peer_connection_map.count(peer_connection_id)) {
    id = peer_connection_id;
  } else {
    id = g_peer_connection_id++;
    g_peer_connection_map[id] =
        new rtc::RefCountedObject<SimplePeerConnection>();
    if (!g_peer_connection_map[id]->InitializePeerConnection(true))
      return -1;
  }

  g_peer_connection_map[id]->ReceivedSdp(sdp);
  return id;
}

bool ReceivedIceCandidate(int peer_connection_id, const char* ice_candidate) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->ReceivedIceCandidate(
      ice_candidate);
}
