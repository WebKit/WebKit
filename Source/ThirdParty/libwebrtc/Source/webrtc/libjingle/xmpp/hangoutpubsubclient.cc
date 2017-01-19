/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/hangoutpubsubclient.h"

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/base/logging.h"


// Gives a high-level API for MUC call PubSub needs such as
// presenter state, recording state, mute state, and remote mute.

namespace buzz {

namespace {
const char kPresenting[] = "s";
const char kNotPresenting[] = "o";

}  // namespace

// A simple serialiazer where presence of item => true, lack of item
// => false.
class BoolStateSerializer : public PubSubStateSerializer<bool> {
  virtual XmlElement* Write(const QName& state_name, const bool& state) {
    if (!state) {
      return NULL;
    }

    return new XmlElement(state_name, true);
  }

  virtual void Parse(const XmlElement* state_elem, bool *state_out) {
    *state_out = state_elem != NULL;
  }
};

class PresenterStateClient : public PubSubStateClient<bool> {
 public:
  PresenterStateClient(const std::string& publisher_nick,
                       PubSubClient* client,
                       const QName& state_name,
                       bool default_state)
      : PubSubStateClient<bool>(
          publisher_nick, client, state_name, default_state,
          new PublishedNickKeySerializer(), NULL) {
  }

  virtual void Publish(const std::string& published_nick,
                       const bool& state,
                       std::string* task_id_out) {
    XmlElement* presenter_elem = new XmlElement(QN_PRESENTER_PRESENTER, true);
    presenter_elem->AddAttr(QN_NICK, published_nick);

    XmlElement* presentation_item_elem =
        new XmlElement(QN_PRESENTER_PRESENTATION_ITEM, false);
    const std::string& presentation_type = state ? kPresenting : kNotPresenting;
    presentation_item_elem->AddAttr(
        QN_PRESENTER_PRESENTATION_TYPE, presentation_type);

    // The Presenter state is kind of dumb in that it doesn't always use
    // retracts.  It relies on setting the "type" to a special value.
    std::string itemid = published_nick;
    std::vector<XmlElement*> children;
    children.push_back(presenter_elem);
    children.push_back(presentation_item_elem);
    client()->PublishItem(itemid, children, task_id_out);
  }

 protected:
  virtual bool ParseStateItem(const PubSubItem& item,
                              StateItemInfo* info_out,
                              bool* state_out) {
    const XmlElement* presenter_elem =
        item.elem->FirstNamed(QN_PRESENTER_PRESENTER);
    const XmlElement* presentation_item_elem =
        item.elem->FirstNamed(QN_PRESENTER_PRESENTATION_ITEM);
    if (presentation_item_elem == NULL || presenter_elem == NULL) {
      return false;
    }

    info_out->publisher_nick =
        client()->GetPublisherNickFromPubSubItem(item.elem);
    info_out->published_nick = presenter_elem->Attr(QN_NICK);
    *state_out = (presentation_item_elem->Attr(
        QN_PRESENTER_PRESENTATION_TYPE) != kNotPresenting);
    return true;
  }

