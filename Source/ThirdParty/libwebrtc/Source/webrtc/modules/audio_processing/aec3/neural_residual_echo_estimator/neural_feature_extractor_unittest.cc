/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
Python script used to generate the test data:

import numpy as np
from typing import List

def python_feature_extractor(time_frame: np.ndarray) -> np.ndarray:
  frame_length: int = 256
  sqrt_hann: np.ndarray = np.sqrt(np.hanning(frame_length))
  magnitude_spectrum: np.ndarray = np.abs(np.fft.rfft(time_frame * sqrt_hann))
  return np.power(magnitude_spectrum + 1e-8, 0.3)

def format_as_cpp_array(data: np.ndarray, name: str) -> str:
  elements_per_line = 6
  s = f"constexpr float {name}[] = {{\n    "
  for i, x in enumerate(data):
    s += f"{x:.8f}, "
    if (i + 1) % elements_per_line == 0 and i < len(data) - 1:
      s += "\n    "
  s = s.rstrip(", ") + "\n};"
  return s

# Generate two frames of white noise
np.random.seed(0) # for reproducibility
noise1: np.ndarray = np.random.uniform(-1.0, 1.0, 256)
noise2: np.ndarray = np.random.uniform(-1.0, 1.0, 256)

# Scale to match the C++ implementation's expected input range
noise1_scaled: np.ndarray = noise1 * 32768.0
noise2_scaled: np.ndarray = noise2 * 32768.0

# Python equivalent
expected_output1: np.ndarray = python_feature_extractor(noise1)
expected_output2: np.ndarray = python_feature_extractor(noise2)

print(format_as_cpp_array(noise1_scaled, "noise1_scaled"))
print(format_as_cpp_array(noise2_scaled, "noise2_scaled"))
print(format_as_cpp_array(expected_output1, "expected_output1"))
print(format_as_cpp_array(expected_output2, "expected_output2"))
*/

#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"

#include <array>
#include <cstddef>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::FloatNear;
using ::testing::Pointwise;

constexpr float kTolerance = 1e-6f;

// Test data generated from the Python implementation.

