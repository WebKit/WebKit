/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/fakexmppclient.h"
#include "webrtc/libjingle/xmpp/hangoutpubsubclient.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/base/faketaskrunner.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"

class TestHangoutPubSubListener : public sigslot::has_slots<> {
 public:
  TestHangoutPubSubListener() :
      request_error_count(0),
      publish_audio_mute_error_count(0),
      publish_video_mute_error_count(0),
      publish_video_pause_error_count(0),
      publish_presenter_error_count(0),
      publish_recording_error_count(0),
      remote_mute_error_count(0) {
  }

  void OnPresenterStateChange(
      const std::string& nick, bool was_presenting, bool is_presenting) {
    last_presenter_nick = nick;
    last_was_presenting = was_presenting;
    last_is_presenting = is_presenting;
  }

  void OnAudioMuteStateChange(
      const std::string& nick, bool was_muted, bool is_muted) {
    last_audio_muted_nick = nick;
    last_was_audio_muted = was_muted;
    last_is_audio_muted = is_muted;
  }

  void OnVideoMuteStateChange(
      const std::string& nick, bool was_muted, bool is_muted) {
    last_video_muted_nick = nick;
    last_was_video_muted = was_muted;
    last_is_video_muted = is_muted;
  }

  void OnVideoPauseStateChange(
      const std::string& nick, bool was_paused, bool is_paused) {
    last_video_paused_nick = nick;
    last_was_video_paused = was_paused;
    last_is_video_paused = is_paused;
  }

  void OnRecordingStateChange(
      const std::string& nick, bool was_recording, bool is_recording) {
    last_recording_nick = nick;
    last_was_recording = was_recording;
    last_is_recording = is_recording;
  }

  void OnRemoteMute(
      const std::string& mutee_nick,
      const std::string& muter_nick,
      bool should_mute_locally) {
    last_mutee_nick = mutee_nick;
    last_muter_nick = muter_nick;
    last_should_mute = should_mute_locally;
  }

  void OnMediaBlock(
      const std::string& blockee_nick,
      const std::string& blocker_nick) {
    last_blockee_nick = blockee_nick;
    last_blocker_nick = blocker_nick;
  }

  void OnRequestError(const std::string& node, const buzz::XmlElement* stanza) {
    ++request_error_count;
    request_error_node = node;
  }

  void OnPublishAudioMuteError(const std::string& task_id,
                               const buzz::XmlElement* stanza) {
    ++publish_audio_mute_error_count;
    error_task_id = task_id;
  }

  void OnPublishVideoMuteError(const std::string& task_id,
                               const buzz::XmlElement* stanza) {
    ++publish_video_mute_error_count;
    error_task_id = task_id;
  }

  void OnPublishVideoPauseError(const std::string& task_id,
                               const buzz::XmlElement* stanza) {
    ++publish_video_pause_error_count;
    error_task_id = task_id;
  }

  void OnPublishPresenterError(const std::string& task_id,
                               const buzz::XmlElement* stanza) {
    ++publish_presenter_error_count;
    error_task_id = task_id;
  }

  void OnPublishRecordingError(const std::string& task_id,
                               const buzz::XmlElement* stanza) {
    ++publish_recording_error_count;
    error_task_id = task_id;
  }

  void OnRemoteMuteResult(const std::string& task_id,
                          const std::string& mutee_nick) {
    result_task_id = task_id;
    remote_mute_mutee_nick = mutee_nick;
  }

  void OnRemoteMuteError(const std::string& task_id,
                         const std::string& mutee_nick,
                         const buzz::XmlElement* stanza) {
    ++remote_mute_error_count;
    error_task_id = task_id;
    remote_mute_mutee_nick = mutee_nick;
  }

  void OnMediaBlockResult(const std::string& task_id,
                          const std::string& blockee_nick) {
    result_task_id = task_id;
    media_blockee_nick = blockee_nick;
  }

