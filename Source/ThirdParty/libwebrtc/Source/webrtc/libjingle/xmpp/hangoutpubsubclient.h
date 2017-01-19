/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_HANGOUTPUBSUBCLIENT_H_
#define WEBRTC_LIBJINGLE_XMPP_HANGOUTPUBSUBCLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/pubsubclient.h"
#include "webrtc/libjingle/xmpp/pubsubstateclient.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sigslotrepeater.h"

// Gives a high-level API for MUC call PubSub needs such as
// presenter state, recording state, mute state, and remote mute.

namespace buzz {

class Jid;
class XmlElement;
class XmppTaskParentInterface;

// A client tied to a specific MUC jid and local nick.  Provides ways
// to get updates and publish state and events.  Must call
// RequestAll() to start getting updates.
class HangoutPubSubClient : public sigslot::has_slots<> {
 public:
  HangoutPubSubClient(XmppTaskParentInterface* parent,
                      const Jid& mucjid,
                      const std::string& nick);
  ~HangoutPubSubClient();
  const Jid& mucjid() const { return mucjid_; }
  const std::string& nick() const { return nick_; }

  // Requests all of the different states and subscribes for updates.
  // Responses and updates will be signalled via the various signals.
  void RequestAll();
  // Signal (nick, was_presenting, is_presenting)
  sigslot::signal3<const std::string&, bool, bool> SignalPresenterStateChange;
  // Signal (nick, was_muted, is_muted)
  sigslot::signal3<const std::string&, bool, bool> SignalAudioMuteStateChange;
  // Signal (nick, was_muted, is_muted)
  sigslot::signal3<const std::string&, bool, bool> SignalVideoMuteStateChange;
  // Signal (nick, was_paused, is_paused)
  sigslot::signal3<const std::string&, bool, bool> SignalVideoPauseStateChange;
  // Signal (nick, was_recording, is_recording)
  sigslot::signal3<const std::string&, bool, bool> SignalRecordingStateChange;
  // Signal (mutee_nick, muter_nick, should_mute_locally)
  sigslot::signal3<const std::string&,
                   const std::string&,
                   bool> SignalRemoteMute;
  // Signal (blockee_nick, blocker_nick)
  sigslot::signal2<const std::string&, const std::string&> SignalMediaBlock;

  // Signal (node, error stanza)
  sigslot::signal2<const std::string&, const XmlElement*> SignalRequestError;

  // On each of these, provide a task_id_out to get the task_id, which
  // can be correlated to the error and result signals.
  void PublishPresenterState(
      bool presenting, std::string* task_id_out = NULL);
  void PublishAudioMuteState(
      bool muted, std::string* task_id_out = NULL);
  void PublishVideoMuteState(
      bool muted, std::string* task_id_out = NULL);
  void PublishVideoPauseState(
      bool paused, std::string* task_id_out = NULL);
  void PublishRecordingState(
      bool recording, std::string* task_id_out = NULL);
  void RemoteMute(
      const std::string& mutee_nick, std::string* task_id_out = NULL);
  void BlockMedia(
      const std::string& blockee_nick, std::string* task_id_out = NULL);

  // Signal task_id
  sigslot::signal1<const std::string&> SignalPublishAudioMuteResult;
  sigslot::signal1<const std::string&> SignalPublishVideoMuteResult;
  sigslot::signal1<const std::string&> SignalPublishVideoPauseResult;
  sigslot::signal1<const std::string&> SignalPublishPresenterResult;
  sigslot::signal1<const std::string&> SignalPublishRecordingResult;
  // Signal (task_id, mutee_nick)
  sigslot::signal2<const std::string&,
                   const std::string&> SignalRemoteMuteResult;
  // Signal (task_id, blockee_nick)
  sigslot::signal2<const std::string&,
                   const std::string&> SignalMediaBlockResult;

  // Signal (task_id, error stanza)
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishAudioMuteError;
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishVideoMuteError;
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishVideoPauseError;
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishPresenterError;
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishRecordingError;
  sigslot::signal2<const std::string&,
                   const XmlElement*> SignalPublishMediaBlockError;
  // Signal (task_id, mutee_nick, error stanza)
  sigslot::signal3<const std::string&,
                   const std::string&,
                   const XmlElement*> SignalRemoteMuteError;
  // Signal (task_id, blockee_nick, error stanza)
  sigslot::signal3<const std::string&,
                   const std::string&,
                   const XmlElement*> SignalMediaBlockError;


 private:
  void OnPresenterRequestError(PubSubClient* client,
                               const XmlElement* stanza);
  void OnMediaRequestError(PubSubClient* client,
                           const XmlElement* stanza);

  void OnPresenterStateChange(const PubSubStateChange<bool>& change);
  void OnPresenterPublishResult(const std::string& task_id,
                               const XmlElement* item);
  void OnPresenterPublishError(const std::string& task_id,
                               const XmlElement* item,
                               const XmlElement* stanza);
  void OnAudioMuteStateChange(const PubSubStateChange<bool>& change);
  void OnAudioMutePublishResult(const std::string& task_id,
                               const XmlElement* item);
  void OnAudioMutePublishError(const std::string& task_id,
                               const XmlElement* item,
                               const XmlElement* stanza);
  void OnVideoMuteStateChange(const PubSubStateChange<bool>& change);
  void OnVideoMutePublishResult(const std::string& task_id,
                               const XmlElement* item);
  void OnVideoMutePublishError(const std::string& task_id,
                               const XmlElement* item,
                               const XmlElement* stanza);
  void OnVideoPauseStateChange(const PubSubStateChange<bool>& change);
  void OnVideoPausePublishResult(const std::string& task_id,
                               const XmlElement* item);
  void OnVideoPausePublishError(const std::string& task_id,
                               const XmlElement* item,
                               const XmlElement* stanza);
  void OnRecordingStateChange(const PubSubStateChange<bool>& change);
  void OnRecordingPublishResult(const std::string& task_id,
                               const XmlElement* item);
  void OnRecordingPublishError(const std::string& task_id,
                               const XmlElement* item,
                               const XmlElement* stanza);
  void OnMediaBlockStateChange(const PubSubStateChange<bool>& change);
  void OnMediaBlockPublishResult(const std::string& task_id,
                                 const XmlElement* item);
  void OnMediaBlockPublishError(const std::string& task_id,
                                const XmlElement* item,
                                const XmlElement* stanza);
  Jid mucjid_;
  std::string nick_;
  std::unique_ptr<PubSubClient> media_client_;
  std::unique_ptr<PubSubClient> presenter_client_;
  std::unique_ptr<PubSubStateClient<bool> > presenter_state_client_;
  std::unique_ptr<PubSubStateClient<bool> > audio_mute_state_client_;
  std::unique_ptr<PubSubStateClient<bool> > video_mute_state_client_;
  std::unique_ptr<PubSubStateClient<bool> > video_pause_state_client_;
  std::unique_ptr<PubSubStateClient<bool> > recording_state_client_;
  std::unique_ptr<PubSubStateClient<bool> > media_block_state_client_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_HANGOUTPUBSUBCLIENT_H_
