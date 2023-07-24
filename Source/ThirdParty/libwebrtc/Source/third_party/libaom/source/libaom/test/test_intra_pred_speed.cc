/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

//  Test and time AOM intra-predictor functions

#include <stdio.h>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/md5_helper.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "av1/common/common_data.h"

// -----------------------------------------------------------------------------

namespace {

// Note:
// APPLY_UNIT_TESTS
// 1: Do unit tests
// 0: Generate MD5 array as required
#define APPLY_UNIT_TESTS 1

typedef void (*AvxPredFunc)(uint8_t *dst, ptrdiff_t y_stride,
                            const uint8_t *above, const uint8_t *left);

const int kBPS = 64;
const int kTotalPixels = kBPS * kBPS;
// 4 DC variants, V, H, PAETH, SMOOTH, SMOOTH_V, SMOOTH_H
const int kNumAv1IntraFuncs = 10;

#if APPLY_UNIT_TESTS
const char *kAv1IntraPredNames[kNumAv1IntraFuncs] = {
  "DC_PRED", "DC_LEFT_PRED", "DC_TOP_PRED", "DC_128_PRED",   "V_PRED",
  "H_PRED",  "PAETH_PRED",   "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED",
};
#endif  // APPLY_UNIT_TESTS

template <typename Pixel>
struct IntraPredTestMem {
  void Init(int block_width, int block_height, int bd) {
    ASSERT_LE(block_width, kBPS);
    ASSERT_LE(block_height, kBPS);
    // Note: for blocks having width <= 32 and height <= 32, we generate 32x32
    // random pixels as before to avoid having to recalculate all hashes again.
    const int block_size_upto_32 = (block_width <= 32) && (block_height <= 32);
    stride = block_size_upto_32 ? 32 : kBPS;
    num_pixels = stride * stride;
    libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
    above = above_mem + 16;
    const int mask = (1 << bd) - 1;
    for (int i = 0; i < num_pixels; ++i) ref_src[i] = rnd.Rand16() & mask;
    for (int i = 0; i < stride; ++i) left[i] = rnd.Rand16() & mask;
    for (int i = -1; i < stride; ++i) above[i] = rnd.Rand16() & mask;

    for (int i = stride; i < 2 * stride; ++i) {
      left[i] = rnd.Rand16() & mask;
      above[i] = rnd.Rand16() & mask;
    }
  }

  DECLARE_ALIGNED(16, Pixel, src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, ref_src[kTotalPixels]);
  DECLARE_ALIGNED(16, Pixel, left[2 * kBPS]);
  Pixel *above;
  int stride;
  int num_pixels;

 private:
  DECLARE_ALIGNED(16, Pixel, above_mem[2 * kBPS + 16]);
};

// -----------------------------------------------------------------------------
// Low Bittdepth

typedef IntraPredTestMem<uint8_t> Av1IntraPredTestMem;

static const char *const kTxSizeStrings[TX_SIZES_ALL] = {
  "4X4",  "8X8",  "16X16", "32X32", "64X64", "4X8",   "8X4",
  "8X16", "16X8", "16X32", "32X16", "32X64", "64X32", "4X16",
  "16X4", "8X32", "32X8",  "16X64", "64X16",
};

void CheckMd5Signature(TX_SIZE tx_size, bool is_hbd,
                       const char *const signatures[], const void *data,
                       size_t data_size, int elapsed_time, int idx) {
  const std::string hbd_str = is_hbd ? "Hbd " : "";
  const std::string name_str = hbd_str + "Intra" + kTxSizeStrings[tx_size];
  libaom_test::MD5 md5;
  md5.Add(reinterpret_cast<const uint8_t *>(data), data_size);
#if APPLY_UNIT_TESTS
  printf("Mode %s[%13s]: %5d ms     MD5: %s\n", name_str.c_str(),
         kAv1IntraPredNames[idx], elapsed_time, md5.Get());
  EXPECT_STREQ(signatures[idx], md5.Get());
#else
  (void)signatures;
  (void)elapsed_time;
  (void)idx;
  printf("\"%s\",\n", md5.Get());
#endif
}

void TestIntraPred(TX_SIZE tx_size, AvxPredFunc const *pred_funcs,
                   const char *const signatures[]) {
  const int block_width = tx_size_wide[tx_size];
  const int block_height = tx_size_high[tx_size];
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1IntraPredTestMem intra_pred_test_mem;
  intra_pred_test_mem.Init(block_width, block_height, 8);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == nullptr) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, intra_pred_test_mem.stride,
                    intra_pred_test_mem.above, intra_pred_test_mem.left);
    }
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(
        tx_size, false, signatures, intra_pred_test_mem.src,
        intra_pred_test_mem.num_pixels * sizeof(*intra_pred_test_mem.src),
        elapsed_time, k);
  }
}

static const char *const kSignatures[TX_SIZES_ALL][kNumAv1IntraFuncs] = {
  {
      // 4X4
      "e7ed7353c3383fff942e500e9bfe82fe",
      "2a4a26fcc6ce005eadc08354d196c8a9",
      "269d92eff86f315d9c38fe7640d85b15",
      "ae2960eea9f71ee3dabe08b282ec1773",
      "6c1abcc44e90148998b51acd11144e9c",
      "f7bb3186e1ef8a2b326037ff898cad8e",
      "59fc0e923a08cfac0a493fb38988e2bb",
      "9ff8bb37d9c830e6ab8ecb0c435d3c91",
      "de6937fca02354f2874dbc5dbec5d5b3",
      "723cf948137f7d8c7860d814e55ae67d",
  },
  {
      // 8X8
      "d8bbae5d6547cfc17e4f5f44c8730e88",
      "373bab6d931868d41a601d9d88ce9ac3",
      "6fdd5ff4ff79656c14747598ca9e3706",
      "d9661c2811d6a73674f40ffb2b841847",
      "7c722d10b19ccff0b8c171868e747385",
      "f81dd986eb2b50f750d3a7da716b7e27",
      "064404361748dd111a890a1470d7f0ea",
      "dc29b7e1f78cc8e7525d5ea4c0ab9b78",
      "97111eb1bc26bade6272015df829f1ae",
      "d19a8a73cc46b807f2c5e817576cc1e1",
  },
  {
      // 16X16
      "50971c07ce26977d30298538fffec619",
      "527a6b9e0dc5b21b98cf276305432bef",
      "7eff2868f80ebc2c43a4f367281d80f7",
      "67cd60512b54964ef6aff1bd4816d922",
      "48371c87dc95c08a33b2048f89cf6468",
      "b0acf2872ee411d7530af6d2625a7084",
      "93d6b5352b571805ab16a55e1bbed86a",
      "03764e4c0aebbc180e4e2c68fb06df2b",
      "bb6c74c9076c9f266ab11fb57060d8e6",
      "0c5162bc28489756ddb847b5678e6f07",
  },
  {
      // 32X32
      "a0a618c900e65ae521ccc8af789729f2",
      "985aaa7c72b4a6c2fb431d32100cf13a",
      "10662d09febc3ca13ee4e700120daeb5",
      "b3b01379ba08916ef6b1b35f7d9ad51c",
      "9f4261755795af97e34679c333ec7004",
      "bc2c9da91ad97ef0d1610fb0a9041657",
      "ef1653982b69e1f64bee3759f3e1ec45",
      "1a51a675deba2c83282142eb48d3dc3d",
      "866c224746dc260cda861a7b1b383fb3",
      "cea23799fc3526e1b6a6ff02b42b82af",
  },
  {
      // 64X64
      "6e1094fa7b50bc813aa2ba29f5df8755",
      "afe020786b83b793c2bbd9468097ff6e",
      "be91585259bc37bf4dc1651936e90b3e",
      "a1650dbcd56e10288c3e269eca37967d",
      "9e5c34f3797e0cdd3cd9d4c05b0d8950",
      "bc87be7ac899cc6a28f399d7516c49fe",
      "9811fd0d2dd515f06122f5d1bd18b784",
      "3c140e466f2c2c0d9cb7d2157ab8dc27",
      "9543de76c925a8f6adc884cc7f98dc91",
      "df1df0376cc944afe7e74e94f53e575a",
  },
  {
      // 4X8
      "d9fbebdc85f71ab1e18461b2db4a2adc",
      "5ccb2a68284bc9714d94b8a06ccadbb2",
      "735d059abc2744f3ff3f9590f7191b37",
      "d9fbebdc85f71ab1e18461b2db4a2adc",
      "6819497c44cd0ace120add83672996ee",
      "7e3244f5a2d3edf81c7e962a842b97f9",
      "809350f164cd4d1650850bb0f59c3260",
      "1b60a394331eeab6927a6f8aaff57040",
      "5307de1bd7329ba6b281d2c1b0b457f9",
      "24c58a8138339846d95568efb91751db",
  },
  {
      // 8X4
      "23f9fc11344426c9bee2e06d57dfd628",
      "2d71a26d1bae1fb34734de7b42fc5eb7",
      "5af9c1b2fd9d5721fad67b67b3f7c816",
      "00d71b17be662753813d515f197d145e",
      "bef10ec984427e28f4390f43809d10af",
      "77773cdfb7ed6bc882ab202a64b0a470",
      "2cc48bd66d6b0121b5221d52ccd732af",
      "b302155e1c9eeeafe2ba2bf68e807a46",
      "561bc8d0e76d5041ebd5168fc6a115e1",
      "81d0113fb1d0a9a24ffd6f1987b77948",
  },
  {
      // 8X16
      "c849de88b24f773dfcdd1d48d1209796",
      "6cb807c1897b94866a0f3d3c56ed8695",
      "d56db05a8ac7981762f5b877f486c4ef",
      "b4bc01eb6e59a40922ad17715cafb04b",
      "09d178439534f4062ae687c351f66d64",
      "644501399cf73080ac606e5cef7ca09b",
      "278076495180e17c065a95ab7278539a",
      "9dd7f324816f242be408ffeb0c673732",
      "f520c4a20acfa0bea1d253c6f0f040fd",
      "85f38df809df2c2d7c8b4a157a65cd44",
  },
  {
      // 16X8
      "b4cbdbdf10ce13300b4063a3daf99e04",
      "3731e1e6202064a9d0604d7c293ecee4",
      "6c856188c4256a06452f0d5d70cac436",
      "1f2192b4c8c497589484ea7bf9c944e8",
      "84011bd4b7f565119d06787840e333a0",
      "0e48949f7a6aa36f0d76b5d01f91124a",
      "60eff8064634b6c73b10681356baeee9",
      "1559aeb081a9c0c71111d6093c2ff9fd",
      "c15479b739713773e5cabb748451987b",
      "72e33ec12c9b67aea26d8d005fb82de2",
  },
  {
      // 16X32
      "abe5233d189cdbf79424721571bbaa7b",
      "282759f81e3cfb2e2d396fe406b72a8b",
      "e2224926c264f6f174cbc3167a233168",
      "6814e85c2b33f8c9415d62e80394b47b",
      "99cbbb60459c08a3061d72c4e4f6276a",
      "1d1567d40b8e816f8c1f71e576fe0f87",
      "36fdd371b624a075814d497c4832ec85",
      "8ab8da61b727442b6ff692b40d0df018",
      "e35a10ad7fdf2327e821504a90f6a6eb",
      "1f7211e727dc1de7d6a55d082fbdd821",
  },
  {
      // 32X16
      "d1aeb8d5fdcfd3307922af01a798a4dc",
      "b0bcb514ebfbee065faea9d34c12ae75",
      "d6a18c63b4e909871c0137ca652fad23",
      "fd047f2fc1b8ffb95d0eeef3e8796a45",
      "645ab60779ea348fd93c81561c31bab9",
      "4409633c9db8dff41ade4292a3a56e7f",
      "5e36a11e069b31c2a739f3a9c7b37c24",
      "e83b9483d702cfae496991c3c7fa92c0",
      "12f6ddf98c7f30a277307f1ea935b030",
      "354321d6c32bbdb0739e4fa2acbf41e1",
  },
  {
      // 32X64
      "0ce332b343934b34cd4417725faa85cb",
      "4e2a2cfd8f56f15939bdfc753145b303",
      "0f46d124ba9f48cdd5d5290acf786d6d",
      "e1e8ed803236367821981500a3d9eebe",
      "1d2f8e48e3adb7c448be05d9f66f4954",
      "9fb2e176636a5689b26f73ca73fcc512",
      "e720ebccae7e25e36f23da53ae5b5d6a",
      "86fe4364734169aaa4520d799890d530",
      "b1870290764bb1b100d1974e2bd70f1d",
      "ce5b238e19d85ef69d85badfab4e63ae",
  },
  {
      // 64X32
      "a6c5aeb722615089efbca80b02951ceb",
      "538424b24bd0830f21788e7238ca762f",
      "80c15b303235f9bc2259027bb92dfdc4",
      "e48e1ac15e97191a8fda08d62fff343e",
      "12604b37875533665078405ef4582e35",
      "0048afa17bd3e1632d68b96048836530",
      "07a0cfcb56a5eed50c4bd6c26814336b",
      "529d8a070de5bc6531fa3ee8f450c233",
      "33c50a11c7d78f72434064f634305e95",
      "e0ef7f0559c1a50ec5a8c12011b962f7",
  },
  {
      // 4X16
      "750491056568eb8fe15387b86bdf06b8",
      "3a52dae9f599f08cfb3bd1b910dc0e11",
      "af79f71e3e03dbeca44e2e13561f70c7",
      "ca7dfd7624afc0c06fb5552f44398535",
      "b591af115444bf43140c29c269f68fb2",
      "483d942ae36e69e62f31eb215331416f",
      "f14b58525e81870bc5d95c7ac71a347f",
      "371208bb4027d9badb04095d1590bbc4",
      "c7049c21b2924d70c7c12784d6b6b796",
      "7d87233f4b5b0f12086045e5d7b2d4c2",
  },
  {
      // 16X4
      "7c6e325a65e77e732b3adbe237e045e4",
      "24478f93ffcec47852e004d0fe948464",
      "258d042c67d4ba3ecfa667f0adc9aebf",
      "b2cd21d06959f159a1f3c4d9768ee7fb",
      "b4e1f38157bf8410e7c3da02f687a343",
      "869e703729eb0fc0711c254944ff5d5a",
      "9638dd77105a640b146a8201ea7a0801",
      "919d932c6af8a1cc7486e8ce996dd487",
      "e1c9be493b6714c7ae48f30044c43140",
      "bf0fe3889d654b2f6eb98c8fc751f9e4",
  },
  {
      // 8X32
      "8dfac4319fe0bd40013ffb3102da8c72",
      "feb46b6dc4e2ca0a09533bfc51d4dcb0",
      "850837ec714c37262216527aaf4cbbe9",
      "4603c7800fb08361f163daca876e8bda",
      "1ff95e7d2debc27b05806fb25abfd624",
      "d81b9a51a062b23ca7823804cb7bec22",
      "f1d8978158766f46335203608cb807e7",
      "f3527096256258c0878d644a9d7d53ca",
      "cbde98ac8b009953eb112807ad2ea29e",
      "654fb1153415747feae599f538122af5",
  },
  {
      // 32X8
      "3d4ee16fab374357474f60b845327bc7",
      "bc17c5059473a476df4e85f56395ad55",
      "3d4ee16fab374357474f60b845327bc7",
      "c14b8db34dc2355b84e3735c9ba16c7f",
      "a71d25b5d47a92a8b9223c98f18458ee",
      "6c1cfe2b1893f4576a80675687cb6426",
      "92d11bbef8b85bb48d799bb055de3514",
      "bcf81d1db8ae5cc03360467f44f498ec",
      "79f8c564163555592e808e145eaf5c60",
      "46fff139cef2ef773938bcc8b0e5abb8",
  },
  {
      // 16X64
      "3b2a053ee8b05a8ac35ad23b0422a151",
      "12b0c69595328c465e0b25e0c9e3e9fc",
      "f77c544ac8035e01920deae40cee7b07",
      "727797ef15ccd8d325476fe8f12006a3",
      "f3be77c0fe67eb5d9d515e92bec21eb7",
      "f1ece6409e01e9dd98b800d49628247d",
      "efd2ec9bfbbd4fd1f6604ea369df1894",
      "ec703de918422b9e03197ba0ed60a199",
      "739418efb89c07f700895deaa5d0b3e3",
      "9943ae1bbeeebfe1d3a92dc39e049d63",
  },
  {
      // 64X16
      "821b76b1494d4f84d20817840f719a1a",
      "69e462c3338a9aaf993c3f7cfbc15649",
      "516d8f6eb054d74d150e7b444185b6b9",
      "de1b736e9d99129609d6ef3a491507a0",
      "fd9b4276e7affe1e0e4ce4f428058994",
      "cd82fd361a4767ac29a9f406b480b8f3",
      "2792c2f810157a4a6cb13c28529ff779",
      "1220442d90c4255ba0969d28b91e93a6",
      "c7253e10b45f7f67dfee3256c9b94825",
      "879792198071c7e0b50b9b5010d8c18f",
  },
};

}  // namespace