  void OnMediaBlockError(const std::string& task_id,
                         const std::string& blockee_nick,
                         const buzz::XmlElement* stanza) {
    ++media_block_error_count;
    error_task_id = task_id;
    media_blockee_nick = blockee_nick;
  }

  std::string last_presenter_nick;
  bool last_is_presenting;
  bool last_was_presenting;
  std::string last_audio_muted_nick;
  bool last_is_audio_muted;
  bool last_was_audio_muted;
  std::string last_video_muted_nick;
  bool last_is_video_muted;
  bool last_was_video_muted;
  std::string last_video_paused_nick;
  bool last_is_video_paused;
  bool last_was_video_paused;
  std::string last_recording_nick;
  bool last_is_recording;
  bool last_was_recording;
  std::string last_mutee_nick;
  std::string last_muter_nick;
  bool last_should_mute;
  std::string last_blockee_nick;
  std::string last_blocker_nick;

  int request_error_count;
  std::string request_error_node;
  int publish_audio_mute_error_count;
  int publish_video_mute_error_count;
  int publish_video_pause_error_count;
  int publish_presenter_error_count;
  int publish_recording_error_count;
  int remote_mute_error_count;
  std::string result_task_id;
  std::string error_task_id;
  std::string remote_mute_mutee_nick;
  int media_block_error_count;
  std::string media_blockee_nick;
};

class HangoutPubSubClientTest : public testing::Test {
 public:
  HangoutPubSubClientTest() :
      pubsubjid("room@domain.com"),
      nick("me") {

    runner.reset(new rtc::FakeTaskRunner());
    xmpp_client = new buzz::FakeXmppClient(runner.get());
    client.reset(new buzz::HangoutPubSubClient(xmpp_client, pubsubjid, nick));
    listener.reset(new TestHangoutPubSubListener());
    client->SignalPresenterStateChange.connect(
        listener.get(), &TestHangoutPubSubListener::OnPresenterStateChange);
    client->SignalAudioMuteStateChange.connect(
        listener.get(), &TestHangoutPubSubListener::OnAudioMuteStateChange);
    client->SignalVideoMuteStateChange.connect(
        listener.get(), &TestHangoutPubSubListener::OnVideoMuteStateChange);
    client->SignalVideoPauseStateChange.connect(
        listener.get(), &TestHangoutPubSubListener::OnVideoPauseStateChange);
    client->SignalRecordingStateChange.connect(
        listener.get(), &TestHangoutPubSubListener::OnRecordingStateChange);
    client->SignalRemoteMute.connect(
        listener.get(), &TestHangoutPubSubListener::OnRemoteMute);
    client->SignalMediaBlock.connect(
        listener.get(), &TestHangoutPubSubListener::OnMediaBlock);
    client->SignalRequestError.connect(
        listener.get(), &TestHangoutPubSubListener::OnRequestError);
    client->SignalPublishAudioMuteError.connect(
        listener.get(), &TestHangoutPubSubListener::OnPublishAudioMuteError);
    client->SignalPublishVideoMuteError.connect(
        listener.get(), &TestHangoutPubSubListener::OnPublishVideoMuteError);
    client->SignalPublishVideoPauseError.connect(
        listener.get(), &TestHangoutPubSubListener::OnPublishVideoPauseError);
    client->SignalPublishPresenterError.connect(
        listener.get(), &TestHangoutPubSubListener::OnPublishPresenterError);
    client->SignalPublishRecordingError.connect(
        listener.get(), &TestHangoutPubSubListener::OnPublishRecordingError);
    client->SignalRemoteMuteResult.connect(
        listener.get(), &TestHangoutPubSubListener::OnRemoteMuteResult);
    client->SignalRemoteMuteError.connect(
        listener.get(), &TestHangoutPubSubListener::OnRemoteMuteError);
    client->SignalMediaBlockResult.connect(
        listener.get(), &TestHangoutPubSubListener::OnMediaBlockResult);
    client->SignalMediaBlockError.connect(
        listener.get(), &TestHangoutPubSubListener::OnMediaBlockError);
  }