  virtual bool StatesEqual(const bool& state1, const bool& state2) {
    // Make every item trigger an event, even if state doesn't change.
    return false;
  }
};

HangoutPubSubClient::HangoutPubSubClient(XmppTaskParentInterface* parent,
                                         const Jid& mucjid,
                                         const std::string& nick)
    : mucjid_(mucjid),
      nick_(nick) {
  presenter_client_.reset(new PubSubClient(parent, mucjid, NS_PRESENTER));
  presenter_client_->SignalRequestError.connect(
      this, &HangoutPubSubClient::OnPresenterRequestError);

  media_client_.reset(new PubSubClient(parent, mucjid, NS_GOOGLE_MUC_MEDIA));
  media_client_->SignalRequestError.connect(
      this, &HangoutPubSubClient::OnMediaRequestError);

  presenter_state_client_.reset(new PresenterStateClient(
      nick_, presenter_client_.get(), QN_PRESENTER_PRESENTER, false));
  presenter_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnPresenterStateChange);
  presenter_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnPresenterPublishResult);
  presenter_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnPresenterPublishError);

  audio_mute_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_AUDIO_MUTE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  // Can't just repeat because we need to watch for remote mutes.
  audio_mute_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnAudioMuteStateChange);
  audio_mute_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnAudioMutePublishResult);
  audio_mute_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnAudioMutePublishError);

  video_mute_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_VIDEO_MUTE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  // Can't just repeat because we need to watch for remote mutes.
  video_mute_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnVideoMuteStateChange);
  video_mute_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnVideoMutePublishResult);
  video_mute_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnVideoMutePublishError);

  video_pause_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_VIDEO_PAUSE, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  video_pause_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnVideoPauseStateChange);
  video_pause_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnVideoPausePublishResult);
  video_pause_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnVideoPausePublishError);

  recording_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_RECORDING, false,
      new PublishedNickKeySerializer(), new BoolStateSerializer()));
  recording_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnRecordingStateChange);
  recording_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnRecordingPublishResult);
  recording_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnRecordingPublishError);

  media_block_state_client_.reset(new PubSubStateClient<bool>(
      nick_, media_client_.get(), QN_GOOGLE_MUC_MEDIA_BLOCK, false,
      new PublisherAndPublishedNicksKeySerializer(),
      new BoolStateSerializer()));
  media_block_state_client_->SignalStateChange.connect(
      this, &HangoutPubSubClient::OnMediaBlockStateChange);
  media_block_state_client_->SignalPublishResult.connect(
      this, &HangoutPubSubClient::OnMediaBlockPublishResult);
  media_block_state_client_->SignalPublishError.connect(
      this, &HangoutPubSubClient::OnMediaBlockPublishError);
}

HangoutPubSubClient::~HangoutPubSubClient() {
}

void HangoutPubSubClient::RequestAll() {
  presenter_client_->RequestItems();
  media_client_->RequestItems();
}

void HangoutPubSubClient::OnPresenterRequestError(
    PubSubClient* client, const XmlElement* stanza) {
  SignalRequestError(client->node(), stanza);
}

void HangoutPubSubClient::OnMediaRequestError(
    PubSubClient* client, const XmlElement* stanza) {
  SignalRequestError(client->node(), stanza);
}

void HangoutPubSubClient::PublishPresenterState(
    bool presenting, std::string* task_id_out) {
  presenter_state_client_->Publish(nick_, presenting, task_id_out);
}

void HangoutPubSubClient::PublishAudioMuteState(
    bool muted, std::string* task_id_out) {
  audio_mute_state_client_->Publish(nick_, muted, task_id_out);
}

void HangoutPubSubClient::PublishVideoMuteState(
    bool muted, std::string* task_id_out) {
  video_mute_state_client_->Publish(nick_, muted, task_id_out);
}

void HangoutPubSubClient::PublishVideoPauseState(
    bool paused, std::string* task_id_out) {
  video_pause_state_client_->Publish(nick_, paused, task_id_out);
}

void HangoutPubSubClient::PublishRecordingState(
    bool recording, std::string* task_id_out) {
  recording_state_client_->Publish(nick_, recording, task_id_out);
}

// Remote mute is accomplished by setting another client's mute state.
void HangoutPubSubClient::RemoteMute(
    const std::string& mutee_nick, std::string* task_id_out) {
  audio_mute_state_client_->Publish(mutee_nick, true, task_id_out);
}

// Block media is accomplished by setting another client's block
// state, kind of like remote mute.
void HangoutPubSubClient::BlockMedia(
    const std::string& blockee_nick, std::string* task_id_out) {
  media_block_state_client_->Publish(blockee_nick, true, task_id_out);
}

void HangoutPubSubClient::OnPresenterStateChange(
    const PubSubStateChange<bool>& change) {
  SignalPresenterStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnPresenterPublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishPresenterResult(task_id);
}

void HangoutPubSubClient::OnPresenterPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishPresenterError(task_id, stanza);
}