// Defines a test case for |arch| (e.g., C, SSE2, ...) passing the predictors
// to TestIntraPred. The test name is 'arch.TestIntraPred_tx_size', e.g.,
// C.TestIntraPred.0
#define INTRA_PRED_TEST(arch, tx_size, dc, dc_left, dc_top, dc_128, v, h,  \
                        paeth, smooth, smooth_v, smooth_h)                 \
  TEST(arch, DISABLED_##TestIntraPred_##tx_size) {                         \
    static const AvxPredFunc aom_intra_pred[] = {                          \
      dc, dc_left, dc_top, dc_128, v, h, paeth, smooth, smooth_v, smooth_h \
    };                                                                     \
    TestIntraPred(tx_size, aom_intra_pred, kSignatures[tx_size]);          \
  }

// -----------------------------------------------------------------------------
// 4x4, 4x8, 4x16

INTRA_PRED_TEST(C, TX_4X4, aom_dc_predictor_4x4_c, aom_dc_left_predictor_4x4_c,
                aom_dc_top_predictor_4x4_c, aom_dc_128_predictor_4x4_c,
                aom_v_predictor_4x4_c, aom_h_predictor_4x4_c,
                aom_paeth_predictor_4x4_c, aom_smooth_predictor_4x4_c,
                aom_smooth_v_predictor_4x4_c, aom_smooth_h_predictor_4x4_c)
INTRA_PRED_TEST(C, TX_4X8, aom_dc_predictor_4x8_c, aom_dc_left_predictor_4x8_c,
                aom_dc_top_predictor_4x8_c, aom_dc_128_predictor_4x8_c,
                aom_v_predictor_4x8_c, aom_h_predictor_4x8_c,
                aom_paeth_predictor_4x8_c, aom_smooth_predictor_4x8_c,
                aom_smooth_v_predictor_4x8_c, aom_smooth_h_predictor_4x8_c)
INTRA_PRED_TEST(C, TX_4X16, aom_dc_predictor_4x16_c,
                aom_dc_left_predictor_4x16_c, aom_dc_top_predictor_4x16_c,
                aom_dc_128_predictor_4x16_c, aom_v_predictor_4x16_c,
                aom_h_predictor_4x16_c, aom_paeth_predictor_4x16_c,
                aom_smooth_predictor_4x16_c, aom_smooth_v_predictor_4x16_c,
                aom_smooth_h_predictor_4x16_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TX_4X4, aom_dc_predictor_4x4_sse2,
                aom_dc_left_predictor_4x4_sse2, aom_dc_top_predictor_4x4_sse2,
                aom_dc_128_predictor_4x4_sse2, aom_v_predictor_4x4_sse2,
                aom_h_predictor_4x4_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_4X8, aom_dc_predictor_4x8_sse2,
                aom_dc_left_predictor_4x8_sse2, aom_dc_top_predictor_4x8_sse2,
                aom_dc_128_predictor_4x8_sse2, aom_v_predictor_4x8_sse2,
                aom_h_predictor_4x8_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_4X16, aom_dc_predictor_4x16_sse2,
                aom_dc_left_predictor_4x16_sse2, aom_dc_top_predictor_4x16_sse2,
                aom_dc_128_predictor_4x16_sse2, aom_v_predictor_4x16_sse2,
                aom_h_predictor_4x16_sse2, nullptr, nullptr, nullptr, nullptr)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TX_4X4, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_4x4_ssse3,
                aom_smooth_predictor_4x4_ssse3,
                aom_smooth_v_predictor_4x4_ssse3,
                aom_smooth_h_predictor_4x4_ssse3)
INTRA_PRED_TEST(SSSE3, TX_4X8, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_4x8_ssse3,
                aom_smooth_predictor_4x8_ssse3,
                aom_smooth_v_predictor_4x8_ssse3,
                aom_smooth_h_predictor_4x8_ssse3)
INTRA_PRED_TEST(SSSE3, TX_4X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_4x16_ssse3,
                aom_smooth_predictor_4x16_ssse3,
                aom_smooth_v_predictor_4x16_ssse3,
                aom_smooth_h_predictor_4x16_ssse3)
#endif  // HAVE_SSSE3

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TX_4X4, aom_dc_predictor_4x4_neon,
                aom_dc_left_predictor_4x4_neon, aom_dc_top_predictor_4x4_neon,
                aom_dc_128_predictor_4x4_neon, aom_v_predictor_4x4_neon,
                aom_h_predictor_4x4_neon, aom_paeth_predictor_4x4_neon,
                aom_smooth_predictor_4x4_neon, aom_smooth_v_predictor_4x4_neon,
                aom_smooth_h_predictor_4x4_neon)
INTRA_PRED_TEST(NEON, TX_4X8, aom_dc_predictor_4x8_neon,
                aom_dc_left_predictor_4x8_neon, aom_dc_top_predictor_4x8_neon,
                aom_dc_128_predictor_4x8_neon, aom_v_predictor_4x8_neon,
                aom_h_predictor_4x8_neon, aom_paeth_predictor_4x8_neon,
                aom_smooth_predictor_4x8_neon, aom_smooth_v_predictor_4x8_neon,
                aom_smooth_h_predictor_4x8_neon)