  std::unique_ptr<rtc::FakeTaskRunner> runner;
  // xmpp_client deleted by deleting runner.
  buzz::FakeXmppClient* xmpp_client;
  std::unique_ptr<buzz::HangoutPubSubClient> client;
  std::unique_ptr<TestHangoutPubSubListener> listener;
  buzz::Jid pubsubjid;
  std::string nick;
};

TEST_F(HangoutPubSubClientTest, TestRequest) {
  ASSERT_EQ(0U, xmpp_client->sent_stanzas().size());

  client->RequestAll();
  std::string expected_presenter_request =
      "<cli:iq type=\"get\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pub:pubsub xmlns:pub=\"http://jabber.org/protocol/pubsub\">"
          "<pub:items node=\"google:presenter\"/>"
        "</pub:pubsub>"
      "</cli:iq>";

  std::string expected_media_request =
      "<cli:iq type=\"get\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pub:pubsub xmlns:pub=\"http://jabber.org/protocol/pubsub\">"
          "<pub:items node=\"google:muc#media\"/>"
        "</pub:pubsub>"
      "</cli:iq>";

  ASSERT_EQ(2U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_presenter_request, xmpp_client->sent_stanzas()[0]->Str());
  EXPECT_EQ(expected_media_request, xmpp_client->sent_stanzas()[1]->Str());

  std::string presenter_response =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'>"
      "  <pubsub xmlns='http://jabber.org/protocol/pubsub'>"
      "    <items node='google:presenter'>"
      "      <item id='12344'>"
      "        <presenter xmlns='google:presenter' nick='presenting-nick2'/>"
      "        <pre:presentation-item xmlns:pre='google:presenter'"
      "          pre:presentation-type='s'/>"
      "      </item>"
      "      <item id='12345'>"
      "        <presenter xmlns='google:presenter' nick='presenting-nick'/>"
      "        <pre:presentation-item xmlns:pre='google:presenter'"
      "          pre:presentation-type='o'/>"
      "      </item>"
      // Some clients are "bad" in that they'll jam multiple states in
      // all at once.  We have to deal with it.
      "      <item id='12346'>"
      "        <presenter xmlns='google:presenter' nick='presenting-nick'/>"
      "        <pre:presentation-item xmlns:pre='google:presenter'"
      "          pre:presentation-type='s'/>"
      "      </item>"
      "    </items>"
      "  </pubsub>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(presenter_response));
  EXPECT_EQ("presenting-nick", listener->last_presenter_nick);
  EXPECT_FALSE(listener->last_was_presenting);
  EXPECT_TRUE(listener->last_is_presenting);

  std::string media_response =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'>"
      "  <pubsub xmlns='http://jabber.org/protocol/pubsub'>"
      "    <items node='google:muc#media'>"
      "      <item id='audio-mute:muted-nick'>"
      "        <audio-mute nick='muted-nick' xmlns='google:muc#media'/>"
      "      </item>"
      "      <item id='video-mute:video-muted-nick'>"
      "        <video-mute nick='video-muted-nick' xmlns='google:muc#media'/>"
      "      </item>"
      "      <item id='video-pause:video-paused-nick'>"
      "        <video-pause nick='video-paused-nick' xmlns='google:muc#media'/>"
      "      </item>"
      "      <item id='recording:recording-nick'>"
      "        <recording nick='recording-nick' xmlns='google:muc#media'/>"
      "      </item>"
      "    </items>"
      "  </pubsub>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(media_response));
  EXPECT_EQ("muted-nick", listener->last_audio_muted_nick);
  EXPECT_FALSE(listener->last_was_audio_muted);
  EXPECT_TRUE(listener->last_is_audio_muted);

  EXPECT_EQ("video-muted-nick", listener->last_video_muted_nick);
  EXPECT_FALSE(listener->last_was_video_muted);
  EXPECT_TRUE(listener->last_is_video_muted);

  EXPECT_EQ("video-paused-nick", listener->last_video_paused_nick);
  EXPECT_FALSE(listener->last_was_video_paused);
  EXPECT_TRUE(listener->last_is_video_paused);

  EXPECT_EQ("recording-nick", listener->last_recording_nick);
  EXPECT_FALSE(listener->last_was_recording);
  EXPECT_TRUE(listener->last_is_recording);

  std::string incoming_presenter_resets_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:presenter'>"
      "      <item id='12348'>"
      "        <presenter xmlns='google:presenter' nick='presenting-nick'/>"
      "        <pre:presentation-item xmlns:pre='google:presenter'"
      "          pre:presentation-type='o'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_presenter_resets_message));
  EXPECT_EQ("presenting-nick", listener->last_presenter_nick);
  //EXPECT_TRUE(listener->last_was_presenting);
  EXPECT_FALSE(listener->last_is_presenting);

  std::string incoming_presenter_retracts_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:presenter'>"
      "      <retract id='12344'/>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_presenter_retracts_message));
  EXPECT_EQ("presenting-nick2", listener->last_presenter_nick);
  EXPECT_TRUE(listener->last_was_presenting);
  EXPECT_FALSE(listener->last_is_presenting);

  std::string incoming_media_retracts_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:muc#media'>"
      "      <item id='audio-mute:muted-nick'>"
      "      </item>"
      "      <retract id='video-mute:video-muted-nick'/>"
      "      <retract id='video-pause:video-paused-nick'/>"
      "      <retract id='recording:recording-nick'/>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_media_retracts_message));
  EXPECT_EQ("muted-nick", listener->last_audio_muted_nick);
  EXPECT_TRUE(listener->last_was_audio_muted);
  EXPECT_FALSE(listener->last_is_audio_muted);

  EXPECT_EQ("video-paused-nick", listener->last_video_paused_nick);
  EXPECT_TRUE(listener->last_was_video_paused);
  EXPECT_FALSE(listener->last_is_video_paused);

  EXPECT_EQ("recording-nick", listener->last_recording_nick);
  EXPECT_TRUE(listener->last_was_recording);
  EXPECT_FALSE(listener->last_is_recording);

  std::string incoming_presenter_changes_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:presenter'>"
      "      <item id='presenting-nick2'>"
      "        <presenter xmlns='google:presenter' nick='presenting-nick2'/>"
      "        <pre:presentation-item xmlns:pre='google:presenter'"
      "          pre:presentation-type='s'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_presenter_changes_message));
  EXPECT_EQ("presenting-nick2", listener->last_presenter_nick);
  EXPECT_FALSE(listener->last_was_presenting);
  EXPECT_TRUE(listener->last_is_presenting);

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_presenter_changes_message));
  EXPECT_EQ("presenting-nick2", listener->last_presenter_nick);
  EXPECT_TRUE(listener->last_was_presenting);
  EXPECT_TRUE(listener->last_is_presenting);

  std::string incoming_media_changes_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:muc#media'>"
      "      <item id='audio-mute:muted-nick2'>"
      "        <audio-mute nick='muted-nick2' xmlns='google:muc#media'/>"
      "      </item>"
      "      <item id='video-pause:video-paused-nick2'>"
      "        <video-pause nick='video-paused-nick2' xmlns='google:muc#media'/>"
      "      </item>"
      "      <item id='recording:recording-nick2'>"
      "        <recording nick='recording-nick2' xmlns='google:muc#media'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_media_changes_message));
  EXPECT_EQ("muted-nick2", listener->last_audio_muted_nick);
  EXPECT_FALSE(listener->last_was_audio_muted);
  EXPECT_TRUE(listener->last_is_audio_muted);

  EXPECT_EQ("video-paused-nick2", listener->last_video_paused_nick);
  EXPECT_FALSE(listener->last_was_video_paused);
  EXPECT_TRUE(listener->last_is_video_paused);

  EXPECT_EQ("recording-nick2", listener->last_recording_nick);
  EXPECT_FALSE(listener->last_was_recording);
  EXPECT_TRUE(listener->last_is_recording);

  std::string incoming_remote_mute_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:muc#media'>"
      "      <item id='audio-mute:mutee' publisher='room@domain.com/muter'>"
      "        <audio-mute nick='mutee' xmlns='google:muc#media'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  listener->last_is_audio_muted = false;
  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_remote_mute_message));
  EXPECT_EQ("mutee", listener->last_mutee_nick);
  EXPECT_EQ("muter", listener->last_muter_nick);
  EXPECT_FALSE(listener->last_should_mute);
  EXPECT_EQ("mutee", listener->last_audio_muted_nick);
  EXPECT_TRUE(listener->last_is_audio_muted);

  std::string incoming_remote_mute_me_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:muc#media'>"
      "      <item id='audio-mute:me' publisher='room@domain.com/muter'>"
      "        <audio-mute nick='me' xmlns='google:muc#media'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  listener->last_is_audio_muted = false;
  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_remote_mute_me_message));
  EXPECT_EQ("me", listener->last_mutee_nick);
  EXPECT_EQ("muter", listener->last_muter_nick);
  EXPECT_TRUE(listener->last_should_mute);
  EXPECT_EQ("me", listener->last_audio_muted_nick);
  EXPECT_TRUE(listener->last_is_audio_muted);

  std::string incoming_media_block_message =
      "<message xmlns='jabber:client' from='room@domain.com'>"
      "  <event xmlns='http://jabber.org/protocol/pubsub#event'>"
      "    <items node='google:muc#media'>"
      "      <item id='block:blocker:blockee'"
      "            publisher='room@domain.com/blocker'>"
      "        <block nick='blockee' xmlns='google:muc#media'/>"
      "      </item>"
      "    </items>"
      "  </event>"
      "</message>";

  xmpp_client->HandleStanza(
      buzz::XmlElement::ForStr(incoming_media_block_message));
  EXPECT_EQ("blockee", listener->last_blockee_nick);
  EXPECT_EQ("blocker", listener->last_blocker_nick);
}

