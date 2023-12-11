/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/single_process_encoded_image_data_injector.h"

#include <utility>

#include "api/video/encoded_image.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

rtc::scoped_refptr<EncodedImageBuffer>
CreateEncodedImageBufferOfSizeNFilledWithValuesFromX(size_t n, uint8_t x) {
  auto buffer = EncodedImageBuffer::Create(n);
  for (size_t i = 0; i < n; ++i) {
    buffer->data()[i] = static_cast<uint8_t>(x + i);
  }
  return buffer;
}

EncodedImage CreateEncodedImageOfSizeNFilledWithValuesFromX(size_t n,
                                                            uint8_t x) {
  EncodedImage image;
  image.SetEncodedData(
      CreateEncodedImageBufferOfSizeNFilledWithValuesFromX(n, x));
  return image;
}

EncodedImage DeepCopyEncodedImage(const EncodedImage& source) {
  EncodedImage copy = source;
  copy.SetEncodedData(EncodedImageBuffer::Create(source.data(), source.size()));
  return copy;
}

TEST(SingleProcessEncodedImageDataInjectorTest, InjectExtractDiscardFalse) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(512, false, source));
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, InjectExtractDiscardTrue) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(512, true, source));
  EXPECT_EQ(out.id, 512);
  EXPECT_TRUE(out.discard);
  EXPECT_EQ(out.image.size(), 0ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
}

TEST(SingleProcessEncodedImageDataInjectorTest,
     InjectWithUnsetSpatialLayerSizes) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImage intermediate = injector.InjectData(512, false, source);
  intermediate.SetSpatialIndex(2);

  EncodedImageExtractionResult out = injector.ExtractData(intermediate);
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
  EXPECT_EQ(out.image.SpatialIndex().value_or(0), 2);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(out.image.SpatialLayerFrameSize(i).value_or(0), 0ul);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest,
     InjectWithZeroSpatialLayerSizes) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImage intermediate = injector.InjectData(512, false, source);
  intermediate.SetSpatialIndex(2);
  intermediate.SetSpatialLayerFrameSize(0, 0);
  intermediate.SetSpatialLayerFrameSize(1, 0);
  intermediate.SetSpatialLayerFrameSize(2, 0);

  EncodedImageExtractionResult out = injector.ExtractData(intermediate);
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
  EXPECT_EQ(out.image.SpatialIndex().value_or(0), 2);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(out.image.SpatialLayerFrameSize(i).value_or(0), 0ul);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, Inject3Extract3) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  // 1st frame
  EncodedImage source1 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source1.SetRtpTimestamp(123456710);
  // 2nd frame 1st spatial layer
  EncodedImage source2 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/11);
  source2.SetRtpTimestamp(123456720);
  // 2nd frame 2nd spatial layer
  EncodedImage source3 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/21);
  source3.SetRtpTimestamp(123456720);

  EncodedImage intermediate1 = injector.InjectData(510, false, source1);
  EncodedImage intermediate2 = injector.InjectData(520, true, source2);
  EncodedImage intermediate3 = injector.InjectData(520, false, source3);

  // Extract ids in different order.
  EncodedImageExtractionResult out3 = injector.ExtractData(intermediate3);
  EncodedImageExtractionResult out1 = injector.ExtractData(intermediate1);
  EncodedImageExtractionResult out2 = injector.ExtractData(intermediate2);

  EXPECT_EQ(out1.id, 510);
  EXPECT_FALSE(out1.discard);
  EXPECT_EQ(out1.image.size(), 10ul);
  EXPECT_EQ(out1.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out1.image.data()[i], i + 1);
  }
  EXPECT_EQ(out2.id, 520);
  EXPECT_TRUE(out2.discard);
  EXPECT_EQ(out2.image.size(), 0ul);
  EXPECT_EQ(out2.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  EXPECT_EQ(out3.id, 520);
  EXPECT_FALSE(out3.discard);
  EXPECT_EQ(out3.image.size(), 10ul);
  EXPECT_EQ(out3.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out3.image.data()[i], i + 21);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, InjectExtractFromConcatenated) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source1 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source1.SetRtpTimestamp(123456710);
  EncodedImage source2 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/11);
  source2.SetRtpTimestamp(123456710);
  EncodedImage source3 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/21);
  source3.SetRtpTimestamp(123456710);

  // Inject id into 3 images with same frame id.
  EncodedImage intermediate1 = injector.InjectData(512, false, source1);
  EncodedImage intermediate2 = injector.InjectData(512, true, source2);
  EncodedImage intermediate3 = injector.InjectData(512, false, source3);

  // Concatenate them into single encoded image, like it can be done in jitter
  // buffer.
  size_t concatenated_length =
      intermediate1.size() + intermediate2.size() + intermediate3.size();
  rtc::Buffer concatenated_buffer;
  concatenated_buffer.AppendData(intermediate1.data(), intermediate1.size());
  concatenated_buffer.AppendData(intermediate2.data(), intermediate2.size());
  concatenated_buffer.AppendData(intermediate3.data(), intermediate3.size());
  EncodedImage concatenated;
  concatenated.SetEncodedData(EncodedImageBuffer::Create(
      concatenated_buffer.data(), concatenated_length));
  concatenated.SetSpatialIndex(2);
  concatenated.SetSpatialLayerFrameSize(0, intermediate1.size());
  concatenated.SetSpatialLayerFrameSize(1, intermediate2.size());
  concatenated.SetSpatialLayerFrameSize(2, intermediate3.size());

  // Extract frame id from concatenated image
  EncodedImageExtractionResult out = injector.ExtractData(concatenated);

  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 2 * 10ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
    EXPECT_EQ(out.image.data()[i + 10], i + 21);
  }
  EXPECT_EQ(out.image.SpatialIndex().value_or(0), 2);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(1).value_or(0), 0ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(2).value_or(0), 10ul);
}

