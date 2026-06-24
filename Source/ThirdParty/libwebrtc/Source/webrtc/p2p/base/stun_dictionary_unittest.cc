/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_dictionary.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "api/transport/stun.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket_address.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void Sync(StunDictionaryView& dictionary, StunDictionaryWriter& writer) {
  int pending = writer.Pending();
  auto delta = writer.CreateDelta();
  if (delta == nullptr) {
    EXPECT_EQ(pending, 0);
  } else {
    EXPECT_NE(pending, 0);
    auto delta_ack = dictionary.ApplyDelta(*delta);
    if (!delta_ack.ok()) {
      RTC_LOG(LS_ERROR) << "delta_ack.error(): " << delta_ack.error().message();
    }
    EXPECT_TRUE(delta_ack.ok());
    ASSERT_NE(delta_ack.value().first.get(), nullptr);
    writer.ApplyDeltaAck(*delta_ack.value().first);
    EXPECT_FALSE(writer.Pending());
  }
}

void XorToggle(StunByteStringAttribute& attr, size_t byte) {
  ASSERT_TRUE(attr.length() > byte);
  uint8_t val = attr.GetByte(byte);
  uint8_t new_val = val ^ (128 - (byte & 255));
  attr.SetByte(byte, new_val);
}

std::unique_ptr<StunByteStringAttribute> Crop(
    const StunByteStringAttribute& attr,
    int new_length) {
  auto new_attr = std::make_unique<StunByteStringAttribute>(attr.type());
  std::string content = std::string(attr.string_view());
  content.erase(new_length);
  new_attr->CopyBytes(content);
  return new_attr;
}

}  // namespace

constexpr int kKey1 = 100;

TEST(StunDictionary, CreateEmptyDictionaryWriter) {
  StunDictionaryView dictionary;
  StunDictionaryWriter writer;
  EXPECT_TRUE(dictionary.empty());
  EXPECT_TRUE(writer->empty());
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_EQ(writer.CreateDelta().get(), nullptr);
}

TEST(StunDictionary, SetAndGet) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer->GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(writer->GetUInt64(kKey1), nullptr);
  EXPECT_EQ(writer->GetByteString(kKey1), nullptr);
  EXPECT_EQ(writer->GetAddress(kKey1), nullptr);
  EXPECT_EQ(writer->GetUInt16List(kKey1), nullptr);
}

TEST(StunDictionary, SetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);

  StunDictionaryView dictionary;
  EXPECT_TRUE(dictionary.empty());

  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.bytes_stored(), 12);
}

TEST(StunDictionary, SetSetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt32(kKey1)->SetValue(29);

  StunDictionaryView dictionary;
  EXPECT_TRUE(dictionary.empty());

  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 29u);
  EXPECT_EQ(dictionary.bytes_stored(), 12);
}

TEST(StunDictionary, SetAndApplyAndSetAndApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);

  StunDictionaryView dictionary;
  EXPECT_TRUE(dictionary.empty());

  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.bytes_stored(), 12);

  writer.SetUInt32(kKey1)->SetValue(29);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 29u);
  EXPECT_EQ(dictionary.bytes_stored(), 12);
}

TEST(StunDictionary, ChangeType) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer->GetUInt32(kKey1)->value(), 27u);

  writer.SetUInt64(kKey1)->SetValue(29);
  EXPECT_EQ(writer->GetUInt32(kKey1), nullptr);
  EXPECT_EQ(writer->GetUInt64(kKey1)->value(), 29ull);
}

TEST(StunDictionary, ChangeTypeApply) {
  StunDictionaryWriter writer;
  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer->GetUInt32(kKey1)->value(), 27u);

  StunDictionaryView dictionary;
  EXPECT_TRUE(dictionary.empty());
  Sync(dictionary, writer);
  EXPECT_EQ(writer->GetUInt32(kKey1)->value(), 27u);

  writer.SetUInt64(kKey1)->SetValue(29);
  EXPECT_EQ(writer->GetUInt32(kKey1), nullptr);
  EXPECT_EQ(writer->GetUInt64(kKey1)->value(), 29ull);

  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(dictionary.GetUInt64(kKey1)->value(), 29ull);
  EXPECT_EQ(dictionary.bytes_stored(), 16);
}

TEST(StunDictionary, Pending) {
  StunDictionaryWriter writer;
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_FALSE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(27);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(29);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1 + 1)->SetValue(31);
  EXPECT_EQ(writer.Pending(), 2);
  EXPECT_TRUE(writer.Pending(kKey1));
  EXPECT_TRUE(writer.Pending(kKey1 + 1));

  StunDictionaryView dictionary;

  Sync(dictionary, writer);
  EXPECT_EQ(writer.Pending(), 0);
  EXPECT_FALSE(writer.Pending(kKey1));

  writer.SetUInt32(kKey1)->SetValue(32);
  EXPECT_EQ(writer.Pending(), 1);
  EXPECT_TRUE(writer.Pending(kKey1));
}