TEST_F(HangoutPubSubClientTest, TestRequestError) {
  client->RequestAll();
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'>"
      "  <error type='auth'>"
      "    <forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
      "  </error>"
      "</iq>";

  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->request_error_count);
  EXPECT_EQ("google:presenter", listener->request_error_node);
}

TEST_F(HangoutPubSubClientTest, TestPublish) {
  client->PublishPresenterState(true);
  std::string expected_presenter_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:presenter\">"
            "<item id=\"me\">"
              "<presenter xmlns=\"google:presenter\""
              " nick=\"me\"/>"
              "<pre:presentation-item"
              " pre:presentation-type=\"s\" xmlns:pre=\"google:presenter\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(1U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_presenter_iq,
            xmpp_client->sent_stanzas()[0]->Str());

  client->PublishAudioMuteState(true);
  std::string expected_audio_mute_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:muc#media\">"
            "<item id=\"audio-mute:me\">"
              "<audio-mute xmlns=\"google:muc#media\" nick=\"me\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(2U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_audio_mute_iq, xmpp_client->sent_stanzas()[1]->Str());

  client->PublishVideoPauseState(true);
  std::string expected_video_pause_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:muc#media\">"
            "<item id=\"video-pause:me\">"
              "<video-pause xmlns=\"google:muc#media\" nick=\"me\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(3U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_video_pause_iq, xmpp_client->sent_stanzas()[2]->Str());

  client->PublishRecordingState(true);
  std::string expected_recording_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:muc#media\">"
            "<item id=\"recording:me\">"
              "<recording xmlns=\"google:muc#media\" nick=\"me\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(4U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_recording_iq, xmpp_client->sent_stanzas()[3]->Str());

  client->RemoteMute("mutee");
  std::string expected_remote_mute_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:muc#media\">"
            "<item id=\"audio-mute:mutee\">"
              "<audio-mute xmlns=\"google:muc#media\" nick=\"mutee\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(5U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_remote_mute_iq, xmpp_client->sent_stanzas()[4]->Str());

  client->PublishPresenterState(false);
  std::string expected_presenter_retract_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:presenter\">"
            "<item id=\"me\">"
              "<presenter xmlns=\"google:presenter\""
              " nick=\"me\"/>"
              "<pre:presentation-item"
              " pre:presentation-type=\"o\" xmlns:pre=\"google:presenter\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(6U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_presenter_retract_iq,
            xmpp_client->sent_stanzas()[5]->Str());

  client->PublishAudioMuteState(false);
  std::string expected_audio_mute_retract_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<retract node=\"google:muc#media\" notify=\"true\">"
            "<item id=\"audio-mute:me\"/>"
          "</retract>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(7U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_audio_mute_retract_iq,
            xmpp_client->sent_stanzas()[6]->Str());

  client->PublishVideoPauseState(false);
  std::string expected_video_pause_retract_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<retract node=\"google:muc#media\" notify=\"true\">"
            "<item id=\"video-pause:me\"/>"
          "</retract>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(8U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_video_pause_retract_iq,
            xmpp_client->sent_stanzas()[7]->Str());

  client->BlockMedia("blockee");
  std::string expected_media_block_iq =
      "<cli:iq type=\"set\" to=\"room@domain.com\" id=\"0\" "
        "xmlns:cli=\"jabber:client\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<publish node=\"google:muc#media\">"
            "<item id=\"block:me:blockee\">"
              "<block xmlns=\"google:muc#media\" nick=\"blockee\"/>"
            "</item>"
          "</publish>"
        "</pubsub>"
      "</cli:iq>";

  ASSERT_EQ(9U, xmpp_client->sent_stanzas().size());
  EXPECT_EQ(expected_media_block_iq, xmpp_client->sent_stanzas()[8]->Str());
}

