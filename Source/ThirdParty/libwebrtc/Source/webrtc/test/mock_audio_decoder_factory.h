/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_MOCK_AUDIO_DECODER_FACTORY_H_
#define TEST_MOCK_AUDIO_DECODER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "test/gmock.h"

namespace webrtc {

class MockAudioDecoderFactory : public AudioDecoderFactory {
 public:
  MOCK_METHOD0(GetSupportedDecoders, std::vector<AudioCodecSpec>());
  MOCK_METHOD1(IsSupportedDecoder, bool(const SdpAudioFormat&));
  std::unique_ptr<AudioDecoder> MakeAudioDecoder(const SdpAudioFormat& format) {
    std::unique_ptr<AudioDecoder> return_value;
    MakeAudioDecoderMock(format, &return_value);
    return return_value;
  }
  MOCK_METHOD2(MakeAudioDecoderMock,
               void(const SdpAudioFormat& format,
                    std::unique_ptr<AudioDecoder>* return_value));

  // Creates a MockAudioDecoderFactory with no formats and that may not be
  // invoked to create a codec - useful for initializing a voice engine, for
  // example.
  static rtc::scoped_refptr<webrtc::MockAudioDecoderFactory>
  CreateUnusedFactory() {
    using testing::_;
    using testing::AnyNumber;
    using testing::Return;

    rtc::scoped_refptr<webrtc::MockAudioDecoderFactory> factory =
        new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>;
    ON_CALL(*factory.get(), GetSupportedDecoders())
        .WillByDefault(Return(std::vector<webrtc::AudioCodecSpec>()));
    EXPECT_CALL(*factory.get(), GetSupportedDecoders()).Times(AnyNumber());
    ON_CALL(*factory, IsSupportedDecoder(_)).WillByDefault(Return(false));
    EXPECT_CALL(*factory, IsSupportedDecoder(_)).Times(AnyNumber());
    EXPECT_CALL(*factory.get(), MakeAudioDecoderMock(_, _)).Times(0);
    return factory;
  }

  // Creates a MockAudioDecoderFactory with no formats that may be invoked to
  // create a codec any number of times. It will, though, return nullptr on each
  // call, since it supports no codecs.
  static rtc::scoped_refptr<webrtc::MockAudioDecoderFactory>
  CreateEmptyFactory() {
    using testing::_;
    using testing::AnyNumber;
    using testing::Return;
    using testing::SetArgPointee;

    rtc::scoped_refptr<webrtc::MockAudioDecoderFactory> factory =
        new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>;
    ON_CALL(*factory.get(), GetSupportedDecoders())
        .WillByDefault(Return(std::vector<webrtc::AudioCodecSpec>()));
    EXPECT_CALL(*factory.get(), GetSupportedDecoders()).Times(AnyNumber());
    ON_CALL(*factory, IsSupportedDecoder(_)).WillByDefault(Return(false));
    EXPECT_CALL(*factory, IsSupportedDecoder(_)).Times(AnyNumber());
    ON_CALL(*factory.get(), MakeAudioDecoderMock(_, _))
        .WillByDefault(SetArgPointee<1>(nullptr));
    EXPECT_CALL(*factory.get(), MakeAudioDecoderMock(_, _)).Times(AnyNumber());
    return factory;
  }
};

}  // namespace webrtc

#endif  // TEST_MOCK_AUDIO_DECODER_FACTORY_H_
