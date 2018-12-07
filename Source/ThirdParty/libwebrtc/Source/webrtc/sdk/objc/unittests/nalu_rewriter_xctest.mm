/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/h264_common.h"
#include "components/video_codec/nalu_rewriter.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/gunit.h"

#import <XCTest/XCTest.h>

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#endif

@interface NaluRewriterTests : XCTestCase

@end

static const uint8_t NALU_TEST_DATA_0[] = {0xAA, 0xBB, 0xCC};
static const uint8_t NALU_TEST_DATA_1[] = {0xDE, 0xAD, 0xBE, 0xEF};

// clang-format off
static const uint8_t SPS_PPS_BUFFER[] = {
  // SPS nalu.
  0x00, 0x00, 0x00, 0x01, 0x27, 0x42, 0x00, 0x1E, 0xAB, 0x40, 0xF0, 0x28,
  0xD3, 0x70, 0x20, 0x20, 0x20, 0x20,
  // PPS nalu.
  0x00, 0x00, 0x00, 0x01, 0x28, 0xCE, 0x3C, 0x30};
// clang-format on

@implementation NaluRewriterTests

- (void)testCreateVideoFormatDescription {
  CMVideoFormatDescriptionRef description =
      webrtc::CreateVideoFormatDescription(SPS_PPS_BUFFER, arraysize(SPS_PPS_BUFFER));
  XCTAssertTrue(description);
  if (description) {
    CFRelease(description);
    description = nullptr;
  }

  // clang-format off
  const uint8_t sps_pps_not_at_start_buffer[] = {
    // Add some non-SPS/PPS NALUs at the beginning
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x01,
    0xAB, 0x33, 0x21,
    // SPS nalu.
    0x00, 0x00, 0x01, 0x27, 0x42, 0x00, 0x1E, 0xAB, 0x40, 0xF0, 0x28, 0xD3,
    0x70, 0x20, 0x20, 0x20, 0x20,
    // PPS nalu.
    0x00, 0x00, 0x01, 0x28, 0xCE, 0x3C, 0x30};
  // clang-format on
  description = webrtc::CreateVideoFormatDescription(sps_pps_not_at_start_buffer,
                                                     arraysize(sps_pps_not_at_start_buffer));

  XCTAssertTrue(description);

  if (description) {
    CFRelease(description);
    description = nullptr;
  }

  const uint8_t other_buffer[] = {0x00, 0x00, 0x00, 0x01, 0x28};
  XCTAssertFalse(webrtc::CreateVideoFormatDescription(other_buffer, arraysize(other_buffer)));
}

