/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>
#include <string>
#include <vector>

#include "rtc_base/gunit.h"
#include "rtc_base/openssladapter.h"

namespace rtc {

TEST(OpenSSLAdapterTest, TestTransformAlpnProtocols) {
  EXPECT_EQ("", TransformAlpnProtocols(std::vector<std::string>()));

  // Protocols larger than 255 characters (whose size can't be fit in a byte),
  // can't be converted, and an empty string will be returned.
  std::string large_protocol(256, 'a');
  EXPECT_EQ("",
            TransformAlpnProtocols(std::vector<std::string>{large_protocol}));

  // One protocol test.
  std::vector<std::string> alpn_protos{"h2"};
  std::stringstream expected_response;
  expected_response << static_cast<char>(2) << "h2";
  EXPECT_EQ(expected_response.str(), TransformAlpnProtocols(alpn_protos));

  // Standard protocols test (h2,http/1.1).
  alpn_protos.push_back("http/1.1");
  expected_response << static_cast<char>(8) << "http/1.1";
  EXPECT_EQ(expected_response.str(), TransformAlpnProtocols(alpn_protos));
}

}  // namespace rtc