constexpr float noise1_scaled[] = {
    3199.0418,   14102.6503,  6734.7006,   2941.4643,   -5003.3591,
    9561.3166,   -4090.2845,  25675.2354,  30386.6027,  -7638.7766,
    19118.4921,  1893.6575,   4459.3684,   27891.9013,  -28112.5809,
    -27057.8942, -31442.9671, 21798.5742,  18229.2808,  24249.1161,
    31366.7317,  19605.6557,  -2524.4885,  18384.7601,  -25016.7672,
    9169.8641,   -23373.1990, 29141.8221,  1431.8516,   -5592.7151,
    -15430.0834, 17972.1791,  -2873.7318,  4484.8873,   -31536.5916,
    7709.3599,   7346.3053,   7663.3864,   29081.4741,  11915.7751,
    -9207.2902,  -4126.6739,  12951.9581,  -28821.0635, 10929.2235,
    11182.9234,  -18980.3685, -24318.6862, -12096.0876, -8931.8509,
    4600.4155,   -4023.8112,  32006.0679,  -26080.3913, -19079.0529,
    -22196.4194, 10034.1072,  -16168.2815, -2207.8572,  -16749.3244,
    -22349.7694, -25534.4547, 10245.2160,  -23712.0421, -19884.7783,
    -8603.2272,  21036.6123,  -26404.3708, 22147.5575,  -26470.0947,
    31225.2475,  -2054.4748,  31245.0147,  6871.1560,   15680.3779,
    -30199.7888, -14233.9629, -24890.7982, -13360.1560, -24987.0602,
    -11928.6544, -5618.8604,  -28564.0297, 12613.8528,  4364.7929,
    -15375.4343, 1523.5844,   -26611.5147, 4977.2295,   28134.3556,
    -11890.2651, 10971.4067,  -24130.4953, 14177.2196,  -13801.4823,
    -20762.3709, 5669.7117,   -31450.2319, 21557.4138,  -32460.2773,
    11653.3846,  -15072.7575, 15413.6754,  30289.9885,  -16465.7140,
    4991.0471,   6032.0600,   4735.1009,   -18148.1221, 29671.3592,
    -3465.1912,  22702.2388,  13073.0738,  -13275.1720, 20565.0539,
    -6782.5998,  24975.9791,  5326.2990,   25017.4087,  12617.7503,
    14762.2645,  86.7947,     29889.8971,  9436.5417,   -4990.2355,
    6972.5857,   -31510.1546, -13003.9928, 10497.1330,  -13757.4739,
    7734.2592,   -4668.2144,  -23889.5717, -13219.7695, 4585.2204,
    5955.4373,   4870.9795,   10040.1689,  9968.2399,   -4494.5614,
    25988.0777,  -8679.4653,  -4203.1563,  25685.0890,  20066.7293,
    13362.0422,  -26199.5307, 27491.2126,  14040.5178,  32692.4374,
    -22973.7559, 24125.5093,  -22118.8630, 7573.3116,   -24653.3336,
    22807.0673,  20140.4553,  4528.5860,   -6082.8354,  -28235.0718,
    12938.6921,  -3044.6267,  14552.6358,  24011.2321,  31163.7774,
    23317.9278,  -32000.3058, -9176.4776,  15072.6615,  -21520.0775,
    1378.6550,   -29206.9056, -19661.0277, -31554.1557, 19247.7727,
    -18092.8716, -10135.0323, 28054.7356,  13396.5022,  -30681.4039,
    -21974.6038, 7961.2085,   5061.2528,   -17177.4561, 28456.6486,
    7468.8729,   2335.2314,   5892.3402,   15081.2773,  -12324.3728,
    -6670.1845,  -19015.6801, -20565.6552, 29122.3889,  15699.2009,
    -625.2915,   -17864.1549, -16098.4936, -28965.0009, -4298.0720,
    -12334.1451, 12867.5669,  -8011.6555,  -20997.4934, -31150.6549,
    -28360.7282, 11756.6848,  -3034.5236,  2397.2552,   25996.2499,
    32134.8533,  -18553.4392, 10687.4931,  -15510.9047, -31414.6161,
    16933.1035,  -11795.3560, -7637.3102,  5787.9504,   21695.5916,
    8452.9541,   24422.0334,  -14841.1492, 19532.7973,  -20602.1628,
    29674.1540,  12287.2317,  -18644.4889, 29318.8790,  15129.3662,
    -16125.6805, -18788.3863, 1192.8020,   -31086.1681, -19171.2411,
    -4935.8131,  -8246.3962,  -2387.1210,  -14573.3251, 5687.4989,
    23845.6410,  -25065.4323, 1138.9572,   -24112.7846, 14212.1161,
    -6811.8313,  4287.4511,   -20756.5727, -23275.2572, -782.7436,
    -9462.5636,  28864.1480,  17388.3558,  16296.4190,  26458.1769,
    -27300.8273};