TEST(SingleProcessEncodedImageDataInjector,
     InjectExtractFromConcatenatedAllDiscarded) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/1);

  EncodedImage source1 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source1.SetRtpTimestamp(123456710);
  EncodedImage source2 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/11);
  source2.SetRtpTimestamp(123456710);
  EncodedImage source3 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/21);
  source3.SetRtpTimestamp(123456710);

  // Inject id into 3 images with same frame id.
  EncodedImage intermediate1 = injector.InjectData(512, true, source1);
  EncodedImage intermediate2 = injector.InjectData(512, true, source2);
  EncodedImage intermediate3 = injector.InjectData(512, true, source3);

  // Concatenate them into single encoded image, like it can be done in jitter
  // buffer.
  size_t concatenated_length =
      intermediate1.size() + intermediate2.size() + intermediate3.size();
  rtc::Buffer concatenated_buffer;
  concatenated_buffer.AppendData(intermediate1.data(), intermediate1.size());
  concatenated_buffer.AppendData(intermediate2.data(), intermediate2.size());
  concatenated_buffer.AppendData(intermediate3.data(), intermediate3.size());
  EncodedImage concatenated;
  concatenated.SetEncodedData(EncodedImageBuffer::Create(
      concatenated_buffer.data(), concatenated_length));
  concatenated.SetSpatialIndex(2);
  concatenated.SetSpatialLayerFrameSize(0, intermediate1.size());
  concatenated.SetSpatialLayerFrameSize(1, intermediate2.size());
  concatenated.SetSpatialLayerFrameSize(2, intermediate3.size());

  // Extract frame id from concatenated image
  EncodedImageExtractionResult out = injector.ExtractData(concatenated);

  EXPECT_EQ(out.id, 512);
  EXPECT_TRUE(out.discard);
  EXPECT_EQ(out.image.size(), 0ul);
  EXPECT_EQ(out.image.SpatialIndex().value_or(0), 2);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(out.image.SpatialLayerFrameSize(i).value_or(0), 0ul);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, InjectOnceExtractTwice) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/2);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImageExtractionResult out = injector.ExtractData(
      injector.InjectData(/*id=*/512, /*discard=*/false, source));
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
  out = injector.ExtractData(
      injector.InjectData(/*id=*/512, /*discard=*/false, source));
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, Add1stReceiverAfterStart) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/0);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);
  EncodedImage modified_image = injector.InjectData(
      /*id=*/512, /*discard=*/false, source);

  injector.AddParticipantInCall();
  EncodedImageExtractionResult out = injector.ExtractData(modified_image);

  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest, Add3rdReceiverAfterStart) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/2);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);
  EncodedImage modified_image = injector.InjectData(
      /*id=*/512, /*discard=*/false, source);
  injector.ExtractData(modified_image);

  injector.AddParticipantInCall();
  injector.ExtractData(modified_image);
  EncodedImageExtractionResult out = injector.ExtractData(modified_image);

  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