INTRA_PRED_TEST(NEON, TX_4X16, aom_dc_predictor_4x16_neon,
                aom_dc_left_predictor_4x16_neon, aom_dc_top_predictor_4x16_neon,
                aom_dc_128_predictor_4x16_neon, aom_v_predictor_4x16_neon,
                aom_h_predictor_4x16_neon, aom_paeth_predictor_4x16_neon,
                aom_smooth_predictor_4x16_neon,
                aom_smooth_v_predictor_4x16_neon,
                aom_smooth_h_predictor_4x16_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 8x8, 8x4, 8x16, 8x32

INTRA_PRED_TEST(C, TX_8X8, aom_dc_predictor_8x8_c, aom_dc_left_predictor_8x8_c,
                aom_dc_top_predictor_8x8_c, aom_dc_128_predictor_8x8_c,
                aom_v_predictor_8x8_c, aom_h_predictor_8x8_c,
                aom_paeth_predictor_8x8_c, aom_smooth_predictor_8x8_c,
                aom_smooth_v_predictor_8x8_c, aom_smooth_h_predictor_8x8_c)

INTRA_PRED_TEST(C, TX_8X4, aom_dc_predictor_8x4_c, aom_dc_left_predictor_8x4_c,
                aom_dc_top_predictor_8x4_c, aom_dc_128_predictor_8x4_c,
                aom_v_predictor_8x4_c, aom_h_predictor_8x4_c,
                aom_paeth_predictor_8x4_c, aom_smooth_predictor_8x4_c,
                aom_smooth_v_predictor_8x4_c, aom_smooth_h_predictor_8x4_c)
INTRA_PRED_TEST(C, TX_8X16, aom_dc_predictor_8x16_c,
                aom_dc_left_predictor_8x16_c, aom_dc_top_predictor_8x16_c,
                aom_dc_128_predictor_8x16_c, aom_v_predictor_8x16_c,
                aom_h_predictor_8x16_c, aom_paeth_predictor_8x16_c,
                aom_smooth_predictor_8x16_c, aom_smooth_v_predictor_8x16_c,
                aom_smooth_h_predictor_8x16_c)
INTRA_PRED_TEST(C, TX_8X32, aom_dc_predictor_8x32_c,
                aom_dc_left_predictor_8x32_c, aom_dc_top_predictor_8x32_c,
                aom_dc_128_predictor_8x32_c, aom_v_predictor_8x32_c,
                aom_h_predictor_8x32_c, aom_paeth_predictor_8x32_c,
                aom_smooth_predictor_8x32_c, aom_smooth_v_predictor_8x32_c,
                aom_smooth_h_predictor_8x32_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TX_8X8, aom_dc_predictor_8x8_sse2,
                aom_dc_left_predictor_8x8_sse2, aom_dc_top_predictor_8x8_sse2,
                aom_dc_128_predictor_8x8_sse2, aom_v_predictor_8x8_sse2,
                aom_h_predictor_8x8_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_8X4, aom_dc_predictor_8x4_sse2,
                aom_dc_left_predictor_8x4_sse2, aom_dc_top_predictor_8x4_sse2,
                aom_dc_128_predictor_8x4_sse2, aom_v_predictor_8x4_sse2,
                aom_h_predictor_8x4_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_8X16, aom_dc_predictor_8x16_sse2,
                aom_dc_left_predictor_8x16_sse2, aom_dc_top_predictor_8x16_sse2,
                aom_dc_128_predictor_8x16_sse2, aom_v_predictor_8x16_sse2,
                aom_h_predictor_8x16_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_8X32, aom_dc_predictor_8x32_sse2,
                aom_dc_left_predictor_8x32_sse2, aom_dc_top_predictor_8x32_sse2,
                aom_dc_128_predictor_8x32_sse2, aom_v_predictor_8x32_sse2,
                aom_h_predictor_8x32_sse2, nullptr, nullptr, nullptr, nullptr)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TX_8X8, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_8x8_ssse3,
                aom_smooth_predictor_8x8_ssse3,
                aom_smooth_v_predictor_8x8_ssse3,
                aom_smooth_h_predictor_8x8_ssse3)
INTRA_PRED_TEST(SSSE3, TX_8X4, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_8x4_ssse3,
                aom_smooth_predictor_8x4_ssse3,
                aom_smooth_v_predictor_8x4_ssse3,
                aom_smooth_h_predictor_8x4_ssse3)
INTRA_PRED_TEST(SSSE3, TX_8X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_8x16_ssse3,
                aom_smooth_predictor_8x16_ssse3,
                aom_smooth_v_predictor_8x16_ssse3,
                aom_smooth_h_predictor_8x16_ssse3)
INTRA_PRED_TEST(SSSE3, TX_8X32, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_8x32_ssse3,
                aom_smooth_predictor_8x32_ssse3,
                aom_smooth_v_predictor_8x32_ssse3,
                aom_smooth_h_predictor_8x32_ssse3)
#endif  // HAVE_SSSE3

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TX_8X8, aom_dc_predictor_8x8_neon,
                aom_dc_left_predictor_8x8_neon, aom_dc_top_predictor_8x8_neon,
                aom_dc_128_predictor_8x8_neon, aom_v_predictor_8x8_neon,
                aom_h_predictor_8x8_neon, aom_paeth_predictor_8x8_neon,
                aom_smooth_predictor_8x8_neon, aom_smooth_v_predictor_8x8_neon,
                aom_smooth_h_predictor_8x8_neon)
INTRA_PRED_TEST(NEON, TX_8X4, aom_dc_predictor_8x4_neon,
                aom_dc_left_predictor_8x4_neon, aom_dc_top_predictor_8x4_neon,
                aom_dc_128_predictor_8x4_neon, aom_v_predictor_8x4_neon,
                aom_h_predictor_8x4_neon, aom_paeth_predictor_8x4_neon,
                aom_smooth_predictor_8x4_neon, aom_smooth_v_predictor_8x4_neon,
                aom_smooth_h_predictor_8x4_neon)
INTRA_PRED_TEST(NEON, TX_8X16, aom_dc_predictor_8x16_neon,
                aom_dc_left_predictor_8x16_neon, aom_dc_top_predictor_8x16_neon,
                aom_dc_128_predictor_8x16_neon, aom_v_predictor_8x16_neon,
                aom_h_predictor_8x16_neon, aom_paeth_predictor_8x16_neon,
                aom_smooth_predictor_8x16_neon,
                aom_smooth_v_predictor_8x16_neon,
                aom_smooth_h_predictor_8x16_neon)
INTRA_PRED_TEST(NEON, TX_8X32, aom_dc_predictor_8x32_neon,
                aom_dc_left_predictor_8x32_neon, aom_dc_top_predictor_8x32_neon,
                aom_dc_128_predictor_8x32_neon, aom_v_predictor_8x32_neon,
                aom_h_predictor_8x32_neon, aom_paeth_predictor_8x32_neon,
                aom_smooth_predictor_8x32_neon,
                aom_smooth_v_predictor_8x32_neon,
                aom_smooth_h_predictor_8x32_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 16x16, 16x8, 16x32, 16x4, 16x64

INTRA_PRED_TEST(C, TX_16X16, aom_dc_predictor_16x16_c,
                aom_dc_left_predictor_16x16_c, aom_dc_top_predictor_16x16_c,
                aom_dc_128_predictor_16x16_c, aom_v_predictor_16x16_c,
                aom_h_predictor_16x16_c, aom_paeth_predictor_16x16_c,
                aom_smooth_predictor_16x16_c, aom_smooth_v_predictor_16x16_c,
                aom_smooth_h_predictor_16x16_c)
INTRA_PRED_TEST(C, TX_16X8, aom_dc_predictor_16x8_c,
                aom_dc_left_predictor_16x8_c, aom_dc_top_predictor_16x8_c,
                aom_dc_128_predictor_16x8_c, aom_v_predictor_16x8_c,
                aom_h_predictor_16x8_c, aom_paeth_predictor_16x8_c,
                aom_smooth_predictor_16x8_c, aom_smooth_v_predictor_16x8_c,
                aom_smooth_h_predictor_16x8_c)
INTRA_PRED_TEST(C, TX_16X32, aom_dc_predictor_16x32_c,
                aom_dc_left_predictor_16x32_c, aom_dc_top_predictor_16x32_c,
                aom_dc_128_predictor_16x32_c, aom_v_predictor_16x32_c,
                aom_h_predictor_16x32_c, aom_paeth_predictor_16x32_c,
                aom_smooth_predictor_16x32_c, aom_smooth_v_predictor_16x32_c,
                aom_smooth_h_predictor_16x32_c)
INTRA_PRED_TEST(C, TX_16X4, aom_dc_predictor_16x4_c,
                aom_dc_left_predictor_16x4_c, aom_dc_top_predictor_16x4_c,
                aom_dc_128_predictor_16x4_c, aom_v_predictor_16x4_c,
                aom_h_predictor_16x4_c, aom_paeth_predictor_16x4_c,
                aom_smooth_predictor_16x4_c, aom_smooth_v_predictor_16x4_c,
                aom_smooth_h_predictor_16x4_c)
INTRA_PRED_TEST(C, TX_16X64, aom_dc_predictor_16x64_c,
                aom_dc_left_predictor_16x64_c, aom_dc_top_predictor_16x64_c,
                aom_dc_128_predictor_16x64_c, aom_v_predictor_16x64_c,
                aom_h_predictor_16x64_c, aom_paeth_predictor_16x64_c,
                aom_smooth_predictor_16x64_c, aom_smooth_v_predictor_16x64_c,
                aom_smooth_h_predictor_16x64_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TX_16X16, aom_dc_predictor_16x16_sse2,
                aom_dc_left_predictor_16x16_sse2,
                aom_dc_top_predictor_16x16_sse2,
                aom_dc_128_predictor_16x16_sse2, aom_v_predictor_16x16_sse2,
                aom_h_predictor_16x16_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_16X8, aom_dc_predictor_16x8_sse2,
                aom_dc_left_predictor_16x8_sse2, aom_dc_top_predictor_16x8_sse2,
                aom_dc_128_predictor_16x8_sse2, aom_v_predictor_16x8_sse2,
                aom_h_predictor_16x8_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_16X32, aom_dc_predictor_16x32_sse2,
                aom_dc_left_predictor_16x32_sse2,
                aom_dc_top_predictor_16x32_sse2,
                aom_dc_128_predictor_16x32_sse2, aom_v_predictor_16x32_sse2,
                aom_h_predictor_16x32_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_16X64, aom_dc_predictor_16x64_sse2,
                aom_dc_left_predictor_16x64_sse2,
                aom_dc_top_predictor_16x64_sse2,
                aom_dc_128_predictor_16x64_sse2, aom_v_predictor_16x64_sse2,
                aom_h_predictor_16x64_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_16X4, aom_dc_predictor_16x4_sse2,
                aom_dc_left_predictor_16x4_sse2, aom_dc_top_predictor_16x4_sse2,
                aom_dc_128_predictor_16x4_sse2, aom_v_predictor_16x4_sse2,
                aom_h_predictor_16x4_sse2, nullptr, nullptr, nullptr, nullptr)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TX_16X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x16_ssse3,
                aom_smooth_predictor_16x16_ssse3,
                aom_smooth_v_predictor_16x16_ssse3,
                aom_smooth_h_predictor_16x16_ssse3)
INTRA_PRED_TEST(SSSE3, TX_16X8, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x8_ssse3,
                aom_smooth_predictor_16x8_ssse3,
                aom_smooth_v_predictor_16x8_ssse3,
                aom_smooth_h_predictor_16x8_ssse3)