constexpr float noise2_scaled[] = {
    3420.4857,   5536.2237,   30273.4625,  -13621.8197, -16985.0451,
    -26195.1362, -31691.2678, 28149.6333,  11135.6508,  18687.7812,
    -14304.5358, 5662.9767,   -28576.6277, -941.9099,   31293.1215,
    24674.6478,  -10606.4149, 30249.4616,  -17583.2022, 29446.5583,
    28926.1293,  19608.5408,  8549.0360,   24529.3362,  -13564.6226,
    22868.3648,  7725.1669,   -31900.5093, -10011.7042, -23059.4405,
    31577.1709,  -1417.5236,  -170.9595,   9140.4708,   -8612.4393,
    -23796.1038, 21110.3078,  -20326.1272, 741.8008,    -18067.1592,
    -26355.6639, 23736.5833,  30993.2516,  30201.2602,  26644.0212,
    17959.9660,  -10934.9993, -27452.9393, -6079.0426,  -17548.3033,
    -24085.2904, -29266.5962, 14784.5523,  -32019.0901, 17732.7799,
    -23137.7046, -27556.4408, -26895.7755, 11275.3251,  -16687.6145,
    -5207.5255,  3759.7211,   23629.0817,  14879.5728,  -15051.7904,
    -24151.1433, -29138.9885, -13002.4319, -15589.8250, -2874.3718,
    12011.5256,  12820.5092,  -14187.3089, -7869.1070,  -20896.0906,
    18910.1187,  -29042.4045, 12910.4112,  18264.5815,  18180.1820,
    -15766.4828, -8269.7822,  5740.9297,   -14888.3438, -8463.7910,
    -19853.8507, -2630.8848,  -29844.2882, 19647.4231,  -27724.5823,
    1234.3803,   -12660.8933, 5081.8547,   30109.4234,  9540.0915,
    -30450.4874, -4561.1457,  656.4644,    2370.9283,   11887.7396,
    -14575.4621, -24322.9940, -7033.6069,  29911.0054,  -20504.1899,
    26475.4925,  2870.8667,   -2823.8531,  25037.4659,  -2712.9308,
    14691.0502,  -6617.4765,  26479.4533,  12453.4797,  13082.4309,
    -11290.5158, 16828.2451,  8916.8973,   -17038.0314, -22246.9277,
    19424.3117,  30091.9425,  -2743.4138,  5962.7383,   23443.7112,
    -2803.4038,  29614.0457,  4964.4282,   21021.7940,  26793.9819,
    20678.1690,  -22320.6137, 8447.4881,   -6656.2124,  -28658.0440,
    -4978.6223,  -15814.8810, 22874.5746,  -30585.3480, 30079.8917,
    -9478.5471,  -9390.8572,  -31697.8952, -20628.6143, -6471.0574,
    28134.0423,  -26239.6359, 29183.2813,  24214.8003,  -3004.0132,
    -11357.3310, -17514.8807, 7501.5590,   -30600.4236, -31745.2410,
    -4666.4435,  -28306.6975, -16256.7954, -18273.9983, -16174.8619,
    -24179.1644, -31979.1941, -25199.6211, 7764.7223,   31080.8552,
    32135.2500,  -5960.2308,  -22088.6187, 9093.8905,   -635.3488,
    32073.9592,  -28488.2235, 18562.0521,  -13867.5161, -16946.3893,
    10649.8996,  -16642.0031, 10869.7431,  1134.3310,   -4974.9041,
    3584.0202,   -13955.7916, 13538.0800,  -5579.9402,  -9139.2861,
    21538.8596,  27850.6315,  -29752.8649, -17522.5574, -9927.4346,
    20641.6432,  31817.1662,  30734.5296,  26538.6948,  -13332.8886,
    32244.4488,  -16422.0082, -25827.3342, 29553.6303,  -17470.5701,
    12436.6530,  -28943.5577, 15119.7515,  25016.4158,  -14913.5756,
    -7926.1273,  -8238.1253,  16304.5872,  -17183.0646, -21505.4353,
    -3323.2225,  -12814.3585, 22229.0983,  -17187.3517, 156.5955,
    29005.1588,  8781.6731,   24070.6785,  28849.5822,  16434.1260,
    13079.3511,  30668.5914,  32401.0502,  -3157.4142,  -28123.4782,
    -13579.4504, -22783.2820, -5407.6129,  -24163.8226, 6823.4644,
    -7680.2910,  25912.0093,  30657.3916,  3072.6489,   -14757.1625,
    6044.4127,   26002.1393,  -6112.3234,  3413.0019,   -14964.9642,
    -2920.0122,  -6441.3017,  -16487.9752, 384.4593,    -12426.8822,
    -8320.7872,  1636.4629,   16422.9954,  -10911.2547, 27797.6689,
    23744.9083,  -29577.0328, -16145.2835, -3530.0630,  -25911.1067,
    -9930.2776};

constexpr float expected_output1[] = {
    1.2202633, 1.7272304, 2.1753535, 1.6639511, 1.4504087, 1.5607018, 1.7948899,
    2.0986237, 1.5073231, 0.8968942, 1.8699082, 2.0595039, 1.6333632, 1.6918161,
    1.7579499, 1.8817867, 1.9033698, 1.9425666, 1.4443926, 1.1040287, 1.4225169,
    1.1535512, 1.4586320, 1.6414003, 1.7915095, 1.6791658, 1.4245994, 1.5361380,
    2.0224556, 1.4529938, 1.3677414, 1.6004674, 1.7868200, 1.3359326, 1.9621301,
    1.4692749, 1.6836248, 1.6219408, 1.6542681, 2.2417320, 1.9120614, 1.6369832,
    1.1818825, 1.3819567, 1.3740384, 1.3323426, 1.7350840, 1.5579263, 1.1322017,
    1.2045572, 1.6530098, 1.6843505, 1.2226551, 1.4986210, 1.5158652, 1.5288519,
    1.4476088, 1.7631098, 1.4404006, 1.0171719, 1.7696546, 2.0226616, 2.0523162,
    1.5416487, 1.5385250, 1.0534991, 1.3605192, 1.4166694, 1.6238999, 1.6638377,
    1.4028377, 1.6349643, 1.5471496, 1.5039228, 1.3435868, 2.0315477, 1.6629901,
    1.5412650, 1.7623193, 1.8761405, 1.5532731, 1.9655503, 1.9347810, 1.4526643,
    0.9392141, 1.5384618, 1.5229951, 1.3041083, 1.2288715, 1.5890568, 1.4367742,
    1.8774723, 1.7158524, 1.5562983, 1.8137322, 1.3629094, 1.5521119, 1.5687853,
    1.6626421, 1.8479395, 1.6954730, 1.5309387, 1.5702729, 1.8073848, 1.8335479,
    1.7042632, 1.6445036, 1.6976509, 1.1417790, 1.5238974, 2.1945088, 2.1619850,
    1.7098370, 1.6523124, 1.9440371, 1.8486495, 2.0672979, 2.1444454, 1.3407502,
    1.6834193, 1.8205304, 1.8301605, 1.6322767, 1.9723609, 2.1829500, 1.4344222,
    1.9528573, 1.8263685, 1.7367752};

