/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "net/dcsctp/packet/bounded_byte_writer.h"

#include <vector>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(BoundedByteWriterTest, CanWriteData) {
  std::vector<uint8_t> data(14);

  BoundedByteWriter<8> writer(data);
  writer.Store32<0>(0x01020304);
  writer.Store16<4>(0x0506);
  writer.Store8<6>(0x07);
  writer.Store8<7>(0x08);

  uint8_t variable_data[] = {0, 0, 0, 0, 3, 0};
  writer.CopyToVariableData(variable_data);

  BoundedByteWriter<6> sub = writer.sub_writer<6>(0);
  sub.Store32<0>(0x09000000);
  sub.Store16<2>(0x0102);

  BoundedByteWriter<2> sub2 = writer.sub_writer<2>(4);
  sub2.Store8<1>(0x04);

  EXPECT_THAT(data, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4));
}

}  // namespace
}  // namespace dcsctp