TEST_F(HangoutPubSubClientTest, TestPublishPresenterError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->PublishPresenterState(true);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->publish_presenter_error_count);
  EXPECT_EQ("0", listener->error_task_id);
}


TEST_F(HangoutPubSubClientTest, TestPublishAudioMuteError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->PublishAudioMuteState(true);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->publish_audio_mute_error_count);
  EXPECT_EQ("0", listener->error_task_id);
}

TEST_F(HangoutPubSubClientTest, TestPublishVideoPauseError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->PublishVideoPauseState(true);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->publish_video_pause_error_count);
  EXPECT_EQ("0", listener->error_task_id);
}

TEST_F(HangoutPubSubClientTest, TestPublishRecordingError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->PublishRecordingState(true);
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->publish_recording_error_count);
  EXPECT_EQ("0", listener->error_task_id);
}

TEST_F(HangoutPubSubClientTest, TestPublishRemoteMuteResult) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  client->RemoteMute("joe");
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ("joe", listener->remote_mute_mutee_nick);
  EXPECT_EQ("0", listener->result_task_id);
}

TEST_F(HangoutPubSubClientTest, TestRemoteMuteError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->RemoteMute("joe");
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->remote_mute_error_count);
  EXPECT_EQ("joe", listener->remote_mute_mutee_nick);
  EXPECT_EQ("0", listener->error_task_id);
}

TEST_F(HangoutPubSubClientTest, TestPublishMediaBlockResult) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='result' from='room@domain.com'/>";

  client->BlockMedia("joe");
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ("joe", listener->media_blockee_nick);
  EXPECT_EQ("0", listener->result_task_id);
}

TEST_F(HangoutPubSubClientTest, TestMediaBlockError) {
  std::string result_iq =
      "<iq xmlns='jabber:client' id='0' type='error' from='room@domain.com'/>";

  client->BlockMedia("joe");
  xmpp_client->HandleStanza(buzz::XmlElement::ForStr(result_iq));
  EXPECT_EQ(1, listener->remote_mute_error_count);
  EXPECT_EQ("joe", listener->media_blockee_nick);
  EXPECT_EQ("0", listener->error_task_id);
}