// Since a remote mute is accomplished by another client setting our
// mute state, if our state changes to muted, we should mute ourselves.
// Note that remote un-muting is disallowed by the RoomServer.
void HangoutPubSubClient::OnAudioMuteStateChange(
    const PubSubStateChange<bool>& change) {
  bool was_muted = change.old_state;
  bool is_muted = change.new_state;
  bool remote_action = (!change.publisher_nick.empty() &&
                        (change.publisher_nick != change.published_nick));

  if (remote_action) {
    const std::string& mutee_nick = change.published_nick;
    const std::string& muter_nick = change.publisher_nick;
    if (!is_muted) {
      // The server should prevent remote un-mute.
      LOG(LS_WARNING) << muter_nick << " remote unmuted " << mutee_nick;
      return;
    }
    bool should_mute_locally = (mutee_nick == nick_);
    SignalRemoteMute(mutee_nick, muter_nick, should_mute_locally);
  }
  SignalAudioMuteStateChange(change.published_nick, was_muted, is_muted);
}

const std::string GetAudioMuteNickFromItem(const XmlElement* item) {
  if (item != NULL) {
    const XmlElement* audio_mute_state =
        item->FirstNamed(QN_GOOGLE_MUC_AUDIO_MUTE);
    if (audio_mute_state != NULL) {
      return audio_mute_state->Attr(QN_NICK);
    }
  }
  return std::string();
}

const std::string GetBlockeeNickFromItem(const XmlElement* item) {
  if (item != NULL) {
    const XmlElement* media_block_state =
        item->FirstNamed(QN_GOOGLE_MUC_MEDIA_BLOCK);
    if (media_block_state != NULL) {
      return media_block_state->Attr(QN_NICK);
    }
  }
  return std::string();
}

void HangoutPubSubClient::OnAudioMutePublishResult(
    const std::string& task_id, const XmlElement* item) {
  const std::string& mutee_nick = GetAudioMuteNickFromItem(item);
  if (mutee_nick != nick_) {
    SignalRemoteMuteResult(task_id, mutee_nick);
  } else {
    SignalPublishAudioMuteResult(task_id);
  }
}

void HangoutPubSubClient::OnAudioMutePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  const std::string& mutee_nick = GetAudioMuteNickFromItem(item);
  if (mutee_nick != nick_) {
    SignalRemoteMuteError(task_id, mutee_nick, stanza);
  } else {
    SignalPublishAudioMuteError(task_id, stanza);
  }
}

void HangoutPubSubClient::OnVideoMuteStateChange(
    const PubSubStateChange<bool>& change) {
  SignalVideoMuteStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnVideoMutePublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishVideoMuteResult(task_id);
}

void HangoutPubSubClient::OnVideoMutePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishVideoMuteError(task_id, stanza);
}

void HangoutPubSubClient::OnVideoPauseStateChange(
    const PubSubStateChange<bool>& change) {
  SignalVideoPauseStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnVideoPausePublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishVideoPauseResult(task_id);
}

void HangoutPubSubClient::OnVideoPausePublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishVideoPauseError(task_id, stanza);
}

void HangoutPubSubClient::OnRecordingStateChange(
    const PubSubStateChange<bool>& change) {
  SignalRecordingStateChange(
      change.published_nick, change.old_state, change.new_state);
}

void HangoutPubSubClient::OnRecordingPublishResult(
    const std::string& task_id, const XmlElement* item) {
  SignalPublishRecordingResult(task_id);
}

void HangoutPubSubClient::OnRecordingPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  SignalPublishRecordingError(task_id, stanza);
}

void HangoutPubSubClient::OnMediaBlockStateChange(
    const PubSubStateChange<bool>& change) {
  const std::string& blockee_nick = change.published_nick;
  const std::string& blocker_nick = change.publisher_nick;

  bool was_blockee = change.old_state;
  bool is_blockee = change.new_state;
  if (!was_blockee && is_blockee) {
    SignalMediaBlock(blockee_nick, blocker_nick);
  }
  // TODO: Should we bother signaling unblock? Currently
  // it isn't allowed, but it might happen when a participant leaves
  // the room and the item is retracted.
}

void HangoutPubSubClient::OnMediaBlockPublishResult(
    const std::string& task_id, const XmlElement* item) {
  const std::string& blockee_nick = GetBlockeeNickFromItem(item);
  SignalMediaBlockResult(task_id, blockee_nick);
}

void HangoutPubSubClient::OnMediaBlockPublishError(
    const std::string& task_id, const XmlElement* item,
    const XmlElement* stanza) {
  const std::string& blockee_nick = GetBlockeeNickFromItem(item);
  SignalMediaBlockError(task_id, blockee_nick, stanza);
}

}  // namespace buzz