INTRA_PRED_TEST(SSSE3, TX_16X32, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x32_ssse3,
                aom_smooth_predictor_16x32_ssse3,
                aom_smooth_v_predictor_16x32_ssse3,
                aom_smooth_h_predictor_16x32_ssse3)
INTRA_PRED_TEST(SSSE3, TX_16X64, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x64_ssse3,
                aom_smooth_predictor_16x64_ssse3,
                aom_smooth_v_predictor_16x64_ssse3,
                aom_smooth_h_predictor_16x64_ssse3)
INTRA_PRED_TEST(SSSE3, TX_16X4, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x4_ssse3,
                aom_smooth_predictor_16x4_ssse3,
                aom_smooth_v_predictor_16x4_ssse3,
                aom_smooth_h_predictor_16x4_ssse3)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2, TX_16X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x16_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_16X8, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x8_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_16X32, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x32_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_16X64, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_16x64_avx2, nullptr, nullptr,
                nullptr)
#endif  // HAVE_AVX2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TX_16X16, aom_dc_predictor_16x16_neon,
                aom_dc_left_predictor_16x16_neon,
                aom_dc_top_predictor_16x16_neon,
                aom_dc_128_predictor_16x16_neon, aom_v_predictor_16x16_neon,
                aom_h_predictor_16x16_neon, aom_paeth_predictor_16x16_neon,
                aom_smooth_predictor_16x16_neon,
                aom_smooth_v_predictor_16x16_neon,
                aom_smooth_h_predictor_16x16_neon)
INTRA_PRED_TEST(NEON, TX_16X8, aom_dc_predictor_16x8_neon,
                aom_dc_left_predictor_16x8_neon, aom_dc_top_predictor_16x8_neon,
                aom_dc_128_predictor_16x8_neon, aom_v_predictor_16x8_neon,
                aom_h_predictor_16x8_neon, aom_paeth_predictor_16x8_neon,
                aom_smooth_predictor_16x8_neon,
                aom_smooth_v_predictor_16x8_neon,
                aom_smooth_h_predictor_16x8_neon)
INTRA_PRED_TEST(NEON, TX_16X32, aom_dc_predictor_16x32_neon,
                aom_dc_left_predictor_16x32_neon,
                aom_dc_top_predictor_16x32_neon,
                aom_dc_128_predictor_16x32_neon, aom_v_predictor_16x32_neon,
                aom_h_predictor_16x32_neon, aom_paeth_predictor_16x32_neon,
                aom_smooth_predictor_16x32_neon,
                aom_smooth_v_predictor_16x32_neon,
                aom_smooth_h_predictor_16x32_neon)
INTRA_PRED_TEST(NEON, TX_16X4, aom_dc_predictor_16x4_neon,
                aom_dc_left_predictor_16x4_neon, aom_dc_top_predictor_16x4_neon,
                aom_dc_128_predictor_16x4_neon, aom_v_predictor_16x4_neon,
                aom_h_predictor_16x4_neon, aom_paeth_predictor_16x4_neon,
                aom_smooth_predictor_16x4_neon,
                aom_smooth_v_predictor_16x4_neon,
                aom_smooth_h_predictor_16x4_neon)
INTRA_PRED_TEST(NEON, TX_16X64, aom_dc_predictor_16x64_neon,
                aom_dc_left_predictor_16x64_neon,
                aom_dc_top_predictor_16x64_neon,
                aom_dc_128_predictor_16x64_neon, aom_v_predictor_16x64_neon,
                aom_h_predictor_16x64_neon, aom_paeth_predictor_16x64_neon,
                aom_smooth_predictor_16x64_neon,
                aom_smooth_v_predictor_16x64_neon,
                aom_smooth_h_predictor_16x64_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 32x32, 32x16, 32x64, 32x8

INTRA_PRED_TEST(C, TX_32X32, aom_dc_predictor_32x32_c,
                aom_dc_left_predictor_32x32_c, aom_dc_top_predictor_32x32_c,
                aom_dc_128_predictor_32x32_c, aom_v_predictor_32x32_c,
                aom_h_predictor_32x32_c, aom_paeth_predictor_32x32_c,
                aom_smooth_predictor_32x32_c, aom_smooth_v_predictor_32x32_c,
                aom_smooth_h_predictor_32x32_c)
INTRA_PRED_TEST(C, TX_32X16, aom_dc_predictor_32x16_c,
                aom_dc_left_predictor_32x16_c, aom_dc_top_predictor_32x16_c,
                aom_dc_128_predictor_32x16_c, aom_v_predictor_32x16_c,
                aom_h_predictor_32x16_c, aom_paeth_predictor_32x16_c,
                aom_smooth_predictor_32x16_c, aom_smooth_v_predictor_32x16_c,
                aom_smooth_h_predictor_32x16_c)
INTRA_PRED_TEST(C, TX_32X64, aom_dc_predictor_32x64_c,
                aom_dc_left_predictor_32x64_c, aom_dc_top_predictor_32x64_c,
                aom_dc_128_predictor_32x64_c, aom_v_predictor_32x64_c,
                aom_h_predictor_32x64_c, aom_paeth_predictor_32x64_c,
                aom_smooth_predictor_32x64_c, aom_smooth_v_predictor_32x64_c,
                aom_smooth_h_predictor_32x64_c)
INTRA_PRED_TEST(C, TX_32X8, aom_dc_predictor_32x8_c,
                aom_dc_left_predictor_32x8_c, aom_dc_top_predictor_32x8_c,
                aom_dc_128_predictor_32x8_c, aom_v_predictor_32x8_c,
                aom_h_predictor_32x8_c, aom_paeth_predictor_32x8_c,
                aom_smooth_predictor_32x8_c, aom_smooth_v_predictor_32x8_c,
                aom_smooth_h_predictor_32x8_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TX_32X32, aom_dc_predictor_32x32_sse2,
                aom_dc_left_predictor_32x32_sse2,
                aom_dc_top_predictor_32x32_sse2,
                aom_dc_128_predictor_32x32_sse2, aom_v_predictor_32x32_sse2,
                aom_h_predictor_32x32_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_32X16, aom_dc_predictor_32x16_sse2,
                aom_dc_left_predictor_32x16_sse2,
                aom_dc_top_predictor_32x16_sse2,
                aom_dc_128_predictor_32x16_sse2, aom_v_predictor_32x16_sse2,
                aom_h_predictor_32x16_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_32X64, aom_dc_predictor_32x64_sse2,
                aom_dc_left_predictor_32x64_sse2,
                aom_dc_top_predictor_32x64_sse2,
                aom_dc_128_predictor_32x64_sse2, aom_v_predictor_32x64_sse2,
                aom_h_predictor_32x64_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_32X8, aom_dc_predictor_32x8_sse2,
                aom_dc_left_predictor_32x8_sse2, aom_dc_top_predictor_32x8_sse2,
                aom_dc_128_predictor_32x8_sse2, aom_v_predictor_32x8_sse2,
                aom_h_predictor_32x8_sse2, nullptr, nullptr, nullptr, nullptr)
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TX_32X32, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_32x32_ssse3,
                aom_smooth_predictor_32x32_ssse3,
                aom_smooth_v_predictor_32x32_ssse3,
                aom_smooth_h_predictor_32x32_ssse3)
INTRA_PRED_TEST(SSSE3, TX_32X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_32x16_ssse3,
                aom_smooth_predictor_32x16_ssse3,
                aom_smooth_v_predictor_32x16_ssse3,
                aom_smooth_h_predictor_32x16_ssse3)
INTRA_PRED_TEST(SSSE3, TX_32X64, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_32x64_ssse3,
                aom_smooth_predictor_32x64_ssse3,
                aom_smooth_v_predictor_32x64_ssse3,
                aom_smooth_h_predictor_32x64_ssse3)
INTRA_PRED_TEST(SSSE3, TX_32X8, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_32x8_ssse3,
                aom_smooth_predictor_32x8_ssse3,
                aom_smooth_v_predictor_32x8_ssse3,
                aom_smooth_h_predictor_32x8_ssse3)
#endif  // HAVE_SSSE3

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2, TX_32X32, aom_dc_predictor_32x32_avx2,
                aom_dc_left_predictor_32x32_avx2,
                aom_dc_top_predictor_32x32_avx2,
                aom_dc_128_predictor_32x32_avx2, aom_v_predictor_32x32_avx2,
                aom_h_predictor_32x32_avx2, aom_paeth_predictor_32x32_avx2,
                nullptr, nullptr, nullptr)
INTRA_PRED_TEST(AVX2, TX_32X16, aom_dc_predictor_32x16_avx2,
                aom_dc_left_predictor_32x16_avx2,
                aom_dc_top_predictor_32x16_avx2,
                aom_dc_128_predictor_32x16_avx2, aom_v_predictor_32x16_avx2,
                nullptr, aom_paeth_predictor_32x16_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_32X64, aom_dc_predictor_32x64_avx2,
                aom_dc_left_predictor_32x64_avx2,
                aom_dc_top_predictor_32x64_avx2,
                aom_dc_128_predictor_32x64_avx2, aom_v_predictor_32x64_avx2,
                nullptr, aom_paeth_predictor_32x64_avx2, nullptr, nullptr,
                nullptr)
#endif  // HAVE_AVX2

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TX_32X32, aom_dc_predictor_32x32_neon,
                aom_dc_left_predictor_32x32_neon,
                aom_dc_top_predictor_32x32_neon,
                aom_dc_128_predictor_32x32_neon, aom_v_predictor_32x32_neon,
                aom_h_predictor_32x32_neon, aom_paeth_predictor_32x32_neon,
                aom_smooth_predictor_32x32_neon,
                aom_smooth_v_predictor_32x32_neon,
                aom_smooth_h_predictor_32x32_neon)
INTRA_PRED_TEST(NEON, TX_32X16, aom_dc_predictor_32x16_neon,
                aom_dc_left_predictor_32x16_neon,
                aom_dc_top_predictor_32x16_neon,
                aom_dc_128_predictor_32x16_neon, aom_v_predictor_32x16_neon,
                aom_h_predictor_32x16_neon, aom_paeth_predictor_32x16_neon,
                aom_smooth_predictor_32x16_neon,
                aom_smooth_v_predictor_32x16_neon,
                aom_smooth_h_predictor_32x16_neon)
INTRA_PRED_TEST(NEON, TX_32X64, aom_dc_predictor_32x64_neon,
                aom_dc_left_predictor_32x64_neon,
                aom_dc_top_predictor_32x64_neon,
                aom_dc_128_predictor_32x64_neon, aom_v_predictor_32x64_neon,
                aom_h_predictor_32x64_neon, aom_paeth_predictor_32x64_neon,
                aom_smooth_predictor_32x64_neon,
                aom_smooth_v_predictor_32x64_neon,
                aom_smooth_h_predictor_32x64_neon)