TEST(StunDictionary, Delete) {
  StunDictionaryWriter writer;
  StunDictionaryView dictionary;

  writer.SetUInt32(kKey1)->SetValue(27);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.bytes_stored(), 12);

  writer.Delete(kKey1);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1), nullptr);
  EXPECT_EQ(dictionary.bytes_stored(), 8);

  writer.Delete(kKey1);
  EXPECT_EQ(writer.Pending(), 0);
}

TEST(StunDictionary, MultiWriter) {
  StunDictionaryWriter writer1;
  StunDictionaryWriter writer2;
  StunDictionaryView dictionary;

  writer1.SetUInt32(kKey1)->SetValue(27);
  Sync(dictionary, writer1);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);

  writer2.SetUInt32(kKey1 + 1)->SetValue(28);
  Sync(dictionary, writer2);
  EXPECT_EQ(dictionary.GetUInt32(kKey1 + 1)->value(), 28u);

  writer1.Delete(kKey1);
  Sync(dictionary, writer1);
  EXPECT_EQ(dictionary.GetUInt32(kKey1), nullptr);

  writer2.Delete(kKey1 + 1);
  Sync(dictionary, writer2);
  EXPECT_EQ(dictionary.GetUInt32(kKey1 + 1), nullptr);
}

TEST(StunDictionary, BytesStoredIsCountedCorrectlyAfterMultipleUpdates) {
  StunDictionaryWriter writer;
  StunDictionaryView dictionary;

  for (int i = 0; i < 10; i++) {
    writer.SetUInt32(kKey1)->SetValue(27);
    writer.SetUInt64(kKey1 + 1)->SetValue(28);
    Sync(dictionary, writer);
    EXPECT_EQ(dictionary.bytes_stored(), 28);
    EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
    EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1)->value(), 28ull);
    writer.Delete(kKey1);
    Sync(dictionary, writer);
    EXPECT_EQ(dictionary.bytes_stored(), 24);
    EXPECT_EQ(dictionary.GetUInt32(kKey1), nullptr);
    EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1)->value(), 28ull);
    writer.Delete(kKey1 + 1);
    Sync(dictionary, writer);
    EXPECT_EQ(dictionary.bytes_stored(), 16);
    EXPECT_EQ(dictionary.GetUInt32(kKey1), nullptr);
    EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1), nullptr);
  }
}

TEST(StunDictionary, MaxBytesStoredCausesErrorOnOverflow) {
  StunDictionaryWriter writer;
  StunDictionaryView dictionary;

  dictionary.set_max_bytes_stored(30);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.bytes_stored(), 28);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1)->value(), 28ull);

  writer.SetByteString(kKey1 + 2)->CopyBytes("k");
  {
    auto delta = writer.CreateDelta();
    auto delta_ack = dictionary.ApplyDelta(*delta);
    EXPECT_FALSE(delta_ack.ok());
  }
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1)->value(), 28ull);
  EXPECT_EQ(dictionary.GetByteString(kKey1 + 2), nullptr);

  writer.Delete(kKey1 + 1);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1), nullptr);
  EXPECT_EQ(dictionary.GetByteString(kKey1 + 2)->string_view(), "k");
}

TEST(StunDictionary, DataTypes) {
  StunDictionaryWriter writer;
  StunDictionaryView dictionary;

  SocketAddress addr("127.0.0.1", 8080);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  writer.SetAddress(kKey1 + 2)->SetAddress(addr);
  writer.SetByteString(kKey1 + 3)->CopyBytes("keso");
  writer.SetUInt16List(kKey1 + 4)->AddTypeAtIndex(0, 7);

  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);
  EXPECT_EQ(dictionary.GetUInt64(kKey1 + 1)->value(), 28ull);
  EXPECT_EQ(dictionary.GetAddress(kKey1 + 2)->GetAddress(), addr);
  EXPECT_EQ(dictionary.GetByteString(kKey1 + 3)->string_view(), "keso");
  EXPECT_EQ(dictionary.GetUInt16List(kKey1 + 4)->GetType(0), 7);
}

