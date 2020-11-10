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

TEST(SingleProcessEncodedImageDataInjector, InjectExtractDiscardFalse) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(512, false, source, 1), 2);
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
}

TEST(SingleProcessEncodedImageDataInjector, InjectExtractDiscardTrue) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(512, true, source, 1), 2);
  EXPECT_EQ(out.id, 512);
  EXPECT_TRUE(out.discard);
  EXPECT_EQ(out.image.size(), 0ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
}

TEST(SingleProcessEncodedImageDataInjector, InjectWithUnsetSpatialLayerSizes) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImage intermediate = injector.InjectData(512, false, source, 1);
  intermediate.SetSpatialIndex(2);

  EncodedImageExtractionResult out = injector.ExtractData(intermediate, 2);
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

TEST(SingleProcessEncodedImageDataInjector, InjectWithZeroSpatialLayerSizes) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImage intermediate = injector.InjectData(512, false, source, 1);
  intermediate.SetSpatialIndex(2);
  intermediate.SetSpatialLayerFrameSize(0, 0);
  intermediate.SetSpatialLayerFrameSize(1, 0);
  intermediate.SetSpatialLayerFrameSize(2, 0);

  EncodedImageExtractionResult out = injector.ExtractData(intermediate, 2);
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

TEST(SingleProcessEncodedImageDataInjector, Inject3Extract3) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  // 1st frame
  EncodedImage source1 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source1.SetTimestamp(123456710);
  // 2nd frame 1st spatial layer
  EncodedImage source2 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 11);
  source2.SetTimestamp(123456720);
  // 2nd frame 2nd spatial layer
  EncodedImage source3 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 21);
  source3.SetTimestamp(123456720);

  EncodedImage intermediate1 = injector.InjectData(510, false, source1, 1);
  EncodedImage intermediate2 = injector.InjectData(520, true, source2, 1);
  EncodedImage intermediate3 = injector.InjectData(520, false, source3, 1);

  // Extract ids in different order.
  EncodedImageExtractionResult out3 = injector.ExtractData(intermediate3, 2);
  EncodedImageExtractionResult out1 = injector.ExtractData(intermediate1, 2);
  EncodedImageExtractionResult out2 = injector.ExtractData(intermediate2, 2);

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

TEST(SingleProcessEncodedImageDataInjector, InjectExtractFromConcatenated) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(1);

  EncodedImage source1 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source1.SetTimestamp(123456710);
  EncodedImage source2 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 11);
  source2.SetTimestamp(123456710);
  EncodedImage source3 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 21);
  source3.SetTimestamp(123456710);

  // Inject id into 3 images with same frame id.
  EncodedImage intermediate1 = injector.InjectData(512, false, source1, 1);
  EncodedImage intermediate2 = injector.InjectData(512, true, source2, 1);
  EncodedImage intermediate3 = injector.InjectData(512, false, source3, 1);

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
  EncodedImageExtractionResult out = injector.ExtractData(concatenated, 2);

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
  injector.Start(1);

  EncodedImage source1 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source1.SetTimestamp(123456710);
  EncodedImage source2 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 11);
  source2.SetTimestamp(123456710);
  EncodedImage source3 = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 21);
  source3.SetTimestamp(123456710);

  // Inject id into 3 images with same frame id.
  EncodedImage intermediate1 = injector.InjectData(512, true, source1, 1);
  EncodedImage intermediate2 = injector.InjectData(512, true, source2, 1);
  EncodedImage intermediate3 = injector.InjectData(512, true, source3, 1);

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
  EncodedImageExtractionResult out = injector.ExtractData(concatenated, 2);

  EXPECT_EQ(out.id, 512);
  EXPECT_TRUE(out.discard);
  EXPECT_EQ(out.image.size(), 0ul);
  EXPECT_EQ(out.image.SpatialIndex().value_or(0), 2);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(out.image.SpatialLayerFrameSize(i).value_or(0), 0ul);
  }
}

TEST(SingleProcessEncodedImageDataInjector, InjectOnceExtractTwice) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(2);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(/*id=*/512, /*discard=*/false,
                                               source, /*coding_entity_id=*/1),
                           /*coding_entity_id=*/2);
  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  EXPECT_EQ(out.image.SpatialLayerFrameSize(0).value_or(0), 0ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(out.image.data()[i], i + 1);
  }
  out =
      injector.ExtractData(injector.InjectData(/*id=*/512, /*discard=*/false,
                                               source, /*coding_entity_id=*/1),
                           2);
  EXPECT_EQ(out.id, 512);
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
EncodedImage DeepCopyEncodedImage(const EncodedImage& source) {
  EncodedImage copy = source;
  copy.SetEncodedData(EncodedImageBuffer::Create(source.data(), source.size()));
  return copy;
}

TEST(SingleProcessEncodedImageDataInjector, InjectOnceExtractMoreThenExpected) {
  SingleProcessEncodedImageDataInjector injector;
  injector.Start(2);

  EncodedImage source = CreateEncodedImageOfSizeNFilledWithValuesFromX(10, 1);
  source.SetTimestamp(123456789);

  EncodedImage modified = injector.InjectData(/*id=*/512, /*discard=*/false,
                                              source, /*coding_entity_id=*/1);

  injector.ExtractData(DeepCopyEncodedImage(modified), /*coding_entity_id=*/2);
  injector.ExtractData(DeepCopyEncodedImage(modified), /*coding_entity_id=*/2);
  EXPECT_DEATH(injector.ExtractData(DeepCopyEncodedImage(modified),
                                    /*coding_entity_id=*/2),
               "Unknown sub_id=0 for frame_id=512");
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