TEST(SingleProcessEncodedImageDataInjectorTest,
     RemoveReceiverRemovesOnlyFullyReceivedFrames) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/3);

  EncodedImage source1 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source1.SetRtpTimestamp(10);
  EncodedImage source2 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source2.SetRtpTimestamp(20);

  EncodedImage modified_image1 = injector.InjectData(
      /*id=*/512, /*discard=*/false, source1);
  EncodedImage modified_image2 = injector.InjectData(
      /*id=*/513, /*discard=*/false, source2);

  // Out of 3 receivers 1st image received by 2 and 2nd image by 1
  injector.ExtractData(DeepCopyEncodedImage(modified_image1));
  injector.ExtractData(DeepCopyEncodedImage(modified_image1));
  injector.ExtractData(DeepCopyEncodedImage(modified_image2));

  // When we removed one receiver, 2nd image should still be available for
  // extraction.
  injector.RemoveParticipantInCall();

  EncodedImageExtractionResult out =
      injector.ExtractData(DeepCopyEncodedImage(modified_image2));

  EXPECT_EQ(out.id, 513);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(SingleProcessEncodedImageDataInjectorTestDeathTest,
     InjectOnceExtractMoreThenExpected) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/2);

  EncodedImage source =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source.SetRtpTimestamp(123456789);

  EncodedImage modified =
      injector.InjectData(/*id=*/512, /*discard=*/false, source);

  injector.ExtractData(DeepCopyEncodedImage(modified));
  injector.ExtractData(DeepCopyEncodedImage(modified));
  EXPECT_DEATH(injector.ExtractData(DeepCopyEncodedImage(modified)),
               "Unknown sub_id=0 for frame_id=512");
}

TEST(SingleProcessEncodedImageDataInjectorTestDeathTest,
     RemoveReceiverRemovesOnlyFullyReceivedFramesVerifyFrameIsRemoved) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(/*expected_receivers_count=*/3);

  EncodedImage source1 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source1.SetRtpTimestamp(10);
  EncodedImage source2 =
      CreateEncodedImageOfSizeNFilledWithValuesFromX(/*n=*/10, /*x=*/1);
  source2.SetRtpTimestamp(20);

  EncodedImage modified_image1 = injector.InjectData(
      /*id=*/512, /*discard=*/false, source1);
  EncodedImage modified_image2 = injector.InjectData(
      /*id=*/513, /*discard=*/false, source2);

  // Out of 3 receivers 1st image received by 2 and 2nd image by 1
  injector.ExtractData(DeepCopyEncodedImage(modified_image1));
  injector.ExtractData(DeepCopyEncodedImage(modified_image1));
  injector.ExtractData(DeepCopyEncodedImage(modified_image2));

  // When we removed one receiver 1st image should be removed.
  injector.RemoveParticipantInCall();

  EXPECT_DEATH(injector.ExtractData(DeepCopyEncodedImage(modified_image1)),
               "Unknown sub_id=0 for frame_id=512");
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
