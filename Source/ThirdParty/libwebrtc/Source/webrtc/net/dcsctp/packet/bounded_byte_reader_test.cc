/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "net/dcsctp/packet/bounded_byte_reader.h"

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(BoundedByteReaderTest, CanLoadData) {
  uint8_t data[14] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4};

  BoundedByteReader<8> reader(data);
  EXPECT_EQ(reader.variable_data_size(), 6U);
  EXPECT_EQ(reader.Load32<0>(), 0x01020304U);
  EXPECT_EQ(reader.Load32<4>(), 0x05060708U);
  EXPECT_EQ(reader.Load16<4>(), 0x0506U);
  EXPECT_EQ(reader.Load8<4>(), 0x05U);
  EXPECT_EQ(reader.Load8<5>(), 0x06U);

  BoundedByteReader<6> sub = reader.sub_reader<6>(0);
  EXPECT_EQ(sub.Load16<0>(), 0x0900U);
  EXPECT_EQ(sub.Load32<0>(), 0x09000102U);
  EXPECT_EQ(sub.Load16<4>(), 0x0304U);

  EXPECT_THAT(reader.variable_data(), ElementsAre(9, 0, 1, 2, 3, 4));
}

}  // namespace
}  // namespace dcsctp