INTRA_PRED_TEST(NEON, TX_32X8, aom_dc_predictor_32x8_neon,
                aom_dc_left_predictor_32x8_neon, aom_dc_top_predictor_32x8_neon,
                aom_dc_128_predictor_32x8_neon, aom_v_predictor_32x8_neon,
                aom_h_predictor_32x8_neon, aom_paeth_predictor_32x8_neon,
                aom_smooth_predictor_32x8_neon,
                aom_smooth_v_predictor_32x8_neon,
                aom_smooth_h_predictor_32x8_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 64x64, 64x32, 64x16

INTRA_PRED_TEST(C, TX_64X64, aom_dc_predictor_64x64_c,
                aom_dc_left_predictor_64x64_c, aom_dc_top_predictor_64x64_c,
                aom_dc_128_predictor_64x64_c, aom_v_predictor_64x64_c,
                aom_h_predictor_64x64_c, aom_paeth_predictor_64x64_c,
                aom_smooth_predictor_64x64_c, aom_smooth_v_predictor_64x64_c,
                aom_smooth_h_predictor_64x64_c)
INTRA_PRED_TEST(C, TX_64X32, aom_dc_predictor_64x32_c,
                aom_dc_left_predictor_64x32_c, aom_dc_top_predictor_64x32_c,
                aom_dc_128_predictor_64x32_c, aom_v_predictor_64x32_c,
                aom_h_predictor_64x32_c, aom_paeth_predictor_64x32_c,
                aom_smooth_predictor_64x32_c, aom_smooth_v_predictor_64x32_c,
                aom_smooth_h_predictor_64x32_c)
INTRA_PRED_TEST(C, TX_64X16, aom_dc_predictor_64x16_c,
                aom_dc_left_predictor_64x16_c, aom_dc_top_predictor_64x16_c,
                aom_dc_128_predictor_64x16_c, aom_v_predictor_64x16_c,
                aom_h_predictor_64x16_c, aom_paeth_predictor_64x16_c,
                aom_smooth_predictor_64x16_c, aom_smooth_v_predictor_64x16_c,
                aom_smooth_h_predictor_64x16_c)

#if HAVE_SSE2
INTRA_PRED_TEST(SSE2, TX_64X64, aom_dc_predictor_64x64_sse2,
                aom_dc_left_predictor_64x64_sse2,
                aom_dc_top_predictor_64x64_sse2,
                aom_dc_128_predictor_64x64_sse2, aom_v_predictor_64x64_sse2,
                aom_h_predictor_64x64_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_64X32, aom_dc_predictor_64x32_sse2,
                aom_dc_left_predictor_64x32_sse2,
                aom_dc_top_predictor_64x32_sse2,
                aom_dc_128_predictor_64x32_sse2, aom_v_predictor_64x32_sse2,
                aom_h_predictor_64x32_sse2, nullptr, nullptr, nullptr, nullptr)
INTRA_PRED_TEST(SSE2, TX_64X16, aom_dc_predictor_64x16_sse2,
                aom_dc_left_predictor_64x16_sse2,
                aom_dc_top_predictor_64x16_sse2,
                aom_dc_128_predictor_64x16_sse2, aom_v_predictor_64x16_sse2,
                aom_h_predictor_64x16_sse2, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_SSSE3
INTRA_PRED_TEST(SSSE3, TX_64X64, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_64x64_ssse3,
                aom_smooth_predictor_64x64_ssse3,
                aom_smooth_v_predictor_64x64_ssse3,
                aom_smooth_h_predictor_64x64_ssse3)
INTRA_PRED_TEST(SSSE3, TX_64X32, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_64x32_ssse3,
                aom_smooth_predictor_64x32_ssse3,
                aom_smooth_v_predictor_64x32_ssse3,
                aom_smooth_h_predictor_64x32_ssse3)
INTRA_PRED_TEST(SSSE3, TX_64X16, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, aom_paeth_predictor_64x16_ssse3,
                aom_smooth_predictor_64x16_ssse3,
                aom_smooth_v_predictor_64x16_ssse3,
                aom_smooth_h_predictor_64x16_ssse3)
#endif

#if HAVE_AVX2
INTRA_PRED_TEST(AVX2, TX_64X64, aom_dc_predictor_64x64_avx2,
                aom_dc_left_predictor_64x64_avx2,
                aom_dc_top_predictor_64x64_avx2,
                aom_dc_128_predictor_64x64_avx2, aom_v_predictor_64x64_avx2,
                nullptr, aom_paeth_predictor_64x64_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_64X32, aom_dc_predictor_64x32_avx2,
                aom_dc_left_predictor_64x32_avx2,
                aom_dc_top_predictor_64x32_avx2,
                aom_dc_128_predictor_64x32_avx2, aom_v_predictor_64x32_avx2,
                nullptr, aom_paeth_predictor_64x32_avx2, nullptr, nullptr,
                nullptr)
INTRA_PRED_TEST(AVX2, TX_64X16, aom_dc_predictor_64x16_avx2,
                aom_dc_left_predictor_64x16_avx2,
                aom_dc_top_predictor_64x16_avx2,
                aom_dc_128_predictor_64x16_avx2, aom_v_predictor_64x16_avx2,
                nullptr, aom_paeth_predictor_64x16_avx2, nullptr, nullptr,
                nullptr)
#endif

#if HAVE_NEON
INTRA_PRED_TEST(NEON, TX_64X64, aom_dc_predictor_64x64_neon,
                aom_dc_left_predictor_64x64_neon,
                aom_dc_top_predictor_64x64_neon,
                aom_dc_128_predictor_64x64_neon, aom_v_predictor_64x64_neon,
                aom_h_predictor_64x64_neon, aom_paeth_predictor_64x64_neon,
                aom_smooth_predictor_64x64_neon,
                aom_smooth_v_predictor_64x64_neon,
                aom_smooth_h_predictor_64x64_neon)
INTRA_PRED_TEST(NEON, TX_64X32, aom_dc_predictor_64x32_neon,
                aom_dc_left_predictor_64x32_neon,
                aom_dc_top_predictor_64x32_neon,
                aom_dc_128_predictor_64x32_neon, aom_v_predictor_64x32_neon,
                aom_h_predictor_64x32_neon, aom_paeth_predictor_64x32_neon,
                aom_smooth_predictor_64x32_neon,
                aom_smooth_v_predictor_64x32_neon,
                aom_smooth_h_predictor_64x32_neon)
INTRA_PRED_TEST(NEON, TX_64X16, aom_dc_predictor_64x16_neon,
                aom_dc_left_predictor_64x16_neon,
                aom_dc_top_predictor_64x16_neon,
                aom_dc_128_predictor_64x16_neon, aom_v_predictor_64x16_neon,
                aom_h_predictor_64x16_neon, aom_paeth_predictor_64x16_neon,
                aom_smooth_predictor_64x16_neon,
                aom_smooth_v_predictor_64x16_neon,
                aom_smooth_h_predictor_64x16_neon)
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH
// -----------------------------------------------------------------------------
// High Bitdepth
namespace {

typedef void (*AvxHighbdPredFunc)(uint16_t *dst, ptrdiff_t y_stride,
                                  const uint16_t *above, const uint16_t *left,
                                  int bd);

typedef IntraPredTestMem<uint16_t> Av1HighbdIntraPredTestMem;

void TestHighbdIntraPred(TX_SIZE tx_size, AvxHighbdPredFunc const *pred_funcs,
                         const char *const signatures[]) {
  const int block_width = tx_size_wide[tx_size];
  const int block_height = tx_size_high[tx_size];
  const int num_pixels_per_test =
      block_width * block_height * kNumAv1IntraFuncs;
  const int kNumTests = static_cast<int>(2.e10 / num_pixels_per_test);
  Av1HighbdIntraPredTestMem intra_pred_test_mem;
  const int bd = 12;
  intra_pred_test_mem.Init(block_width, block_height, bd);

  for (int k = 0; k < kNumAv1IntraFuncs; ++k) {
    if (pred_funcs[k] == nullptr) continue;
    memcpy(intra_pred_test_mem.src, intra_pred_test_mem.ref_src,
           sizeof(intra_pred_test_mem.src));
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int num_tests = 0; num_tests < kNumTests; ++num_tests) {
      pred_funcs[k](intra_pred_test_mem.src, intra_pred_test_mem.stride,
                    intra_pred_test_mem.above, intra_pred_test_mem.left, bd);
    }
    aom_usec_timer_mark(&timer);
    const int elapsed_time =
        static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
    CheckMd5Signature(
        tx_size, true, signatures, intra_pred_test_mem.src,
        intra_pred_test_mem.num_pixels * sizeof(*intra_pred_test_mem.src),
        elapsed_time, k);
  }
}

static const char *const kHighbdSignatures[TX_SIZES_ALL][kNumAv1IntraFuncs] = {
  {
      // 4X4
      "11f74af6c5737df472f3275cbde062fa",
      "51bea056b6447c93f6eb8f6b7e8f6f71",
      "27e97f946766331795886f4de04c5594",
      "53ab15974b049111fb596c5168ec7e3f",
      "f0b640bb176fbe4584cf3d32a9b0320a",
      "729783ca909e03afd4b47111c80d967b",
      "6e30009c45474a22032678b1bd579c8f",
      "e57cba016d808aa8a35619df2a65f049",
      "55a6c37f39afcbbf5abca4a985b96459",
      "a623d45b37dafec1f8a75c4c5218913d",
  },
  {
      // 8X8
      "03da8829fe94663047fd108c5fcaa71d",
      "ecdb37b8120a2d3a4c706b016bd1bfd7",
      "1d4543ed8d2b9368cb96898095fe8a75",
      "f791c9a67b913cbd82d9da8ecede30e2",
      "065c70646f4dbaff913282f55a45a441",
      "51f87123616662ef7c35691497dfd0ba",
      "85c01ba03df68f9ece7bd3fa0f8980e6",
      "ad19b7dac092f56df6d054e1f67f21e7",
      "0edc415b5dd7299f7a34fb9f71d31d78",
      "2bc8ec19e9f4b77a64b8a0a1f6aec7e7",
  },
  {
      // 16X16
      "e33cb3f56a878e2fddb1b2fc51cdd275",
      "c7bff6f04b6052c8ab335d726dbbd52d",
      "d0b0b47b654a9bcc5c6008110a44589b",
      "78f5da7b10b2b9ab39f114a33b6254e9",
      "c78e31d23831abb40d6271a318fdd6f3",
      "90d1347f4ec9198a0320daecb6ff90b8",
      "e63ded54ab3d0e8728b6f24d4f01e53f",
      "35ce21fbe0ea114c089fc3489a78155d",
      "f277f6ef8e4d717f1f0dfe2706ac197d",
      "e8014d3f41256976c02e0f1e622ba2b9",
  },
  {
      // 32X32
      "a3e8056ba7e36628cce4917cd956fedd",
      "cc7d3024fe8748b512407edee045377e",
      "2aab0a0f330a1d3e19b8ecb8f06387a3",
      "a547bc3fb7b06910bf3973122a426661",
      "26f712514da95042f93d6e8dc8e431dc",
      "bb08c6e16177081daa3d936538dbc2e3",
      "84bf83f94a51b33654ca940c6f8bc057",
      "7168b03fc31bf29596a344d6a35d007c",
      "b073a70d3672f1282236994f5d12e94b",
      "c51607aebad5dcb3c1e3b58ef9e5b84e",
  },
  {
      // 64X64
      "a6baa0d4bfb2269a94c7a38f86a4bccf",
      "3f1ef5f473a49eba743f17a3324adf9d",
      "12ac11889ae5f55b7781454efd706a6a",
      "d9a906c0e692b22e1b4414e71a704b7e",
      "47d4cadd56f70c11ff8f3e5d8df81161",
      "de997744cf24c16c5ac2a36b02b351cc",
      "23781211ae178ddeb6c4bb97a6bd7d83",
      "a79d2e28340ca34b9e37daabbf030f63",
      "0372bd3ddfc258750a6ac106b70587f4",
      "228ef625d9460cbf6fa253a16a730976",
  },
  {
      // 4X8
      "22d519b796d59644043466320e4ccd14",
      "09513a738c49b3f9542d27f34abbe1d5",
      "807ae5e8813443ff01e71be6efacfb69",
      "cbfa18d0293430b6e9708b0be1fd2394",
      "346c354c34ec7fa780b576db355dab88",
      "f97dae85c35359632380b09ca98d611e",
      "698ae351d8896d89ed9e4e67b6e53eda",
      "dcc197034a9c45a3d8238bf085835f4e",
      "7a35e2c42ffdc2efc2d6d1d75a100fc7",
      "41ab6cebd4516c87a91b2a593e2c2506",
  },
  {
      // 8X4
      "d58cd4c4bf3b7bbaa5db5e1a5622ec78",
      "6e572c35aa782d00cafcb99e9ea047ea",
      "e8c22a3702b416dc9ab974505afbed09",
      "aaa4e4762a795aad7ad74de0c662c4e4",
      "a19f9101967383c3dcbd516dc317a291",
      "9ab8cb91f1a595b9ebe3fe8de58031aa",
      "2cf9021d5f1169268699807ee118b65f",
      "ee9605fcbd6fb871f1c5cd81a6989327",
      "b4871af8316089e3e23522175df7e93f",
      "d33301e1c2cb173be46792a22d19881a",
  },
  {
      // 8X16
      "4562de1d0336610880fdd5685498a9ec",
      "16310fa7076394f16fc85c4b149d89c9",
      "0e94af88e1dc573b6f0f499cddd1f530",
      "dfd245ee20d091c67809160340365aa9",
      "d3562504327f70c096c5be23fd8a3747",
      "601b853558502acbb5135eadd2da117a",
      "3c624345a723a1b2b1bea05a6a08bc99",
      "2a9c781de609e0184cc7ab442050f4e5",
      "0ddc5035c22252747126b61fc238c74d",
      "e43f5d83bab759af69c7b6773fc8f9b2",
  },
  {
      // 16X8
      "a57d6b5a9bfd30c29591d8717ace9c51",
      "f5907ba97ee6c53e339e953fc8d845ee",
      "ea3aa727913ce45af06f89dd1808db5f",
      "408af4f23e48d14b48ee35ae094fcd18",
      "85c41cbcb5d744f7961e8950026fbffe",
      "8a4e588a837638887ba671f8d4910485",
      "b792d8826b67a21757ea7097cff9e05b",
      "f94ce7101bb87fd3bb9312112527dbf4",
      "688c6660a6dc6fa61fa1aa38e708c209",
      "0cdf641b4f81d69509c92ae0b93ef5ff",
  },
  {
      // 16X32
      "aee4b3b0e3cc02d48e2c40d77f807927",
      "8baef2b2e789f79c8df9d90ad10f34a4",
      "038c38ee3c4f090bb8d736eab136aafc",
      "1a3de2aaeaffd68a9fd6c7f6557b83f3",
      "385c6e0ea29421dd81011a2934641e26",
      "6cf96c285d1a2d4787f955dad715b08c",
      "2d7f75dcd73b9528c8396279ff09ff3a",
      "5a63cd1841e4ed470e4ca5ef845f2281",
      "610d899ca945fbead33287d4335a8b32",
      "6bafaad81fce37be46730187e78d8b11",
  },
  {
      // 32X16
      "290b23c9f5a1de7905bfa71a942da29b",
      "701e7b82593c66da5052fc4b6afd79ce",
      "4da828c5455cd246735a663fbb204989",
      "e3fbeaf234efece8dbd752b77226200c",
      "4d1d8c969f05155a7e7e84cf7aad021b",
      "c22e4877c2c946d5bdc0d542e29e70cf",
      "8ac1ce815e7780500f842b0beb0bb980",
      "9fee2e2502b507f25bfad30a55b0b610",
      "4ced9c212ec6f9956e27f68a91b59fef",
      "4a7a0b93f138bb0863e4e465b01ec0b1",
  },
  {
      // 32X64
      "ad9cfc395a5c5644a21d958c7274ac14",
      "f29d6d03c143ddf96fef04c19f2c8333",
      "a8bdc852ef704dd4975c61893e8fbc3f",
      "7d0bd7dea26226741dbca9a97f27fa74",
      "45c27c5cca9a91b6ae8379feb0881c9f",
      "8a0b78df1e001b85c874d686eac4aa1b",
      "ce9fa75fac54a3f6c0cc3f2083b938f1",
      "c0dca10d88762c954af18dc9e3791a39",
      "61df229eddfccab913b8fda4bb02f9ac",
      "4f4df6bc8d50a5600b573f0e44d70e66",
  },
  {
      // 64X32
      "db9d82921fd88b24fdff6f849f2f9c87",
      "5ecc7fdc52d2f575ad4f2d0e9e6b1e11",
      "b4581311a0a73d95dfac7f8f44591032",
      "68bd283cfd1a125f6b2ee47cee874d36",
      "804179f05c032908a5e36077bb87c994",
      "fc5fd041a8ee779015394d0c066ee43c",
      "68f5579ccadfe9a1baafb158334a3db2",
      "fe237e45e215ab06d79046da9ad71e84",
      "9a8a938a6824551bf7d21b8fd1d70ea1",
      "eb7332f2017cd96882c76e7136aeaf53",
  },
  {
      // 4X16
      "7bafa307d507747b8132e7735b7f1c73",
      "e58bc2d8213a97d1fea9cfb73d7a9633",
      "435f8a8e8bbf14dbf2fe16b2be9e97aa",
      "1d0e767b68d84acbfb50b7a04e633836",
      "5f713bd7b324fe73bb7063e35ee14e5e",
      "0dac4e1fa3d59814202715468c01ed56",
      "47709d1db4a330c7a8900f450e6fddd1",
      "258e0b930bb27db28f05da9cf7d1ee7c",
      "36cf030fbae767912593efea045bfff5",
      "248d7aceabb7499febae663fae41a920",
  },
  {
      // 16X4
      "04dde98e632670e393704742c89f9067",
      "8c72543f1664651ae1fa08e2ac0adb9b",
      "2354a2cdc2773aa2df8ab4010db1be39",
      "6300ad3221c26da39b10e0e6d87ee3be",
      "8ea30b661c6ba60b28d3167f19e449b8",
      "fb6c1e4ff101a371cede63c2955cdb7e",
      "a517c06433d6d7927b16a72184a23e92",
      "393828be5d62ab6c48668bea5e2f801a",
      "b1e510c542013eb9d6fb188dea2ce90a",
      "569a8f2fe01679ca216535ecbcdccb62",
  },
  {
      // 8X32
      "9d541865c185ca7607852852613ac1fc",
      "b96be67f08c6b5fa5ebd3411299c2f7c",
      "75a2dcf50004b9d188849b048239767e",
      "429492ff415c9fd9b050d73b2ad500f8",
      "64b3606c1ccd036bd766bd5711392cf4",
      "cb59844a0f01660ac955bae3511f1100",
      "3e076155b7a70e8828618e3f33b51e3d",
      "ed2d1f597ab7c50beff690f737cf9726",
      "7909c6a26aaf20c59d996d3e5b5f9c29",
      "965798807240c98c6f7cc9b457ed0773",
  },
  {
      // 32X8
      "36f391aa31619eec1f4d9ee95ea454cc",
      "b82648f14eeba2527357cb50bc3223cb",
      "7a7b2adf429125e8bee9d1d00a66e13f",
      "4198e4d6ba503b7cc2d7e96bb845f661",
      "96c160d2ec1be9fe0cdea9682f14d257",
      "19a450bcebaa75afb4fc6bd1fd6434af",
      "2bd2e35967d43d0ec1c6587a36f204d5",
      "49799a99aa4ccfbd989bee92a99422f1",
      "955530e99813812a74659edeac3f5475",
      "f0316b84e378a19cd11b19a6e40b2914",
  },
  {
      // 16X64
      "8cba1b70a0bde29e8ef235cedc5faa7d",
      "96d00ddc7537bf7f196006591b733b4e",
      "cbf69d5d157c9f3355a4757b1d6e3414",
      "3ac1f642019493dec1b737d7a3a1b4e5",
      "35f9ee300d7fa3c97338e81a6f21dcd4",
      "aae335442e77c8ebc280f16ea50ba9c7",
      "a6140fdac2278644328be094d88731db",
      "2df93621b6ff100f7008432d509f4161",
      "c77bf5aee39e7ed4a3dd715f816f452a",
      "02109bd63557d90225c32a8f1338258e",
  },
  {
      // 64X16
      "a5e2f9fb685d5f4a048e9a96affd25a4",
      "1348f249690d9eefe09d9ad7ead2c801",
      "525da4b187acd81b1ff1116b60461141",
      "e99d072de858094c98b01bd4a6772634",
      "873bfa9dc24693f19721f7c8d527f7d3",
      "0acfc6507bd3468e9679efc127d6e4b9",
      "57d03f8d079c7264854e22ac1157cfae",
      "6c2c4036f70c7d957a9399b5436c0774",
      "42b8e4a97b7f8416c72a5148c031c0b1",
      "a38a2c5f79993dfae8530e9e25800893",
  },
};

}  // namespace

#define HIGHBD_INTRA_PRED_TEST(arch, tx_size, dc, dc_left, dc_top, dc_128, v, \
                               h, paeth, smooth, smooth_v, smooth_h)          \
  TEST(arch, DISABLED_##TestHighbdIntraPred_##tx_size) {                      \
    static const AvxHighbdPredFunc aom_intra_pred[] = {                       \
      dc, dc_left, dc_top, dc_128, v, h, paeth, smooth, smooth_v, smooth_h    \
    };                                                                        \
    TestHighbdIntraPred(tx_size, aom_intra_pred, kHighbdSignatures[tx_size]); \
  }