constexpr float expected_output2[] = {
    1.4387245, 1.4403429, 1.9777731, 2.2877072, 1.8727017, 1.9210667, 1.3046225,
    1.4128947, 1.1877113, 1.4548492, 1.9006205, 1.8816212, 2.1369724, 1.5365391,
    2.0703334, 2.1232226, 1.6342221, 1.3652948, 1.7604186, 1.9382242, 1.3772832,
    1.4379133, 1.7277498, 1.4895349, 1.7578079, 1.3160488, 1.4388874, 2.1002045,
    1.6475185, 2.0833502, 2.0610020, 1.5717945, 1.2313899, 1.8931674, 1.4281442,
    1.6715665, 1.9357384, 1.0737266, 1.8492249, 1.6154146, 1.7171611, 1.2594775,
    1.6492247, 1.7053658, 1.7071571, 1.4533884, 1.8833118, 1.6621069, 1.7046046,
    1.3899836, 1.9614496, 1.5187381, 1.5449415, 1.7357930, 1.7269029, 1.4725356,
    1.8128068, 1.3997501, 1.7827980, 1.6610097, 1.8852335, 1.2815337, 2.0144823,
    1.5584240, 1.3150680, 2.0394259, 2.0875142, 1.7976422, 1.4628408, 1.6218163,
    1.5199225, 1.2747693, 0.7672618, 1.6249810, 1.8661000, 1.7897703, 1.3933436,
    1.6946455, 1.6239630, 1.5750381, 1.6310876, 1.1158317, 1.7192902, 1.5997313,
    1.9059273, 1.8387797, 1.8939278, 1.5037787, 1.3948746, 1.6324782, 1.2768368,
    1.5373271, 1.4816556, 1.3888880, 1.5884829, 1.3518394, 1.8841741, 1.4184990,
    1.3060136, 1.8983842, 1.9823723, 1.9028293, 1.9833195, 2.0924404, 1.3062575,
    1.4686721, 1.5963211, 1.4120681, 1.5727397, 1.3416973, 2.0295401, 1.7874475,
    1.3433882, 1.7285510, 1.9679515, 2.0707020, 1.9011226, 1.6455043, 1.5474962,
    1.8804304, 2.0154081, 2.0223212, 1.8945941, 1.4271733, 2.0051481, 1.7717120,
    1.6581700, 1.2560840, 1.4274090};

const std::array<FeatureExtractor::ModelInputEnum, 2> kExpectedInputs = {
    FeatureExtractor::ModelInputEnum::kLinearAecOutput,
    FeatureExtractor::ModelInputEnum::kAecRef};
constexpr int kStepSize = 128;
class FrequencyDomainFeatureExtractorTest
    : public ::testing::TestWithParam<int> {
 protected:
  void UpdateBlock(FrequencyDomainFeatureExtractor& extractor,
                   ArrayView<const float, kBlockSize> block,
                   int num_channels) {
    std::vector<ArrayView<const float, kBlockSize>> all_blocks;
    all_blocks.reserve(num_channels);
    for (int i = 0; i < num_channels; ++i) {
      all_blocks.push_back(block);
    }
    for (auto input_type : kExpectedInputs) {
      extractor.UpdateBuffers(all_blocks, input_type);
    }
  }
};

