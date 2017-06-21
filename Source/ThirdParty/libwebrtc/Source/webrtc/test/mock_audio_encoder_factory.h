/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_AUDIO_ENCODER_FACTORY_H_
#define WEBRTC_TEST_MOCK_AUDIO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "webrtc/api/audio_codecs/audio_encoder_factory.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockAudioEncoderFactory : public AudioEncoderFactory {
 public:
  MOCK_METHOD0(GetSupportedEncoders, std::vector<AudioCodecSpec>());
  MOCK_METHOD1(QueryAudioEncoder,
               rtc::Optional<AudioCodecInfo>(const SdpAudioFormat& format));

  std::unique_ptr<AudioEncoder> MakeAudioEncoder(int payload_type,
                                                 const SdpAudioFormat& format) {
    std::unique_ptr<AudioEncoder> return_value;
    MakeAudioEncoderMock(payload_type, format, &return_value);
    return return_value;
  }
  MOCK_METHOD3(MakeAudioEncoderMock,
               void(int payload_type,
                    const SdpAudioFormat& format,
                    std::unique_ptr<AudioEncoder>* return_value));

  // Creates a MockAudioEncoderFactory with no formats and that may not be
  // invoked to create a codec - useful for initializing a voice engine, for
  // example.
  static rtc::scoped_refptr<webrtc::MockAudioEncoderFactory>
  CreateUnusedFactory() {
    using testing::_;
    using testing::AnyNumber;
    using testing::Return;

    rtc::scoped_refptr<webrtc::MockAudioEncoderFactory> factory =
        new rtc::RefCountedObject<webrtc::MockAudioEncoderFactory>;
    ON_CALL(*factory.get(), GetSupportedEncoders())
        .WillByDefault(Return(std::vector<webrtc::AudioCodecSpec>()));
    ON_CALL(*factory.get(), QueryAudioEncoder(_))
        .WillByDefault(Return(rtc::Optional<AudioCodecInfo>()));

    EXPECT_CALL(*factory.get(), GetSupportedEncoders()).Times(AnyNumber());
    EXPECT_CALL(*factory.get(), QueryAudioEncoder(_)).Times(AnyNumber());
    EXPECT_CALL(*factory.get(), MakeAudioEncoderMock(_, _, _)).Times(0);
    return factory;
  }

  // Creates a MockAudioEncoderFactory with no formats that may be invoked to
  // create a codec any number of times. It will, though, return nullptr on each
  // call, since it supports no codecs.
  static rtc::scoped_refptr<webrtc::MockAudioEncoderFactory>
  CreateEmptyFactory() {
    using testing::_;
    using testing::AnyNumber;
    using testing::Return;
    using testing::SetArgPointee;

    rtc::scoped_refptr<webrtc::MockAudioEncoderFactory> factory =
        new rtc::RefCountedObject<webrtc::MockAudioEncoderFactory>;
    ON_CALL(*factory.get(), GetSupportedEncoders())
        .WillByDefault(Return(std::vector<webrtc::AudioCodecSpec>()));
    ON_CALL(*factory.get(), QueryAudioEncoder(_))
        .WillByDefault(Return(rtc::Optional<AudioCodecInfo>()));
    ON_CALL(*factory.get(), MakeAudioEncoderMock(_, _, _))
        .WillByDefault(SetArgPointee<2>(nullptr));

    EXPECT_CALL(*factory.get(), GetSupportedEncoders()).Times(AnyNumber());
    EXPECT_CALL(*factory.get(), QueryAudioEncoder(_)).Times(AnyNumber());
    EXPECT_CALL(*factory.get(), MakeAudioEncoderMock(_, _, _))
        .Times(AnyNumber());
    return factory;
  }
};

}  // namespace webrtc

#endif  // WEBRTC_TEST_MOCK_AUDIO_ENCODER_FACTORY_H_