- (void)testReadEmptyInput {
  const uint8_t annex_b_test_data[] = {0x00};
  webrtc::AnnexBBufferReader reader(annex_b_test_data, 0);
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  XCTAssertEqual(0u, reader.BytesRemaining());
  XCTAssertFalse(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(nullptr, nalu);
  XCTAssertEqual(0u, nalu_length);
}

- (void)testReadSingleNalu {
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x00, 0x01, 0xAA};
  webrtc::AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  XCTAssertEqual(arraysize(annex_b_test_data), reader.BytesRemaining());
  XCTAssertTrue(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(annex_b_test_data + 4, nalu);
  XCTAssertEqual(1u, nalu_length);
  XCTAssertEqual(0u, reader.BytesRemaining());
  XCTAssertFalse(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(nullptr, nalu);
  XCTAssertEqual(0u, nalu_length);
}

- (void)testReadSingleNalu3ByteHeader {
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x01, 0xAA};
  webrtc::AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  XCTAssertEqual(arraysize(annex_b_test_data), reader.BytesRemaining());
  XCTAssertTrue(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(annex_b_test_data + 3, nalu);
  XCTAssertEqual(1u, nalu_length);
  XCTAssertEqual(0u, reader.BytesRemaining());
  XCTAssertFalse(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(nullptr, nalu);
  XCTAssertEqual(0u, nalu_length);
}

- (void)testReadMissingNalu {
  // clang-format off
  const uint8_t annex_b_test_data[] = {0x01,
                                       0x00, 0x01,
                                       0x00, 0x00, 0x00, 0xFF};
  // clang-format on
  webrtc::AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  XCTAssertEqual(0u, reader.BytesRemaining());
  XCTAssertFalse(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(nullptr, nalu);
  XCTAssertEqual(0u, nalu_length);
}

- (void)testReadMultipleNalus {
  // clang-format off
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x00, 0x01, 0xFF,
                                       0x01,
                                       0x00, 0x01,
                                       0x00, 0x00, 0x00, 0xFF,
                                       0x00, 0x00, 0x01, 0xAA, 0xBB};
  // clang-format on
  webrtc::AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  XCTAssertEqual(arraysize(annex_b_test_data), reader.BytesRemaining());
  XCTAssertTrue(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(annex_b_test_data + 4, nalu);
  XCTAssertEqual(8u, nalu_length);
  XCTAssertEqual(5u, reader.BytesRemaining());
  XCTAssertTrue(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(annex_b_test_data + 15, nalu);
  XCTAssertEqual(2u, nalu_length);
  XCTAssertEqual(0u, reader.BytesRemaining());
  XCTAssertFalse(reader.ReadNalu(&nalu, &nalu_length));
  XCTAssertEqual(nullptr, nalu);
  XCTAssertEqual(0u, nalu_length);
}

- (void)testEmptyOutputBuffer {
  const uint8_t expected_buffer[] = {0x00};
  const size_t buffer_size = 1;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memset(buffer.get(), 0, buffer_size);
  webrtc::AvccBufferWriter writer(buffer.get(), 0);
  XCTAssertEqual(0u, writer.BytesRemaining());
  XCTAssertFalse(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  XCTAssertEqual(0, memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

- (void)testWriteSingleNalu {
  const uint8_t expected_buffer[] = {
      0x00, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC,
  };
  const size_t buffer_size = arraysize(NALU_TEST_DATA_0) + 4;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  webrtc::AvccBufferWriter writer(buffer.get(), buffer_size);
  XCTAssertEqual(buffer_size, writer.BytesRemaining());
  XCTAssertTrue(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  XCTAssertEqual(0u, writer.BytesRemaining());
  XCTAssertFalse(writer.WriteNalu(NALU_TEST_DATA_1, arraysize(NALU_TEST_DATA_1)));
  XCTAssertEqual(0, memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

- (void)testWriteMultipleNalus {
  // clang-format off
  const uint8_t expected_buffer[] = {
    0x00, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC,
    0x00, 0x00, 0x00, 0x04, 0xDE, 0xAD, 0xBE, 0xEF
  };
  // clang-format on
  const size_t buffer_size = arraysize(NALU_TEST_DATA_0) + arraysize(NALU_TEST_DATA_1) + 8;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  webrtc::AvccBufferWriter writer(buffer.get(), buffer_size);
  XCTAssertEqual(buffer_size, writer.BytesRemaining());
  XCTAssertTrue(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  XCTAssertEqual(buffer_size - (arraysize(NALU_TEST_DATA_0) + 4), writer.BytesRemaining());
  XCTAssertTrue(writer.WriteNalu(NALU_TEST_DATA_1, arraysize(NALU_TEST_DATA_1)));
  XCTAssertEqual(0u, writer.BytesRemaining());
  XCTAssertEqual(0, memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

- (void)testOverflow {
  const uint8_t expected_buffer[] = {0x00, 0x00, 0x00};
  const size_t buffer_size = arraysize(NALU_TEST_DATA_0);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memset(buffer.get(), 0, buffer_size);
  webrtc::AvccBufferWriter writer(buffer.get(), buffer_size);
  XCTAssertEqual(buffer_size, writer.BytesRemaining());
  XCTAssertFalse(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  XCTAssertEqual(buffer_size, writer.BytesRemaining());
  XCTAssertEqual(0, memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

- (void)testH264AnnexBBufferToCMSampleBuffer {
  // clang-format off
  const uint8_t annex_b_test_data[] = {
    0x00,
    0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x01,
    0xAA, 0xFF, // second chunk, 2 bytes
    0x00, 0x00, 0x01,
    0xBB};  // third chunk, 1 byte, will not fit into output array

  const uint8_t expected_cmsample_data[] = {
    0x00, 0x00, 0x00, 0x04,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x00, 0x02,
    0xAA, 0xFF}; // second chunk, 2 bytes
  // clang-format on

  CMMemoryPoolRef memory_pool = CMMemoryPoolCreate(nil);
  CMSampleBufferRef out_sample_buffer = nil;
  CMVideoFormatDescriptionRef description = [self createDescription];

  Boolean result = webrtc::H264AnnexBBufferToCMSampleBuffer(annex_b_test_data,
                                                            arraysize(annex_b_test_data),
                                                            description,
                                                            &out_sample_buffer,
                                                            memory_pool);

  XCTAssertTrue(result);

  XCTAssertEqual(description, CMSampleBufferGetFormatDescription(out_sample_buffer));

  char* data_ptr = nullptr;
  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(out_sample_buffer);
  size_t block_buffer_size = CMBlockBufferGetDataLength(block_buffer);
  CMBlockBufferGetDataPointer(block_buffer, 0, nullptr, nullptr, &data_ptr);
  XCTAssertEqual(block_buffer_size, arraysize(annex_b_test_data));

  int data_comparison_result =
      memcmp(expected_cmsample_data, data_ptr, arraysize(expected_cmsample_data));

  XCTAssertEqual(0, data_comparison_result);

  if (description) {
    CFRelease(description);
    description = nullptr;
  }

  CMMemoryPoolInvalidate(memory_pool);
  CFRelease(memory_pool);
}

- (void)testH264CMSampleBufferToAnnexBBuffer {
  // clang-format off
  const uint8_t cmsample_data[] = {
    0x00, 0x00, 0x00, 0x04,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x00, 0x02,
    0xAA, 0xFF}; // second chunk, 2 bytes

  const uint8_t expected_annex_b_data[] = {
    0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x00, 0x01,
    0xAA, 0xFF}; // second chunk, 2 bytes
  // clang-format on

  rtc::Buffer annexb_buffer(arraysize(cmsample_data));
  std::unique_ptr<webrtc::RTPFragmentationHeader> out_header_ptr;
  CMSampleBufferRef sample_buffer =
      [self createCMSampleBufferRef:(void*)cmsample_data cmsampleSize:arraysize(cmsample_data)];

  Boolean result = webrtc::H264CMSampleBufferToAnnexBBuffer(sample_buffer,
                                                            /* is_keyframe */ false,
                                                            &annexb_buffer,
                                                            &out_header_ptr);

  XCTAssertTrue(result);

  XCTAssertEqual(arraysize(expected_annex_b_data), annexb_buffer.size());

  int data_comparison_result =
      memcmp(expected_annex_b_data, annexb_buffer.data(), arraysize(expected_annex_b_data));

  XCTAssertEqual(0, data_comparison_result);

  webrtc::RTPFragmentationHeader* out_header = out_header_ptr.get();

  XCTAssertEqual(2, (int)out_header->Size());

  XCTAssertEqual(4, (int)out_header->Offset(0));
  XCTAssertEqual(4, (int)out_header->Length(0));
  XCTAssertEqual(0, (int)out_header->TimeDiff(0));
  XCTAssertEqual(0, (int)out_header->PayloadType(0));

  XCTAssertEqual(12, (int)out_header->Offset(1));
  XCTAssertEqual(2, (int)out_header->Length(1));
  XCTAssertEqual(0, (int)out_header->TimeDiff(1));
  XCTAssertEqual(0, (int)out_header->PayloadType(1));
}

- (void)testH264CMSampleBufferToAnnexBBufferWithKeyframe {
  // clang-format off
  const uint8_t cmsample_data[] = {
    0x00, 0x00, 0x00, 0x04,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x00, 0x02,
    0xAA, 0xFF}; // second chunk, 2 bytes

  const uint8_t expected_annex_b_data[] = {
    0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0xFF, // first chunk, 4 bytes
    0x00, 0x00, 0x00, 0x01,
    0xAA, 0xFF}; // second chunk, 2 bytes
  // clang-format on

  rtc::Buffer annexb_buffer(arraysize(cmsample_data));
  std::unique_ptr<webrtc::RTPFragmentationHeader> out_header_ptr;
  CMSampleBufferRef sample_buffer =
      [self createCMSampleBufferRef:(void*)cmsample_data cmsampleSize:arraysize(cmsample_data)];

  Boolean result = webrtc::H264CMSampleBufferToAnnexBBuffer(sample_buffer,
                                                            /* is_keyframe */ true,
                                                            &annexb_buffer,
                                                            &out_header_ptr);

  XCTAssertTrue(result);

  XCTAssertEqual(arraysize(SPS_PPS_BUFFER) + arraysize(expected_annex_b_data),
                 annexb_buffer.size());

  XCTAssertEqual(0, memcmp(SPS_PPS_BUFFER, annexb_buffer.data(), arraysize(SPS_PPS_BUFFER)));

  XCTAssertEqual(0,
                 memcmp(expected_annex_b_data,
                        annexb_buffer.data() + arraysize(SPS_PPS_BUFFER),
                        arraysize(expected_annex_b_data)));

  webrtc::RTPFragmentationHeader* out_header = out_header_ptr.get();

  XCTAssertEqual(4, (int)out_header->Size());

  XCTAssertEqual(4, (int)out_header->Offset(0));
  XCTAssertEqual(14, (int)out_header->Length(0));
  XCTAssertEqual(0, (int)out_header->TimeDiff(0));
  XCTAssertEqual(0, (int)out_header->PayloadType(0));

  XCTAssertEqual(22, (int)out_header->Offset(1));
  XCTAssertEqual(4, (int)out_header->Length(1));
  XCTAssertEqual(0, (int)out_header->TimeDiff(1));
  XCTAssertEqual(0, (int)out_header->PayloadType(1));

  XCTAssertEqual(30, (int)out_header->Offset(2));
  XCTAssertEqual(4, (int)out_header->Length(2));
  XCTAssertEqual(0, (int)out_header->TimeDiff(2));
  XCTAssertEqual(0, (int)out_header->PayloadType(2));

  XCTAssertEqual(38, (int)out_header->Offset(3));
  XCTAssertEqual(2, (int)out_header->Length(3));
  XCTAssertEqual(0, (int)out_header->TimeDiff(3));
  XCTAssertEqual(0, (int)out_header->PayloadType(3));
}

- (CMVideoFormatDescriptionRef)createDescription {
  CMVideoFormatDescriptionRef description =
      webrtc::CreateVideoFormatDescription(SPS_PPS_BUFFER, arraysize(SPS_PPS_BUFFER));
  XCTAssertTrue(description);
  return description;
}

- (CMSampleBufferRef)createCMSampleBufferRef:(void*)cmsampleData cmsampleSize:(size_t)cmsampleSize {
  CMSampleBufferRef sample_buffer = nil;
  OSStatus status;

  CMVideoFormatDescriptionRef description = [self createDescription];
  CMBlockBufferRef block_buffer = nullptr;

  status = CMBlockBufferCreateWithMemoryBlock(nullptr,
                                              cmsampleData,
                                              cmsampleSize,
                                              nullptr,
                                              nullptr,
                                              0,
                                              cmsampleSize,
                                              kCMBlockBufferAssureMemoryNowFlag,
                                              &block_buffer);

  status = CMSampleBufferCreate(nullptr,
                                block_buffer,
                                true,
                                nullptr,
                                nullptr,
                                description,
                                1,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                &sample_buffer);

  return sample_buffer;
}

@end