TEST(StunDictionary, ParseError) {
  StunDictionaryWriter writer;
  StunDictionaryView dictionary;

  SocketAddress addr("127.0.0.1", 8080);

  writer.SetUInt32(kKey1)->SetValue(27);
  writer.SetUInt64(kKey1 + 1)->SetValue(28);
  writer.SetAddress(kKey1 + 2)->SetAddress(addr);
  writer.SetByteString(kKey1 + 3)->CopyBytes("keso");
  writer.SetUInt16List(kKey1 + 4)->AddTypeAtIndex(0, 7);

  auto delta = writer.CreateDelta();

  // The first 10 bytes are in the header...
  // any modification makes parsing fail.
  for (int i = 0; i < 10; i++) {
    XorToggle(*delta, i);
    EXPECT_FALSE(dictionary.ApplyDelta(*delta).ok());
    XorToggle(*delta, i);  // toogle back
  }

  // Remove bytes from the delta.
  for (size_t i = 0; i < delta->length(); i++) {
    // The delta does not contain a footer,
    // so it it possible to Crop at special values (attribute boundaries)
    // and apply will still work.
    const std::vector<int> valid_crop_length = {18, 28, 42, 56, 66, 74};
    bool valid = std::find(valid_crop_length.begin(), valid_crop_length.end(),
                           i) != valid_crop_length.end();
    auto cropped_delta = Crop(*delta, i);
    if (valid) {
      EXPECT_TRUE(dictionary.ApplyDelta(*cropped_delta).ok());
    } else {
      EXPECT_FALSE(dictionary.ApplyDelta(*cropped_delta).ok());
    }
  }
}

TEST(StunDictionary, DuplicateKeysDesyncBytesStored) {
  StunDictionaryView dictionary;
  StunDictionaryWriter writer;

  // 1. Store a value initially.
  writer.SetUInt32(kKey1)->SetValue(10);
  Sync(dictionary, writer);
  EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 10u);
  int initial_bytes = dictionary.bytes_stored();
  EXPECT_EQ(initial_bytes, 12);

  // 2. Manually construct a delta with duplicate keys.
  // We want version 3.
  // It should contain:
  // - Version attribute (version 3)
  // - kKey1 (UINT32, value 27)
  // - kKey1 (BYTE_STRING, length 0) [delete]

  ByteBufferWriter buf;
  buf.WriteUInt16(StunDictionaryView::kDeltaMagic);
  buf.WriteUInt16(StunDictionaryView::kDeltaVersion);

  // Version attribute
  buf.WriteUInt16(StunDictionaryView::kVersionKey);
  buf.WriteUInt16(8);
  buf.WriteUInt16(STUN_VALUE_UINT64);
  buf.WriteUInt64(3);  // Version 3

  // First attribute: kKey1 (UINT32, value 27)
  buf.WriteUInt16(kKey1);
  buf.WriteUInt16(4);  // length
  buf.WriteUInt16(STUN_VALUE_UINT32);
  buf.WriteUInt32(27);

  // Second attribute: kKey1 (BYTE_STRING, length 0) [delete]
  buf.WriteUInt16(kKey1);
  buf.WriteUInt16(0);  // length
  buf.WriteUInt16(STUN_VALUE_BYTE_STRING);

  StunByteStringAttribute delta(STUN_ATTR_GOOG_DELTA, buf.DataView());

  // 3. Apply the delta
  auto delta_ack = dictionary.ApplyDelta(delta);
  // We expect this to fail once fixed.
  EXPECT_FALSE(delta_ack.ok());

  if (delta_ack.ok()) {
    // 4. Verify desync (only if we didn't fail as expected)
    ASSERT_NE(dictionary.GetUInt32(kKey1), nullptr);
    EXPECT_EQ(dictionary.GetUInt32(kKey1)->value(), 27u);

    // In the bugged version, this will be 8 instead of 12.
    EXPECT_EQ(dictionary.bytes_stored(), 8);
  }
}

TEST(StunDictionary, TombstoneLifeline) {
  StunDictionaryWriter writer;

  // 1. Set kKey1 (will be version 2)
  writer.SetUInt32(kKey1)->SetValue(10);

  // 2. Set kKey2 (will be version 3)
  writer.SetUInt32(kKey1 + 1)->SetValue(20);

  // 3. Delete kKey2 (will remove version 3, add version 4 tombstone)
  writer.Delete(kKey1 + 1);

  // Now pending has:
  // (2, kKey1_attr)
  // (4, kKey2_tombstone)

  // We construct an ACK for version 2.
  StunUInt64Attribute ack(STUN_ATTR_GOOG_DELTA_ACK, 2);

  // This should remove kKey1 from pending, but KEEP kKey2 tombstone.
  writer.ApplyDeltaAck(ack);

  // If the bug is present, the tombstone for kKey2 has been deleted from
  // writer.tombstones_, but it is still in pending_. Calling CreateDelta will
  // try to access it and crash/UAF.
  auto delta = writer.CreateDelta();

  // If we survived, let's make sure the delta contains the tombstone.
  ASSERT_NE(delta, nullptr);
}

}  // namespace webrtc