TEST_P(FrequencyDomainFeatureExtractorTest, BasicTest) {
  const int num_channels = this->GetParam();
  FrequencyDomainFeatureExtractor extractor(kStepSize);
  std::vector<float> output(kStepSize + 1);
  // First frame.
  for (int i = 0; i < kStepSize; i += kBlockSize) {
    ArrayView<const float, kBlockSize> block(&noise1_scaled[i], kBlockSize);
    this->UpdateBlock(extractor, block, num_channels);
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  for (auto input_type : kExpectedInputs) {
    extractor.PrepareModelInput(output, input_type);
  }
  EXPECT_FALSE(extractor.ReadyForInference());

  for (int i = kStepSize; i < 2 * kStepSize; i += kBlockSize) {
    ArrayView<const float, kBlockSize> block(&noise1_scaled[i], kBlockSize);
    this->UpdateBlock(extractor, block, num_channels);
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  for (auto input_type : kExpectedInputs) {
    extractor.PrepareModelInput(output, input_type);
    // Compare the output with the expected output.
    EXPECT_THAT(output, Pointwise(FloatNear(kTolerance), expected_output1));
  }
  EXPECT_FALSE(extractor.ReadyForInference());
  // Second frame.
  for (int i = 0; i < kStepSize; i += kBlockSize) {
    ArrayView<const float, kBlockSize> block(&noise2_scaled[i], kBlockSize);
    this->UpdateBlock(extractor, block, num_channels);
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  for (auto input_type : kExpectedInputs) {
    extractor.PrepareModelInput(output, input_type);
  }

  for (int i = kStepSize; i < 2 * kStepSize; i += kBlockSize) {
    ArrayView<const float, kBlockSize> block(&noise2_scaled[i], kBlockSize);
    this->UpdateBlock(extractor, block, num_channels);
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  for (auto input_type : kExpectedInputs) {
    extractor.PrepareModelInput(output, input_type);
    // Compare the output with the expected output.
    EXPECT_THAT(output, Pointwise(FloatNear(kTolerance), expected_output2));
  }
}

TEST_P(FrequencyDomainFeatureExtractorTest, ResetsState) {
  const int num_channels = this->GetParam();
  std::array<float, kBlockSize> block{};
  FrequencyDomainFeatureExtractor extractor(kStepSize);
  EXPECT_FALSE(extractor.ReadyForInference());
  for (int i = 0; i < kStepSize; i += kBlockSize) {
    this->UpdateBlock(extractor, block, num_channels);
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  extractor.Reset();
  EXPECT_FALSE(extractor.ReadyForInference());
}

INSTANTIATE_TEST_SUITE_P(NumChannels,
                         FrequencyDomainFeatureExtractorTest,
                         ::testing::Values(1, 2));

TEST(TimeDomainFeatureExtractorTest, BasicTest) {
  TimeDomainFeatureExtractor extractor(kStepSize);
  std::vector<float> model_input(kStepSize, 0.f);

  // Create two distinct blocks of data.
  std::array<float, kBlockSize> block1;
  std::array<float, kBlockSize> block2;
  for (size_t i = 0; i < kBlockSize; ++i) {
    block1[i] = i;
    block2[i] = i + kBlockSize;
  }
  ArrayView<const float, kBlockSize> block1_view(block1);
  ArrayView<const ArrayView<const float, kBlockSize>> all_blocks1(&block1_view,
                                                                  1);
  ArrayView<const float, kBlockSize> block2_view(block2);
  ArrayView<const ArrayView<const float, kBlockSize>> all_blocks2(&block2_view,
                                                                  1);

  for (auto input_type : kExpectedInputs) {
    extractor.UpdateBuffers(all_blocks1, input_type);
    extractor.UpdateBuffers(all_blocks2, input_type);
  }

  EXPECT_TRUE(extractor.ReadyForInference());
  for (auto input_type : kExpectedInputs) {
    extractor.PrepareModelInput(model_input, input_type);

    // Verify that the model input contains the scaled data from both blocks.
    constexpr float kScaling = 1.0f / 32768;
    for (int i = 0; i < kStepSize; ++i) {
      EXPECT_FLOAT_EQ(model_input[i], i * kScaling);
    }
  }
  EXPECT_FALSE(extractor.ReadyForInference());
}

TEST(TimeDomainFeatureExtractorTest, ResetsState) {
  TimeDomainFeatureExtractor extractor(kStepSize);
  const std::array<float, kBlockSize> block{};
  const std::array<const ArrayView<const float, kBlockSize>, 1> all_blocks = {
      block};
  EXPECT_FALSE(extractor.ReadyForInference());
  for (size_t i = 0; i < kStepSize / kBlockSize; ++i) {
    for (auto input_type : kExpectedInputs) {
      extractor.UpdateBuffers(all_blocks, input_type);
    }
  }
  EXPECT_TRUE(extractor.ReadyForInference());
  extractor.Reset();
  EXPECT_FALSE(extractor.ReadyForInference());
}

}  // namespace
}  // namespace webrtc
