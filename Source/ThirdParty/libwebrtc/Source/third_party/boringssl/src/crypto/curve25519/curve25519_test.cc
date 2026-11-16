// Copyright 2026 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <limits.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "../test/test_util.h"
#include "internal.h"

using namespace bssl;

namespace {

#if !defined(BORINGSSL_SHARED_LIBRARY)

TEST(Curve25519Test, MapToCurveElligator2RFC9380) {
  struct Elligator2Test {
    // RFC 9380 big-endian hex strings
    const char *u_hex;
    const char *qx_hex;
    const char *qy_hex;
  };

  const Elligator2Test kTests[] = {
      // RFC 9380, Appendix J.4.1 (curve25519_XMD:SHA-512_ELL2_RO_)
      // We include this because we are testing map_to_curve_elligator2
      {"005fe8a7b8fef0a16c105e6cadf5a6740b3365e18692a9c05bfbb4"
       "d97f645a6a",
       "36b4df0c864c64707cbf6cf36e9ee2c09a6cb93b28313c169be295"
       "61bb904f98",
       "6cd59d664fb58c66c892883cd0eb792e52055284dac3907dd756b4"
       "5d15c3983d"},
      {"1347edbec6a2b5d8c02e058819819bee177077c9d10a4ce165aab0"
       "fd0252261a",
       "3fa114783a505c0b2b2fbeef0102853c0b494e7757f2a089d0daae"
       "7ed9a0db2b",
       "76c0fe7fec932aaafb8eefb42d9cbb32eb931158f469ff3050af15"
       "cfdbbeff94"},
      {"49bed021c7a3748f09fa8cdfcac044089f7829d3531066ac9e74e0"
       "994e05bc7d",
       "16b3d86e056b7970fa00165f6f48d90b619ad618791661b7b5e1ec"
       "78be10eac1",
       "4ab256422d84c5120b278cbdfc4e1facc5baadffeccecf8ee9bf39"
       "46106d50ca"},
      {"5c36525b663e63389d886105cee7ed712325d5a97e60e140aba7e2"
       "ce5ae851b6",
       "7ec29ddbf34539c40adfa98fcb39ec36368f47f30e8f888cc7e86f"
       "4d46e0c264",
       "10d1abc1cae2d34c06e247f2141ba897657fb39f1080d54f09ce0a"
       "f128067c74"},
      {"6412b7485ba26d3d1b6c290a8e1435b2959f03721874939b21782d"
       "f17323d160",
       "71de3dadfe268872326c35ac512164850860567aea0e7325e6b91a"
       "98f86533ad",
       "26a08b6e9a18084c56f2147bf515414b9b63f1522e1b6c5649f7d4"
       "b0324296ec"},
      {"24c7b46c1c6d9a21d32f5707be1380ab82db1054fde82865d5c9e3"
       "d968f287b2",
       "5704069021f61e41779e2ba6b932268316d6d2a6f064f997a22fef"
       "16d1eaeaca",
       "50483c7540f64fb4497619c050f2c7fe55454ec0f0e79870bb4430"
       "2e34232210"},
      {"5e123990f11bbb5586613ffabdb58d47f64bb5f2fa115f8ea8df01"
       "88e0c9e1b5",
       "7a94d45a198fb5daa381f45f2619ab279744efdd8bd8ed587fc5b6"
       "5d6cea1df0",
       "67d44f85d376e64bb7d713585230cdbfafc8e2676f7568e0b6ee59"
       "361116a6e1"},
      {"5e8553eb00438a0bb1e7faa59dec6d8087f9c8011e5fb8ed9df31c"
       "b6c0d4ac19",
       "30506fb7a32136694abd61b6113770270debe593027a968a01f271"
       "e146e60c18",
       "7eeee0e706b40c6b5174e551426a67f975ad5a977ee2f01e8e20a6"
       "d612458c3b"},
      {"20f481e85da7a3bf60ac0fb11ed1d0558fc6f941b3ac5469aa8b56"
       "ec883d6d7d",
       "02d606e2699b918ee36f2818f2bc5013e437e673c9f9b9cdc15fd0"
       "c5ee913970",
       "29e9dc92297231ef211245db9e31767996c5625dfbf92e1c8107ef"
       "887365de1e"},
      {"017d57fd257e9a78913999a23b52ca988157a81b09c5442501d07f"
       "ed20869465",
       "38920e9b988d1ab7449c0fa9a6058192c0c797bb3d42ac34572434"
       "1a1aa98745",
       "24dcc1be7c4d591d307e89049fd2ed30aae8911245a9d8554bf603"
       "2e5aa40d3d"},
      // RFC 9380, Appendix J.4.2 (curve25519_XMD:SHA-512_ELL2_NU_)
      {"608d892b641f0328523802a6603427c26e55e6f27e71a91a478148"
       "d45b5093cd",
       "51125222da5e763d97f3c10fcc92ea6860b9ccbbd2eb1285728f56"
       "6721c1e65b",
       "343d2204f812d3dfc5304a5808c6c0d81a903a5d228b342442aa3c"
       "9ba5520a3d"},
      {"46f5b22494bfeaa7f232cc8d054be68561af50230234d7d1d63d1d"
       "9abeca8da5",
       "7d56d1e08cb0ccb92baf069c18c49bb5a0dcd927eff8dcf75ca921"
       "ef7f3e6eeb",
       "404d9a7dc25c9c05c44ab9a94590e7c3fe2dcec74533a0b24b188a"
       "5d5dacf429"},
      {"235fe40c443766ce7e18111c33862d66c3b33267efa50d50f9e8e5"
       "d252a40aaa",
       "3fbe66b9c9883d79e8407150e7c2a1c8680bee496c62fabe4619a7"
       "2b3cabe90f",
       "08ec476147c9a0a3ff312d303dbbd076abb7551e5fce82b48ab14b"
       "433f8d0a7b"},
      {"001e92a544463bda9bd04ddbe3d6eed248f82de32f522669efc5dd"
       "ce95f46f5b",
       "227e0bb89de700385d19ec40e857db6e6a3e634b1c32962f370d26"
       "f84ff19683",
       "5f86ff3851d262727326a32c1bf7655a03665830fa7f1b8b1e5a09"
       "d85bc66e4a"},
      {"1a68a1af9f663592291af987203393f707305c7bac9c8d63d6a729"
       "bdc553dc19",
       "3bcd651ee54d5f7b6013898aab251ee8ecc0688166fce6e9548d38"
       "472f6bd196",
       "1bb36ad9197299f111b4ef21271c41f4b7ecf5543db8bb5931307e"
       "bdb2eaa465"},
  };

  for (const auto &t : kTests) {
    std::vector<uint8_t> u_bytes, expected_qx, expected_qy;
    ASSERT_TRUE(DecodeHex(&u_bytes, t.u_hex));
    ASSERT_TRUE(DecodeHex(&expected_qx, t.qx_hex));
    ASSERT_TRUE(DecodeHex(&expected_qy, t.qy_hex));
    ASSERT_EQ(u_bytes.size(), 32u);
    ASSERT_EQ(expected_qx.size(), 32u);
    ASSERT_EQ(expected_qy.size(), 32u);

    // RFC 9380 test vectors encode coordinates in big-endian hex, while
    // Curve25519 serializes field elements in little-endian.
    std::reverse(u_bytes.begin(), u_bytes.end());
    std::reverse(expected_qx.begin(), expected_qx.end());
    std::reverse(expected_qy.begin(), expected_qy.end());

    uint8_t qx[32], qy[32];
    map_to_curve_curve25519_elligator2(qx, qy, u_bytes.data());
    EXPECT_EQ(Bytes(qx), Bytes(expected_qx));
    EXPECT_EQ(Bytes(qy), Bytes(expected_qy));
  }
}

TEST(Curve25519Test, CPaceX25519GeneratorTest) {
  // Test vector from draft-irtf-cfrg-cpace-21, Appendix B.1.1
  const uint8_t u_bytes[32] = {0x03, 0x99, 0x80, 0x87, 0xbd, 0xb1, 0xa2, 0x61,
                               0x7b, 0xbe, 0x25, 0xef, 0x5a, 0x7c, 0x18, 0xcd,
                               0x4f, 0x84, 0xf9, 0x02, 0x32, 0x87, 0x01, 0x79,
                               0x09, 0x58, 0x75, 0x5e, 0xe4, 0xae, 0xd1, 0x53};
  const uint8_t expected_g[32] = {
      0xd0, 0x4b, 0xf6, 0xd4, 0x1f, 0x6a, 0x28, 0x96, 0x32, 0xa2, 0xe9,
      0x29, 0xfa, 0x29, 0xbe, 0xbd, 0x51, 0x09, 0x25, 0x12, 0xa7, 0x82,
      0x9f, 0xdd, 0xe7, 0xd3, 0x14, 0xb6, 0x2f, 0x05, 0xa7, 0x3f};

  uint8_t g[32], v[32];
  map_to_curve_curve25519_elligator2(g, v, u_bytes);
  EXPECT_EQ(Bytes(g), Bytes(expected_g));
}

#endif

}  // namespace