// -----------------------------------------------------------------------------
// 4x4, 4x8, 4x16

HIGHBD_INTRA_PRED_TEST(
    C, TX_4X4, aom_highbd_dc_predictor_4x4_c,
    aom_highbd_dc_left_predictor_4x4_c, aom_highbd_dc_top_predictor_4x4_c,
    aom_highbd_dc_128_predictor_4x4_c, aom_highbd_v_predictor_4x4_c,
    aom_highbd_h_predictor_4x4_c, aom_highbd_paeth_predictor_4x4_c,
    aom_highbd_smooth_predictor_4x4_c, aom_highbd_smooth_v_predictor_4x4_c,
    aom_highbd_smooth_h_predictor_4x4_c)

HIGHBD_INTRA_PRED_TEST(
    C, TX_4X8, aom_highbd_dc_predictor_4x8_c,
    aom_highbd_dc_left_predictor_4x8_c, aom_highbd_dc_top_predictor_4x8_c,
    aom_highbd_dc_128_predictor_4x8_c, aom_highbd_v_predictor_4x8_c,
    aom_highbd_h_predictor_4x8_c, aom_highbd_paeth_predictor_4x8_c,
    aom_highbd_smooth_predictor_4x8_c, aom_highbd_smooth_v_predictor_4x8_c,
    aom_highbd_smooth_h_predictor_4x8_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_4X16, aom_highbd_dc_predictor_4x16_c,
    aom_highbd_dc_left_predictor_4x16_c, aom_highbd_dc_top_predictor_4x16_c,
    aom_highbd_dc_128_predictor_4x16_c, aom_highbd_v_predictor_4x16_c,
    aom_highbd_h_predictor_4x16_c, aom_highbd_paeth_predictor_4x16_c,
    aom_highbd_smooth_predictor_4x16_c, aom_highbd_smooth_v_predictor_4x16_c,
    aom_highbd_smooth_h_predictor_4x16_c)
