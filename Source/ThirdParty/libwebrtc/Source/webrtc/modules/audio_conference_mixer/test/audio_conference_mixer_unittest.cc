/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/audio_conference_mixer/include/audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/include/audio_conference_mixer_defines.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;

class MockAudioMixerOutputReceiver : public AudioMixerOutputReceiver {
 public:
  MOCK_METHOD4(NewMixedAudio, void(const int32_t id,
                                   const AudioFrame& general_audio_frame,
                                   const AudioFrame** unique_audio_frames,
                                   const uint32_t size));
};

class MockMixerParticipant : public MixerParticipant {
 public:
  MockMixerParticipant() {
    ON_CALL(*this, GetAudioFrame(_, _))
        .WillByDefault(Invoke(this, &MockMixerParticipant::FakeAudioFrame));
  }
  MOCK_METHOD2(GetAudioFrame,
               int32_t(const int32_t id, AudioFrame* audio_frame));
  MOCK_CONST_METHOD1(NeededFrequency, int32_t(const int32_t id));
  AudioFrame* fake_frame() { return &fake_frame_; }

 private:
  AudioFrame fake_frame_;
  int32_t FakeAudioFrame(const int32_t id, AudioFrame* audio_frame) {
    audio_frame->CopyFrom(fake_frame_);
    return 0;
  }
};

TEST(AudioConferenceMixer, AnonymousAndNamed) {
  const int kId = 1;
  // Should not matter even if partipants are more than
  // kMaximumAmountOfMixedParticipants.
  const int kNamed =
      AudioConferenceMixer::kMaximumAmountOfMixedParticipants + 1;
  const int kAnonymous =
      AudioConferenceMixer::kMaximumAmountOfMixedParticipants + 1;

  std::unique_ptr<AudioConferenceMixer> mixer(
      AudioConferenceMixer::Create(kId));

  MockMixerParticipant named[kNamed];
  MockMixerParticipant anonymous[kAnonymous];

  for (int i = 0; i < kNamed; ++i) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&named[i], true));
    EXPECT_TRUE(mixer->MixabilityStatus(named[i]));
  }

  for (int i = 0; i < kAnonymous; ++i) {
    // Participant must be registered before turning it into anonymous.
    EXPECT_EQ(-1, mixer->SetAnonymousMixabilityStatus(&anonymous[i], true));
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&anonymous[i], true));
    EXPECT_TRUE(mixer->MixabilityStatus(anonymous[i]));
    EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    EXPECT_EQ(0, mixer->SetAnonymousMixabilityStatus(&anonymous[i], true));
    EXPECT_TRUE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    // Anonymous participants do not show status by MixabilityStatus.
    EXPECT_FALSE(mixer->MixabilityStatus(anonymous[i]));
  }

  for (int i = 0; i < kNamed; ++i) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&named[i], false));
    EXPECT_FALSE(mixer->MixabilityStatus(named[i]));
  }

  for (int i = 0; i < kAnonymous - 1; i++) {
    EXPECT_EQ(0, mixer->SetAnonymousMixabilityStatus(&anonymous[i], false));
    EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    // SetAnonymousMixabilityStatus(anonymous, false) moves anonymous to the
    // named group.
    EXPECT_TRUE(mixer->MixabilityStatus(anonymous[i]));
  }

  // SetMixabilityStatus(anonymous, false) will remove anonymous from both
  // anonymous and named groups.
  EXPECT_EQ(0, mixer->SetMixabilityStatus(&anonymous[kAnonymous - 1], false));
  EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[kAnonymous - 1]));
  EXPECT_FALSE(mixer->MixabilityStatus(anonymous[kAnonymous - 1]));
}

TEST(AudioConferenceMixer, LargestEnergyVadActiveMixed) {
  const int kId = 1;
  const int kParticipants =
      AudioConferenceMixer::kMaximumAmountOfMixedParticipants + 3;
  const int kSampleRateHz = 32000;

  std::unique_ptr<AudioConferenceMixer> mixer(
      AudioConferenceMixer::Create(kId));

  MockAudioMixerOutputReceiver output_receiver;
  EXPECT_EQ(0, mixer->RegisterMixedStreamCallback(&output_receiver));

  MockMixerParticipant participants[kParticipants];

  for (int i = 0; i < kParticipants; ++i) {
    participants[i].fake_frame()->id_ = i;
    participants[i].fake_frame()->sample_rate_hz_ = kSampleRateHz;
    participants[i].fake_frame()->speech_type_ =  AudioFrame::kNormalSpeech;
    participants[i].fake_frame()->vad_activity_ = AudioFrame::kVadActive;
    participants[i].fake_frame()->num_channels_ = 1;

    // Frame duration 10ms.
    participants[i].fake_frame()->samples_per_channel_ = kSampleRateHz / 100;

    // We set the 80-th sample value since the first 80 samples may be
    // modified by a ramped-in window.
    participants[i].fake_frame()->data_[80] = i;

    EXPECT_EQ(0, mixer->SetMixabilityStatus(&participants[i], true));
    EXPECT_CALL(participants[i], GetAudioFrame(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(participants[i], NeededFrequency(_))
        .WillRepeatedly(Return(kSampleRateHz));
  }

  // Last participant gives audio frame with passive VAD, although it has the
  // largest energy.
  participants[kParticipants - 1].fake_frame()->vad_activity_ =
      AudioFrame::kVadPassive;

  EXPECT_CALL(output_receiver, NewMixedAudio(_, _, _, _))
      .Times(AtLeast(1));

  mixer->Process();

  for (int i = 0; i < kParticipants; ++i) {
    bool is_mixed = participants[i].IsMixed();
    if (i == kParticipants - 1 || i < kParticipants - 1 -
        AudioConferenceMixer::kMaximumAmountOfMixedParticipants) {
      EXPECT_FALSE(is_mixed) << "Mixing status of Participant #"
                             << i << " wrong.";
    } else {
      EXPECT_TRUE(is_mixed) << "Mixing status of Participant #"
                            << i << " wrong.";
    }
  }

  EXPECT_EQ(0, mixer->UnRegisterMixedStreamCallback());
}

}  // namespace webrtc