#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TX_4X4, aom_highbd_dc_predictor_4x4_sse2,
                       aom_highbd_dc_left_predictor_4x4_sse2,
                       aom_highbd_dc_top_predictor_4x4_sse2,
                       aom_highbd_dc_128_predictor_4x4_sse2,
                       aom_highbd_v_predictor_4x4_sse2,
                       aom_highbd_h_predictor_4x4_sse2, nullptr, nullptr,
                       nullptr, nullptr)

HIGHBD_INTRA_PRED_TEST(SSE2, TX_4X8, aom_highbd_dc_predictor_4x8_sse2,
                       aom_highbd_dc_left_predictor_4x8_sse2,
                       aom_highbd_dc_top_predictor_4x8_sse2,
                       aom_highbd_dc_128_predictor_4x8_sse2,
                       aom_highbd_v_predictor_4x8_sse2,
                       aom_highbd_h_predictor_4x8_sse2, nullptr, nullptr,
                       nullptr, nullptr)
#endif
#if HAVE_NEON
HIGHBD_INTRA_PRED_TEST(NEON, TX_4X4, aom_highbd_dc_predictor_4x4_neon, nullptr,
                       nullptr, nullptr, aom_highbd_v_predictor_4x4_neon,
                       nullptr, aom_highbd_paeth_predictor_4x4_neon,
                       aom_highbd_smooth_predictor_4x4_neon,
                       aom_highbd_smooth_v_predictor_4x4_neon,
                       aom_highbd_smooth_h_predictor_4x4_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_4X8, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_4x8_neon, nullptr,
                       aom_highbd_paeth_predictor_4x8_neon,
                       aom_highbd_smooth_predictor_4x8_neon,
                       aom_highbd_smooth_v_predictor_4x8_neon,
                       aom_highbd_smooth_h_predictor_4x8_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_4X16, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_4x16_neon, nullptr,
                       aom_highbd_paeth_predictor_4x16_neon,
                       aom_highbd_smooth_predictor_4x16_neon,
                       aom_highbd_smooth_v_predictor_4x16_neon,
                       aom_highbd_smooth_h_predictor_4x16_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 8x8, 8x4, 8x16, 8x32

HIGHBD_INTRA_PRED_TEST(
    C, TX_8X8, aom_highbd_dc_predictor_8x8_c,
    aom_highbd_dc_left_predictor_8x8_c, aom_highbd_dc_top_predictor_8x8_c,
    aom_highbd_dc_128_predictor_8x8_c, aom_highbd_v_predictor_8x8_c,
    aom_highbd_h_predictor_8x8_c, aom_highbd_paeth_predictor_8x8_c,
    aom_highbd_smooth_predictor_8x8_c, aom_highbd_smooth_v_predictor_8x8_c,
    aom_highbd_smooth_h_predictor_8x8_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_8X4, aom_highbd_dc_predictor_8x4_c,
    aom_highbd_dc_left_predictor_8x4_c, aom_highbd_dc_top_predictor_8x4_c,
    aom_highbd_dc_128_predictor_8x4_c, aom_highbd_v_predictor_8x4_c,
    aom_highbd_h_predictor_8x4_c, aom_highbd_paeth_predictor_8x4_c,
    aom_highbd_smooth_predictor_8x4_c, aom_highbd_smooth_v_predictor_8x4_c,
    aom_highbd_smooth_h_predictor_8x4_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_8X16, aom_highbd_dc_predictor_8x16_c,
    aom_highbd_dc_left_predictor_8x16_c, aom_highbd_dc_top_predictor_8x16_c,
    aom_highbd_dc_128_predictor_8x16_c, aom_highbd_v_predictor_8x16_c,
    aom_highbd_h_predictor_8x16_c, aom_highbd_paeth_predictor_8x16_c,
    aom_highbd_smooth_predictor_8x16_c, aom_highbd_smooth_v_predictor_8x16_c,
    aom_highbd_smooth_h_predictor_8x16_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_8X32, aom_highbd_dc_predictor_8x32_c,
    aom_highbd_dc_left_predictor_8x32_c, aom_highbd_dc_top_predictor_8x32_c,
    aom_highbd_dc_128_predictor_8x32_c, aom_highbd_v_predictor_8x32_c,
    aom_highbd_h_predictor_8x32_c, aom_highbd_paeth_predictor_8x32_c,
    aom_highbd_smooth_predictor_8x32_c, aom_highbd_smooth_v_predictor_8x32_c,
    aom_highbd_smooth_h_predictor_8x32_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TX_8X8, aom_highbd_dc_predictor_8x8_sse2,
                       aom_highbd_dc_left_predictor_8x8_sse2,
                       aom_highbd_dc_top_predictor_8x8_sse2,
                       aom_highbd_dc_128_predictor_8x8_sse2,
                       aom_highbd_v_predictor_8x8_sse2,
                       aom_highbd_h_predictor_8x8_sse2, nullptr, nullptr,
                       nullptr, nullptr)
HIGHBD_INTRA_PRED_TEST(SSE2, TX_8X4, aom_highbd_dc_predictor_8x4_sse2,
                       aom_highbd_dc_left_predictor_8x4_sse2,
                       aom_highbd_dc_top_predictor_8x4_sse2,
                       aom_highbd_dc_128_predictor_8x4_sse2,
                       aom_highbd_v_predictor_8x4_sse2,
                       aom_highbd_h_predictor_8x4_sse2, nullptr, nullptr,
                       nullptr, nullptr)
HIGHBD_INTRA_PRED_TEST(SSE2, TX_8X16, aom_highbd_dc_predictor_8x16_sse2,
                       aom_highbd_dc_left_predictor_8x16_sse2,
                       aom_highbd_dc_top_predictor_8x16_sse2,
                       aom_highbd_dc_128_predictor_8x16_sse2,
                       aom_highbd_v_predictor_8x16_sse2,
                       aom_highbd_h_predictor_8x16_sse2, nullptr, nullptr,
                       nullptr, nullptr)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3, TX_8X8, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_NEON
HIGHBD_INTRA_PRED_TEST(NEON, TX_8X8, aom_highbd_dc_predictor_8x8_neon, nullptr,
                       nullptr, nullptr, aom_highbd_v_predictor_8x8_neon,
                       nullptr, aom_highbd_paeth_predictor_8x8_neon,
                       aom_highbd_smooth_predictor_8x8_neon,
                       aom_highbd_smooth_v_predictor_8x8_neon,
                       aom_highbd_smooth_h_predictor_8x8_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_8X4, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_8x4_neon, nullptr,
                       aom_highbd_paeth_predictor_8x4_neon,
                       aom_highbd_smooth_predictor_8x4_neon,
                       aom_highbd_smooth_v_predictor_8x4_neon,
                       aom_highbd_smooth_h_predictor_8x4_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_8X16, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_8x16_neon, nullptr,
                       aom_highbd_paeth_predictor_8x16_neon,
                       aom_highbd_smooth_predictor_8x16_neon,
                       aom_highbd_smooth_v_predictor_8x16_neon,
                       aom_highbd_smooth_h_predictor_8x16_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_8X32, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_8x32_neon, nullptr,
                       aom_highbd_paeth_predictor_8x32_neon,
                       aom_highbd_smooth_predictor_8x32_neon,
                       aom_highbd_smooth_v_predictor_8x32_neon,
                       aom_highbd_smooth_h_predictor_8x32_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 16x16, 16x8, 16x32, 16x4, 16x64

HIGHBD_INTRA_PRED_TEST(
    C, TX_16X16, aom_highbd_dc_predictor_16x16_c,
    aom_highbd_dc_left_predictor_16x16_c, aom_highbd_dc_top_predictor_16x16_c,
    aom_highbd_dc_128_predictor_16x16_c, aom_highbd_v_predictor_16x16_c,
    aom_highbd_h_predictor_16x16_c, aom_highbd_paeth_predictor_16x16_c,
    aom_highbd_smooth_predictor_16x16_c, aom_highbd_smooth_v_predictor_16x16_c,
    aom_highbd_smooth_h_predictor_16x16_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_16X8, aom_highbd_dc_predictor_16x8_c,
    aom_highbd_dc_left_predictor_16x8_c, aom_highbd_dc_top_predictor_16x8_c,
    aom_highbd_dc_128_predictor_16x8_c, aom_highbd_v_predictor_16x8_c,
    aom_highbd_h_predictor_16x8_c, aom_highbd_paeth_predictor_16x8_c,
    aom_highbd_smooth_predictor_16x8_c, aom_highbd_smooth_v_predictor_16x8_c,
    aom_highbd_smooth_h_predictor_16x8_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_16X32, aom_highbd_dc_predictor_16x32_c,
    aom_highbd_dc_left_predictor_16x32_c, aom_highbd_dc_top_predictor_16x32_c,
    aom_highbd_dc_128_predictor_16x32_c, aom_highbd_v_predictor_16x32_c,
    aom_highbd_h_predictor_16x32_c, aom_highbd_paeth_predictor_16x32_c,
    aom_highbd_smooth_predictor_16x32_c, aom_highbd_smooth_v_predictor_16x32_c,
    aom_highbd_smooth_h_predictor_16x32_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_16X4, aom_highbd_dc_predictor_16x4_c,
    aom_highbd_dc_left_predictor_16x4_c, aom_highbd_dc_top_predictor_16x4_c,
    aom_highbd_dc_128_predictor_16x4_c, aom_highbd_v_predictor_16x4_c,
    aom_highbd_h_predictor_16x4_c, aom_highbd_paeth_predictor_16x4_c,
    aom_highbd_smooth_predictor_16x4_c, aom_highbd_smooth_v_predictor_16x4_c,
    aom_highbd_smooth_h_predictor_16x4_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_16X64, aom_highbd_dc_predictor_16x64_c,
    aom_highbd_dc_left_predictor_16x64_c, aom_highbd_dc_top_predictor_16x64_c,
    aom_highbd_dc_128_predictor_16x64_c, aom_highbd_v_predictor_16x64_c,
    aom_highbd_h_predictor_16x64_c, aom_highbd_paeth_predictor_16x64_c,
    aom_highbd_smooth_predictor_16x64_c, aom_highbd_smooth_v_predictor_16x64_c,
    aom_highbd_smooth_h_predictor_16x64_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TX_16X16, aom_highbd_dc_predictor_16x16_sse2,
                       aom_highbd_dc_left_predictor_16x16_sse2,
                       aom_highbd_dc_top_predictor_16x16_sse2,
                       aom_highbd_dc_128_predictor_16x16_sse2,
                       aom_highbd_v_predictor_16x16_sse2,
                       aom_highbd_h_predictor_16x16_sse2, nullptr, nullptr,
                       nullptr, nullptr)
HIGHBD_INTRA_PRED_TEST(SSE2, TX_16X8, aom_highbd_dc_predictor_16x8_sse2,
                       aom_highbd_dc_left_predictor_16x8_sse2,
                       aom_highbd_dc_top_predictor_16x8_sse2,
                       aom_highbd_dc_128_predictor_16x8_sse2,
                       aom_highbd_v_predictor_16x8_sse2,
                       aom_highbd_h_predictor_16x8_sse2, nullptr, nullptr,
                       nullptr, nullptr)
HIGHBD_INTRA_PRED_TEST(SSE2, TX_16X32, aom_highbd_dc_predictor_16x32_sse2,
                       aom_highbd_dc_left_predictor_16x32_sse2,
                       aom_highbd_dc_top_predictor_16x32_sse2,
                       aom_highbd_dc_128_predictor_16x32_sse2,
                       aom_highbd_v_predictor_16x32_sse2,
                       aom_highbd_h_predictor_16x32_sse2, nullptr, nullptr,
                       nullptr, nullptr)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3, TX_16X16, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2, TX_16X16, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)

HIGHBD_INTRA_PRED_TEST(AVX2, TX_16X8, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)

HIGHBD_INTRA_PRED_TEST(AVX2, TX_16X32, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_NEON
HIGHBD_INTRA_PRED_TEST(NEON, TX_16X16, aom_highbd_dc_predictor_16x16_neon,
                       nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_16x16_neon, nullptr,
                       aom_highbd_paeth_predictor_16x16_neon,
                       aom_highbd_smooth_predictor_16x16_neon,
                       aom_highbd_smooth_v_predictor_16x16_neon,
                       aom_highbd_smooth_h_predictor_16x16_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_16X8, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_16x8_neon, nullptr,
                       aom_highbd_paeth_predictor_16x8_neon,
                       aom_highbd_smooth_predictor_16x8_neon,
                       aom_highbd_smooth_v_predictor_16x8_neon,
                       aom_highbd_smooth_h_predictor_16x8_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_16X32, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_16x32_neon, nullptr,
                       aom_highbd_paeth_predictor_16x32_neon,
                       aom_highbd_smooth_predictor_16x32_neon,
                       aom_highbd_smooth_v_predictor_16x32_neon,
                       aom_highbd_smooth_h_predictor_16x32_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_16X4, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_16x4_neon, nullptr,
                       aom_highbd_paeth_predictor_16x4_neon,
                       aom_highbd_smooth_predictor_16x4_neon,
                       aom_highbd_smooth_v_predictor_16x4_neon,
                       aom_highbd_smooth_h_predictor_16x4_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_16X64, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_16x64_neon, nullptr,
                       aom_highbd_paeth_predictor_16x64_neon,
                       aom_highbd_smooth_predictor_16x64_neon,
                       aom_highbd_smooth_v_predictor_16x64_neon,
                       aom_highbd_smooth_h_predictor_16x64_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 32x32, 32x16, 32x64, 32x8

HIGHBD_INTRA_PRED_TEST(
    C, TX_32X32, aom_highbd_dc_predictor_32x32_c,
    aom_highbd_dc_left_predictor_32x32_c, aom_highbd_dc_top_predictor_32x32_c,
    aom_highbd_dc_128_predictor_32x32_c, aom_highbd_v_predictor_32x32_c,
    aom_highbd_h_predictor_32x32_c, aom_highbd_paeth_predictor_32x32_c,
    aom_highbd_smooth_predictor_32x32_c, aom_highbd_smooth_v_predictor_32x32_c,
    aom_highbd_smooth_h_predictor_32x32_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_32X16, aom_highbd_dc_predictor_32x16_c,
    aom_highbd_dc_left_predictor_32x16_c, aom_highbd_dc_top_predictor_32x16_c,
    aom_highbd_dc_128_predictor_32x16_c, aom_highbd_v_predictor_32x16_c,
    aom_highbd_h_predictor_32x16_c, aom_highbd_paeth_predictor_32x16_c,
    aom_highbd_smooth_predictor_32x16_c, aom_highbd_smooth_v_predictor_32x16_c,
    aom_highbd_smooth_h_predictor_32x16_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_32X64, aom_highbd_dc_predictor_32x64_c,
    aom_highbd_dc_left_predictor_32x64_c, aom_highbd_dc_top_predictor_32x64_c,
    aom_highbd_dc_128_predictor_32x64_c, aom_highbd_v_predictor_32x64_c,
    aom_highbd_h_predictor_32x64_c, aom_highbd_paeth_predictor_32x64_c,
    aom_highbd_smooth_predictor_32x64_c, aom_highbd_smooth_v_predictor_32x64_c,
    aom_highbd_smooth_h_predictor_32x64_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_32X8, aom_highbd_dc_predictor_32x8_c,
    aom_highbd_dc_left_predictor_32x8_c, aom_highbd_dc_top_predictor_32x8_c,
    aom_highbd_dc_128_predictor_32x8_c, aom_highbd_v_predictor_32x8_c,
    aom_highbd_h_predictor_32x8_c, aom_highbd_paeth_predictor_32x8_c,
    aom_highbd_smooth_predictor_32x8_c, aom_highbd_smooth_v_predictor_32x8_c,
    aom_highbd_smooth_h_predictor_32x8_c)

#if HAVE_SSE2
HIGHBD_INTRA_PRED_TEST(SSE2, TX_32X32, aom_highbd_dc_predictor_32x32_sse2,
                       aom_highbd_dc_left_predictor_32x32_sse2,
                       aom_highbd_dc_top_predictor_32x32_sse2,
                       aom_highbd_dc_128_predictor_32x32_sse2,
                       aom_highbd_v_predictor_32x32_sse2,
                       aom_highbd_h_predictor_32x32_sse2, nullptr, nullptr,
                       nullptr, nullptr)
HIGHBD_INTRA_PRED_TEST(SSE2, TX_32X16, aom_highbd_dc_predictor_32x16_sse2,
                       aom_highbd_dc_left_predictor_32x16_sse2,
                       aom_highbd_dc_top_predictor_32x16_sse2,
                       aom_highbd_dc_128_predictor_32x16_sse2,
                       aom_highbd_v_predictor_32x16_sse2,
                       aom_highbd_h_predictor_32x16_sse2, nullptr, nullptr,
                       nullptr, nullptr)
#endif

#if HAVE_SSSE3
HIGHBD_INTRA_PRED_TEST(SSSE3, TX_32X32, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_AVX2
HIGHBD_INTRA_PRED_TEST(AVX2, TX_32X32, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)

HIGHBD_INTRA_PRED_TEST(AVX2, TX_32X16, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
#endif

#if HAVE_NEON
HIGHBD_INTRA_PRED_TEST(NEON, TX_32X32, aom_highbd_dc_predictor_32x32_neon,
                       nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_32x32_neon, nullptr,
                       aom_highbd_paeth_predictor_32x32_neon,
                       aom_highbd_smooth_predictor_32x32_neon,
                       aom_highbd_smooth_v_predictor_32x32_neon,
                       aom_highbd_smooth_h_predictor_32x32_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_32X16, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_32x16_neon, nullptr,
                       aom_highbd_paeth_predictor_32x16_neon,
                       aom_highbd_smooth_predictor_32x16_neon,
                       aom_highbd_smooth_v_predictor_32x16_neon,
                       aom_highbd_smooth_h_predictor_32x16_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_32X64, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_32x64_neon, nullptr,
                       aom_highbd_paeth_predictor_32x64_neon,
                       aom_highbd_smooth_predictor_32x64_neon,
                       aom_highbd_smooth_v_predictor_32x64_neon,
                       aom_highbd_smooth_h_predictor_32x64_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_32X8, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_32x8_neon, nullptr,
                       aom_highbd_paeth_predictor_32x8_neon,
                       aom_highbd_smooth_predictor_32x8_neon,
                       aom_highbd_smooth_v_predictor_32x8_neon,
                       aom_highbd_smooth_h_predictor_32x8_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
// 64x64, 64x32, 64x16

HIGHBD_INTRA_PRED_TEST(
    C, TX_64X64, aom_highbd_dc_predictor_64x64_c,
    aom_highbd_dc_left_predictor_64x64_c, aom_highbd_dc_top_predictor_64x64_c,
    aom_highbd_dc_128_predictor_64x64_c, aom_highbd_v_predictor_64x64_c,
    aom_highbd_h_predictor_64x64_c, aom_highbd_paeth_predictor_64x64_c,
    aom_highbd_smooth_predictor_64x64_c, aom_highbd_smooth_v_predictor_64x64_c,
    aom_highbd_smooth_h_predictor_64x64_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_64X32, aom_highbd_dc_predictor_64x32_c,
    aom_highbd_dc_left_predictor_64x32_c, aom_highbd_dc_top_predictor_64x32_c,
    aom_highbd_dc_128_predictor_64x32_c, aom_highbd_v_predictor_64x32_c,
    aom_highbd_h_predictor_64x32_c, aom_highbd_paeth_predictor_64x32_c,
    aom_highbd_smooth_predictor_64x32_c, aom_highbd_smooth_v_predictor_64x32_c,
    aom_highbd_smooth_h_predictor_64x32_c)
HIGHBD_INTRA_PRED_TEST(
    C, TX_64X16, aom_highbd_dc_predictor_64x16_c,
    aom_highbd_dc_left_predictor_64x16_c, aom_highbd_dc_top_predictor_64x16_c,
    aom_highbd_dc_128_predictor_64x16_c, aom_highbd_v_predictor_64x16_c,
    aom_highbd_h_predictor_64x16_c, aom_highbd_paeth_predictor_64x16_c,
    aom_highbd_smooth_predictor_64x16_c, aom_highbd_smooth_v_predictor_64x16_c,
    aom_highbd_smooth_h_predictor_64x16_c)

#if HAVE_NEON
HIGHBD_INTRA_PRED_TEST(NEON, TX_64X64, aom_highbd_dc_predictor_64x64_neon,
                       nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_64x64_neon, nullptr,
                       aom_highbd_paeth_predictor_64x64_neon,
                       aom_highbd_smooth_predictor_64x64_neon,
                       aom_highbd_smooth_v_predictor_64x64_neon,
                       aom_highbd_smooth_h_predictor_64x64_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_64X32, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_64x32_neon, nullptr,
                       aom_highbd_paeth_predictor_64x32_neon,
                       aom_highbd_smooth_predictor_64x32_neon,
                       aom_highbd_smooth_v_predictor_64x32_neon,
                       aom_highbd_smooth_h_predictor_64x32_neon)
HIGHBD_INTRA_PRED_TEST(NEON, TX_64X16, nullptr, nullptr, nullptr, nullptr,
                       aom_highbd_v_predictor_64x16_neon, nullptr,
                       aom_highbd_paeth_predictor_64x16_neon,
                       aom_highbd_smooth_predictor_64x16_neon,
                       aom_highbd_smooth_v_predictor_64x16_neon,
                       aom_highbd_smooth_h_predictor_64x16_neon)
#endif  // HAVE_NEON

// -----------------------------------------------------------------------------
#endif  // CONFIG_AV1_HIGHBITDEPTH

#include "test/test_libaom.cc"
