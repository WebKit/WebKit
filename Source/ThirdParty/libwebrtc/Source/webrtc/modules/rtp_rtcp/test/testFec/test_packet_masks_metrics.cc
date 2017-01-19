/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * The purpose of this test is to compute metrics to characterize the properties
 * and efficiency of the packets masks used in the generic XOR FEC code.
 *
 * The metrics measure the efficiency (recovery potential or residual loss) of
 * the FEC code, under various statistical loss models for the packet/symbol
 * loss events. Various constraints on the behavior of these metrics are
 * verified, and compared to the reference RS (Reed-Solomon) code. This serves
 * in some way as a basic check/benchmark for the packet masks.
 *
 * By an FEC code, we mean an erasure packet/symbol code, characterized by:
 * (1) The code size parameters (k,m), where k = number of source/media packets,
 * and m = number of FEC packets,
 * (2) The code type: XOR or RS.
 * In the case of XOR, the residual loss is determined via the set of packet
 * masks (generator matrix). In the case of RS, the residual loss is determined
 * directly from the MDS (maximum distance separable) property of RS.
 *
 * Currently two classes of packets masks are available (random type and bursty
 * type), so three codes are considered below: RS, XOR-random, and XOR-bursty.
 * The bursty class is defined up to k=12, so (k=12,m=12) is largest code size
 * considered in this test.
 *
 * The XOR codes are defined via the RFC 5109 and correspond to the class of
 * LDGM (low density generator matrix) codes, which is a subset of the LDPC
 * (low density parity check) codes. Future implementation will consider
 * extending our XOR codes to include LDPC codes, which explicitly include
 * protection of FEC packets.
 *
 * The type of packet/symbol loss models considered in this test are:
 * (1) Random loss: Bernoulli process, characterized by the average loss rate.
 * (2) Bursty loss: Markov chain (Gilbert-Elliot model), characterized by two
 * parameters: average loss rate and average burst length.
*/

#include <math.h>

#include <memory>

#include "webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "webrtc/modules/rtp_rtcp/test/testFec/average_residual_loss_xor_codes.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

// Maximum number of media packets allows for XOR (RFC 5109) code.
enum { kMaxNumberMediaPackets = 48 };

// Maximum number of media packets allowed for each mask type.
const uint16_t kMaxMediaPackets[] = {kMaxNumberMediaPackets, 12};

// Maximum gap size for characterizing the consecutiveness of the loss.
const int kMaxGapSize = 2 * kMaxMediaPacketsTest;

// Number of gap levels written to file/output.
const int kGapSizeOutput = 5;

// Maximum number of states for characterizing the residual loss distribution.
const int kNumStatesDistribution = 2 * kMaxMediaPacketsTest * kMaxGapSize + 1;

// The code type.
enum CodeType {
  xor_random_code,    // XOR with random mask type.
  xor_bursty_code,    // XOR with bursty mask type.
  rs_code             // Reed_solomon.
};

// The code size parameters.
struct CodeSizeParams {
  int num_media_packets;
  int num_fec_packets;
  // Protection level: num_fec_packets / (num_media_packets + num_fec_packets).
  float protection_level;
  // Number of loss configurations, for a given loss number and gap number.
  // The gap number refers to the maximum gap/hole of a loss configuration
  // (used to measure the "consecutiveness" of the loss).
  int configuration_density[kNumStatesDistribution];
};

// The type of loss models.
enum LossModelType {
  kRandomLossModel,
  kBurstyLossModel
};

struct LossModel {
  LossModelType loss_type;
  float average_loss_rate;
  float average_burst_length;
};

// Average loss rates.
const float kAverageLossRate[] = { 0.025f, 0.05f, 0.1f, 0.25f };

// Average burst lengths. The case of |kAverageBurstLength = 1.0| refers to
// the random model. Note that for the random (Bernoulli) model, the average
// burst length is determined by the average loss rate, i.e.,
// AverageBurstLength = 1 / (1 - AverageLossRate) for random model.
const float kAverageBurstLength[] = { 1.0f, 2.0f, 4.0f };

// Total number of loss models: For each burst length case, there are
// a number of models corresponding to the loss rates.
const int kNumLossModels =  (sizeof(kAverageBurstLength) /
    sizeof(*kAverageBurstLength)) * (sizeof(kAverageLossRate) /
        sizeof(*kAverageLossRate));

// Thresholds on the average loss rate of the packet loss model, below which
// certain properties of the codes are expected.
float loss_rate_upper_threshold = 0.20f;
float loss_rate_lower_threshold = 0.025f;

// Set of thresholds on the expected average recovery rate, for each code type.
// These are global thresholds for now; in future version we may condition them
// on the code length/size and protection level.
const float kRecoveryRateXorRandom[3] = { 0.94f, 0.50f, 0.19f };
const float kRecoveryRateXorBursty[3] = { 0.90f, 0.54f, 0.22f };

// Metrics for a given FEC code; each code is defined by the code type
// (RS, XOR-random/bursty), and the code size parameters (k,m), where
// k = num_media_packets, m = num_fec_packets.
struct MetricsFecCode {
  // The average and variance of the residual loss, as a function of the
  // packet/symbol loss model. The average/variance is computed by averaging
  // over all loss configurations wrt the loss probability given by the
  // underlying loss model.
  double average_residual_loss[kNumLossModels];
  double variance_residual_loss[kNumLossModels];
  // The residual loss, as a function of the loss number and the gap number of
  // the loss configurations. The gap number refers to the maximum gap/hole of
  // a loss configuration (used to measure the "consecutiveness" of the loss).
  double residual_loss_per_loss_gap[kNumStatesDistribution];
  // The recovery rate as a function of the loss number.
  double recovery_rate_per_loss[2 * kMaxMediaPacketsTest + 1];
};

MetricsFecCode kMetricsXorRandom[kNumberCodes];
MetricsFecCode kMetricsXorBursty[kNumberCodes];
MetricsFecCode kMetricsReedSolomon[kNumberCodes];

class FecPacketMaskMetricsTest : public ::testing::Test {
 protected:
  FecPacketMaskMetricsTest() { }

  int max_num_codes_;
  LossModel loss_model_[kNumLossModels];
  CodeSizeParams code_params_[kNumberCodes];

  uint8_t fec_packet_masks_[kMaxNumberMediaPackets][kMaxNumberMediaPackets];
  FILE* fp_mask_;

  // Measure of the gap of the loss for configuration given by |state|.
  // This is to measure degree of consecutiveness for the loss configuration.
  // Useful if the packets are sent out in order of sequence numbers and there
  // is little/no re-ordering during transmission.
  int GapLoss(int tot_num_packets, uint8_t* state) {
    int max_gap_loss = 0;
    // Find the first loss.
    int first_loss = 0;
    for (int i = 0; i < tot_num_packets; i++) {
      if (state[i] == 1) {
        first_loss = i;
        break;
      }
    }
    int prev_loss = first_loss;
    for (int i = first_loss + 1; i < tot_num_packets; i++) {
      if (state[i] == 1) {  // Lost state.
        int gap_loss = (i - prev_loss) - 1;
        if (gap_loss > max_gap_loss) {
          max_gap_loss = gap_loss;
        }
        prev_loss = i;
      }
    }
    return max_gap_loss;
  }

  // Returns the number of recovered media packets for the XOR code, given the
  // packet mask |fec_packet_masks_|, for the loss state/configuration given by
  // |state|.
  int RecoveredMediaPackets(int num_media_packets,
                            int num_fec_packets,
                            uint8_t* state) {
    std::unique_ptr<uint8_t[]> state_tmp(
        new uint8_t[num_media_packets + num_fec_packets]);
    memcpy(state_tmp.get(), state, num_media_packets + num_fec_packets);
    int num_recovered_packets = 0;
    bool loop_again = true;
    while (loop_again) {
      loop_again = false;
      bool recovered_new_packet = false;
      // Check if we can recover anything: loop over all possible FEC packets.
      for (int i = 0; i < num_fec_packets; i++) {
        if (state_tmp[i + num_media_packets] == 0) {
          // We have this FEC packet.
          int num_packets_in_mask = 0;
          int num_received_packets_in_mask = 0;
          for (int j = 0; j < num_media_packets; j++) {
            if (fec_packet_masks_[i][j] == 1) {
              num_packets_in_mask++;
              if (state_tmp[j] == 0) {
                num_received_packets_in_mask++;
              }
            }
          }
          if ((num_packets_in_mask - 1) == num_received_packets_in_mask) {
            // We can recover the missing media packet for this FEC packet.
            num_recovered_packets++;
            recovered_new_packet = true;
            int jsel = -1;
            int check_num_recovered = 0;
            // Update the state with newly recovered media packet.
            for (int j = 0; j < num_media_packets; j++) {
              if (fec_packet_masks_[i][j] == 1 && state_tmp[j] == 1) {
                // This is the lost media packet we will recover.
                jsel = j;
                check_num_recovered++;
              }
            }
            // Check that we can only recover 1 packet.
            assert(check_num_recovered == 1);
            // Update the state with the newly recovered media packet.
            state_tmp[jsel] = 0;
          }
        }
      }  // Go to the next FEC packet in the loop.
      // If we have recovered at least one new packet in this FEC loop,
      // go through loop again, otherwise we leave loop.
      if (recovered_new_packet) {
        loop_again = true;
      }
    }
    return num_recovered_packets;
  }

  // Compute the probability of occurence of the loss state/configuration,
  // given by |state|, for all the loss models considered in this test.
  void ComputeProbabilityWeight(double* prob_weight,
                                uint8_t* state,
                                int tot_num_packets) {
    // Loop over the loss models.
    for (int k = 0; k < kNumLossModels; k++) {
      double loss_rate = static_cast<double>(
          loss_model_[k].average_loss_rate);
      double burst_length = static_cast<double>(
          loss_model_[k].average_burst_length);
      double result = 1.0;
      if (loss_model_[k].loss_type == kRandomLossModel) {
        for (int i = 0; i < tot_num_packets; i++) {
          if (state[i] == 0) {
            result *= (1.0 - loss_rate);
          } else {
            result *= loss_rate;
          }
        }
      } else {  // Gilbert-Elliot model for burst model.
        assert(loss_model_[k].loss_type == kBurstyLossModel);
        // Transition probabilities: from previous to current state.
        // Prob. of previous = lost --> current = received.
        double prob10 = 1.0 / burst_length;
        // Prob. of previous = lost --> currrent = lost.
        double prob11 = 1.0 - prob10;
        // Prob. of previous = received --> current = lost.
        double prob01 = prob10 * (loss_rate / (1.0 - loss_rate));
        // Prob. of previous = received --> current = received.
        double prob00 = 1.0 - prob01;

        // Use stationary probability for first state/packet.
        if (state[0] == 0) {  // Received
          result = (1.0 - loss_rate);
        } else {   // Lost
          result = loss_rate;
        }

        // Subsequent states: use transition probabilities.
        for (int i = 1; i < tot_num_packets; i++) {
          // Current state is received
          if (state[i] == 0) {
            if (state[i-1] == 0) {
              result *= prob00;   // Previous received, current received.
              } else {
                result *= prob10;  // Previous lost, current received.
              }
          } else {  // Current state is lost
            if (state[i-1] == 0) {
              result *= prob01;  // Previous received, current lost.
            } else {
              result *= prob11;  // Previous lost, current lost.
            }
          }
        }
      }
      prob_weight[k] = result;
    }
  }

  void CopyMetrics(MetricsFecCode* metrics_output,
                   MetricsFecCode metrics_input) {
    memcpy(metrics_output->average_residual_loss,
           metrics_input.average_residual_loss,
           sizeof(double) * kNumLossModels);
    memcpy(metrics_output->variance_residual_loss,
           metrics_input.variance_residual_loss,
           sizeof(double) * kNumLossModels);
    memcpy(metrics_output->residual_loss_per_loss_gap,
           metrics_input.residual_loss_per_loss_gap,
           sizeof(double) * kNumStatesDistribution);
    memcpy(metrics_output->recovery_rate_per_loss,
           metrics_input.recovery_rate_per_loss,
           sizeof(double) * 2 * kMaxMediaPacketsTest);
  }

  // Compute the residual loss per gap, by summing the
  // |residual_loss_per_loss_gap| over all loss configurations up to loss number
  // = |num_fec_packets|.
  double ComputeResidualLossPerGap(MetricsFecCode metrics,
                                   int gap_number,
                                   int num_fec_packets,
                                   int code_index) {
    double residual_loss_gap = 0.0;
    int tot_num_configs = 0;
    for (int loss = 1; loss <= num_fec_packets; loss++) {
      int index = gap_number * (2 * kMaxMediaPacketsTest) + loss;
      residual_loss_gap += metrics.residual_loss_per_loss_gap[index];
      tot_num_configs +=
          code_params_[code_index].configuration_density[index];
    }
    // Normalize, to compare across code sizes.
    if (tot_num_configs > 0) {
      residual_loss_gap = residual_loss_gap /
          static_cast<double>(tot_num_configs);
    }
    return residual_loss_gap;
  }

  // Compute the recovery rate per loss number, by summing the
  // |residual_loss_per_loss_gap| over all gap configurations.
  void ComputeRecoveryRatePerLoss(MetricsFecCode* metrics,
                                  int num_media_packets,
                                  int num_fec_packets,
                                  int code_index) {
    for (int loss = 1; loss <= num_media_packets + num_fec_packets; loss++) {
      metrics->recovery_rate_per_loss[loss] = 0.0;
      int tot_num_configs = 0;
      double arl = 0.0;
      for (int gap = 0; gap < kMaxGapSize; gap ++) {
        int index = gap * (2 * kMaxMediaPacketsTest) + loss;
        arl += metrics->residual_loss_per_loss_gap[index];
        tot_num_configs +=
            code_params_[code_index].configuration_density[index];
      }
      // Normalize, to compare across code sizes.
      if (tot_num_configs > 0) {
        arl = arl / static_cast<double>(tot_num_configs);
      }
      // Recovery rate for a given loss |loss| is 1 minus the scaled |arl|,
      // where the scale factor is relative to code size/parameters.
      double scaled_loss = static_cast<double>(loss * num_media_packets) /
          static_cast<double>(num_media_packets + num_fec_packets);
      metrics->recovery_rate_per_loss[loss] = 1.0 - arl / scaled_loss;
    }
  }

  void SetMetricsZero(MetricsFecCode* metrics) {
    memset(metrics->average_residual_loss, 0, sizeof(double) * kNumLossModels);
    memset(metrics->variance_residual_loss, 0, sizeof(double) * kNumLossModels);
    memset(metrics->residual_loss_per_loss_gap, 0,
           sizeof(double) * kNumStatesDistribution);
    memset(metrics->recovery_rate_per_loss, 0,
           sizeof(double) * 2 * kMaxMediaPacketsTest + 1);
  }

  // Compute the metrics for an FEC code, given by the code type |code_type|
  // (XOR-random/ bursty or RS), and by the code index |code_index|
  // (which containes the code size parameters/protection length).
  void ComputeMetricsForCode(CodeType code_type,
                             int code_index) {
    std::unique_ptr<double[]> prob_weight(new double[kNumLossModels]);
    memset(prob_weight.get() , 0, sizeof(double) * kNumLossModels);
    MetricsFecCode metrics_code;
    SetMetricsZero(&metrics_code);

    int num_media_packets = code_params_[code_index].num_media_packets;
    int num_fec_packets = code_params_[code_index].num_fec_packets;
    int tot_num_packets = num_media_packets + num_fec_packets;
    std::unique_ptr<uint8_t[]> state(new uint8_t[tot_num_packets]);
    memset(state.get() , 0, tot_num_packets);

    int num_loss_configurations = static_cast<int>(pow(2.0f, tot_num_packets));
    // Loop over all loss configurations for the symbol sequence of length
    // |tot_num_packets|. In this version we process up to (k=12, m=12) codes,
    // and get exact expressions for the residual loss.
    // TODO(marpan): For larger codes, loop over some random sample of loss
    // configurations, sampling driven by the underlying statistical loss model
    // (importance sampling).

    // The symbols/packets are arranged as a sequence of source/media packets
    // followed by FEC packets. This is the sequence ordering used in the RTP.
    // A configuration refers to a sequence of received/lost (0/1 bit) states
    // for the string of packets/symbols. For example, for a (k=4,m=3) code
    // (4 media packets, 3 FEC packets), with 2 losses (one media and one FEC),
    // the loss configurations is:
    // Media1   Media2   Media3   Media4   FEC1   FEC2   FEC3
    //   0         0        1       0        0      1     0
    for (int i = 1; i < num_loss_configurations; i++) {
      // Counter for number of packets lost.
      int num_packets_lost = 0;
      // Counters for the number of media packets lost.
      int num_media_packets_lost = 0;

      // Map configuration number to a loss state.
      for (int j = 0; j < tot_num_packets; j++) {
        state[j] = 0;  // Received state.
        int bit_value = i >> (tot_num_packets - j - 1) & 1;
        if (bit_value == 1) {
          state[j] = 1;  // Lost state.
          num_packets_lost++;
           if (j < num_media_packets) {
             num_media_packets_lost++;
           }
        }
      }  // Done with loop over total number of packets.
      assert(num_media_packets_lost <= num_media_packets);
      assert(num_packets_lost <= tot_num_packets && num_packets_lost > 0);
      double residual_loss = 0.0;
      // Only need to compute residual loss (number of recovered packets) for
      // configurations that have at least one media packet lost.
      if (num_media_packets_lost >= 1) {
        // Compute the number of recovered packets.
        int num_recovered_packets = 0;
        if (code_type == xor_random_code || code_type == xor_bursty_code) {
          num_recovered_packets = RecoveredMediaPackets(num_media_packets,
                                                        num_fec_packets,
                                                        state.get());
        } else {
          // For the RS code, we can either completely recover all the packets
          // if the loss is less than or equal to the number of FEC packets,
          // otherwise we can recover none of the missing packets. This is the
          // all or nothing (MDS) property of the RS code.
          if (num_packets_lost <= num_fec_packets) {
            num_recovered_packets = num_media_packets_lost;
          }
        }
        assert(num_recovered_packets <= num_media_packets);
        // Compute the residual loss. We only care about recovering media/source
        // packets, so residual loss is based on lost/recovered media packets.
        residual_loss = static_cast<double>(num_media_packets_lost -
                                            num_recovered_packets);
        // Compute the probability weights for this configuration.
        ComputeProbabilityWeight(prob_weight.get(),
                                 state.get(),
                                 tot_num_packets);
        // Update the average and variance of the residual loss.
        for (int k = 0; k < kNumLossModels; k++) {
          metrics_code.average_residual_loss[k] += residual_loss *
              prob_weight[k];
          metrics_code.variance_residual_loss[k] += residual_loss *
              residual_loss * prob_weight[k];
        }
      }  // Done with processing for num_media_packets_lost >= 1.
      // Update the distribution statistics.
      // Compute the gap of the loss (the "consecutiveness" of the loss).
      int gap_loss = GapLoss(tot_num_packets, state.get());
      assert(gap_loss < kMaxGapSize);
      int index = gap_loss * (2 * kMaxMediaPacketsTest) + num_packets_lost;
      assert(index < kNumStatesDistribution);
      metrics_code.residual_loss_per_loss_gap[index] += residual_loss;
      if (code_type == xor_random_code) {
        // The configuration density is only a function of the code length and
        // only needs to computed for the first |code_type| passed here.
        code_params_[code_index].configuration_density[index]++;
      }
    }  // Done with loop over configurations.
    // Normalize the average residual loss and compute/normalize the variance.
    for (int k = 0; k < kNumLossModels; k++) {
      // Normalize the average residual loss by the total number of packets
      // |tot_num_packets| (i.e., the code length). For a code with no (zero)
      // recovery, the average residual loss for that code would be reduced like
      // ~|average_loss_rate| * |num_media_packets| / |tot_num_packets|. This is
      // the expected reduction in the average residual loss just from adding
      // FEC packets to the symbol sequence.
      metrics_code.average_residual_loss[k] =
          metrics_code.average_residual_loss[k] /
          static_cast<double>(tot_num_packets);
      metrics_code.variance_residual_loss[k] =
               metrics_code.variance_residual_loss[k] /
               static_cast<double>(num_media_packets * num_media_packets);
      metrics_code.variance_residual_loss[k] =
          metrics_code.variance_residual_loss[k] -
          (metrics_code.average_residual_loss[k] *
              metrics_code.average_residual_loss[k]);
      assert(metrics_code.variance_residual_loss[k] >= 0.0);
      assert(metrics_code.average_residual_loss[k] > 0.0);
      metrics_code.variance_residual_loss[k] =
          sqrt(metrics_code.variance_residual_loss[k]) /
          metrics_code.average_residual_loss[k];
    }

    // Compute marginal distribution as a function of loss parameter.
    ComputeRecoveryRatePerLoss(&metrics_code,
                               num_media_packets,
                               num_fec_packets,
                               code_index);
    if (code_type == rs_code) {
      CopyMetrics(&kMetricsReedSolomon[code_index], metrics_code);
    } else if (code_type == xor_random_code) {
      CopyMetrics(&kMetricsXorRandom[code_index], metrics_code);
    } else if (code_type == xor_bursty_code) {
      CopyMetrics(&kMetricsXorBursty[code_index], metrics_code);
    } else {
      assert(false);
    }
  }

  void WriteOutMetricsAllFecCodes()  {
    std::string filename = test::OutputPath() + "data_metrics_all_codes";
    FILE* fp = fopen(filename.c_str(), "wb");
    // Loop through codes up to |kMaxMediaPacketsTest|.
    int code_index = 0;
    for (int num_media_packets = 1; num_media_packets <= kMaxMediaPacketsTest;
        num_media_packets++) {
      for (int num_fec_packets = 1; num_fec_packets <= num_media_packets;
          num_fec_packets++) {
        fprintf(fp, "FOR CODE: (%d, %d) \n", num_media_packets,
                num_fec_packets);
        for (int k = 0; k < kNumLossModels; k++) {
          float loss_rate = loss_model_[k].average_loss_rate;
          float burst_length = loss_model_[k].average_burst_length;
          fprintf(fp, "Loss rate = %.2f, Burst length = %.2f:  %.4f  %.4f  %.4f"
              " **** %.4f %.4f %.4f \n",
              loss_rate,
              burst_length,
              100 * kMetricsReedSolomon[code_index].average_residual_loss[k],
              100 * kMetricsXorRandom[code_index].average_residual_loss[k],
              100 * kMetricsXorBursty[code_index].average_residual_loss[k],
              kMetricsReedSolomon[code_index].variance_residual_loss[k],
              kMetricsXorRandom[code_index].variance_residual_loss[k],
              kMetricsXorBursty[code_index].variance_residual_loss[k]);
        }
        for (int gap = 0; gap < kGapSizeOutput; gap ++) {
          double rs_residual_loss = ComputeResidualLossPerGap(
              kMetricsReedSolomon[code_index],
              gap,
              num_fec_packets,
              code_index);
          double xor_random_residual_loss = ComputeResidualLossPerGap(
              kMetricsXorRandom[code_index],
              gap,
              num_fec_packets,
              code_index);
          double xor_bursty_residual_loss = ComputeResidualLossPerGap(
              kMetricsXorBursty[code_index],
              gap,
              num_fec_packets,
              code_index);
          fprintf(fp, "Residual loss as a function of gap "
              "%d: %.4f %.4f %.4f \n",
              gap,
              rs_residual_loss,
              xor_random_residual_loss,
              xor_bursty_residual_loss);
        }
        fprintf(fp, "Recovery rate as a function of loss number \n");
        for (int loss = 1; loss <= num_media_packets + num_fec_packets;
                     loss ++) {
          fprintf(fp, "For loss number %d: %.4f %.4f %.4f \n",
                  loss,
                  kMetricsReedSolomon[code_index].
                  recovery_rate_per_loss[loss],
                  kMetricsXorRandom[code_index].
                  recovery_rate_per_loss[loss],
                  kMetricsXorBursty[code_index].
                  recovery_rate_per_loss[loss]);
        }
        fprintf(fp, "******************\n");
        fprintf(fp, "\n");
        code_index++;
      }
    }
    fclose(fp);
  }

  void SetLossModels() {
    int num_loss_rates = sizeof(kAverageLossRate) /
        sizeof(*kAverageLossRate);
    int num_burst_lengths = sizeof(kAverageBurstLength) /
        sizeof(*kAverageBurstLength);
    int num_loss_models = 0;
    for (int k = 0; k < num_burst_lengths; k++) {
      for (int k2 = 0; k2 < num_loss_rates; k2++) {
        loss_model_[num_loss_models].average_loss_rate = kAverageLossRate[k2];
        loss_model_[num_loss_models].average_burst_length =
            kAverageBurstLength[k];
        // First set of loss models are of random type.
        if (k == 0) {
          loss_model_[num_loss_models].loss_type = kRandomLossModel;
        } else {
          loss_model_[num_loss_models].loss_type = kBurstyLossModel;
        }
        num_loss_models++;
      }
    }
    assert(num_loss_models == kNumLossModels);
  }

  void SetCodeParams() {
    int code_index = 0;
    for (int num_media_packets = 1; num_media_packets <= kMaxMediaPacketsTest;
        num_media_packets++) {
      for (int num_fec_packets = 1; num_fec_packets <= num_media_packets;
          num_fec_packets++) {
        code_params_[code_index].num_media_packets = num_media_packets;
        code_params_[code_index].num_fec_packets = num_fec_packets;
        code_params_[code_index].protection_level =
            static_cast<float>(num_fec_packets) /
            static_cast<float>(num_media_packets + num_fec_packets);
        for (int k = 0; k < kNumStatesDistribution; k++) {
          code_params_[code_index].configuration_density[k] = 0;
        }
        code_index++;
      }
    }
    max_num_codes_ = code_index;
  }

  // Make some basic checks on the packet masks. Return -1 if any of these
  // checks fail.
  int RejectInvalidMasks(int num_media_packets, int num_fec_packets) {
    // Make sure every FEC packet protects something.
    for (int i = 0; i < num_fec_packets; i++) {
      int row_degree = 0;
      for (int j = 0; j < num_media_packets; j++) {
        if (fec_packet_masks_[i][j] == 1) {
          row_degree++;
        }
      }
      if (row_degree == 0) {
        printf("Invalid mask: FEC packet has empty mask (does not protect "
            "anything) %d %d %d \n", i, num_media_packets, num_fec_packets);
        return -1;
      }
    }
    // Mask sure every media packet has some protection.
    for (int j = 0; j < num_media_packets; j++) {
      int column_degree = 0;
      for (int i = 0; i < num_fec_packets; i++) {
        if (fec_packet_masks_[i][j] == 1) {
          column_degree++;
        }
      }
      if (column_degree == 0) {
        printf("Invalid mask: Media packet has no protection at all %d %d %d "
            "\n", j, num_media_packets, num_fec_packets);
        return -1;
      }
    }
    // Make sure we do not have two identical FEC packets.
    for (int i = 0; i < num_fec_packets; i++) {
      for (int i2 = i + 1; i2 < num_fec_packets; i2++) {
        int overlap = 0;
        for (int j = 0; j < num_media_packets; j++) {
          if (fec_packet_masks_[i][j] == fec_packet_masks_[i2][j]) {
            overlap++;
          }
        }
        if (overlap == num_media_packets) {
          printf("Invalid mask: Two FEC packets are identical %d %d %d %d \n",
                 i, i2, num_media_packets, num_fec_packets);
          return -1;
        }
      }
    }
    // Avoid codes that have two media packets with full protection (all 1s in
    // their corresponding columns). This would mean that if we lose those
    // two packets, we can never recover them even if we receive all the other
    // packets. Exclude the special cases of 1 or 2 FEC packets.
    if (num_fec_packets > 2) {
      for (int j = 0; j < num_media_packets; j++) {
        for (int j2 = j + 1; j2 < num_media_packets; j2++) {
          int degree = 0;
          for (int i = 0; i < num_fec_packets; i++) {
            if (fec_packet_masks_[i][j] == fec_packet_masks_[i][j2] &&
                fec_packet_masks_[i][j] == 1) {
              degree++;
            }
          }
          if (degree == num_fec_packets) {
            printf("Invalid mask: Two media packets are have full degree "
                "%d %d %d %d \n", j, j2, num_media_packets, num_fec_packets);
            return -1;
          }
        }
      }
    }
    return 0;
  }

  void GetPacketMaskConvertToBitMask(uint8_t* packet_mask,
                                     int num_media_packets,
                                     int num_fec_packets,
                                     int mask_bytes_fec_packet,
                                     CodeType code_type) {
    for (int i = 0; i < num_fec_packets; i++) {
      for (int j = 0; j < num_media_packets; j++) {
        const uint8_t byte_mask =
            packet_mask[i * mask_bytes_fec_packet + j / 8];
        const int bit_position = (7 - j % 8);
        fec_packet_masks_[i][j] =
            (byte_mask & (1 << bit_position)) >> bit_position;
        fprintf(fp_mask_, "%d ", fec_packet_masks_[i][j]);
      }
      fprintf(fp_mask_, "\n");
    }
    fprintf(fp_mask_, "\n");
  }

  int ProcessXORPacketMasks(CodeType code_type,
                          FecMaskType fec_mask_type) {
    int code_index = 0;
    // Maximum number of media packets allowed for the mask type.
    const int packet_mask_max = kMaxMediaPackets[fec_mask_type];
    std::unique_ptr<uint8_t[]> packet_mask(
        new uint8_t[packet_mask_max * kUlpfecMaxPacketMaskSize]);
    // Loop through codes up to |kMaxMediaPacketsTest|.
    for (int num_media_packets = 1; num_media_packets <= kMaxMediaPacketsTest;
        num_media_packets++) {
      const int mask_bytes_fec_packet =
          static_cast<int>(internal::PacketMaskSize(num_media_packets));
      internal::PacketMaskTable mask_table(fec_mask_type, num_media_packets);
      for (int num_fec_packets = 1; num_fec_packets <= num_media_packets;
          num_fec_packets++) {
        memset(packet_mask.get(), 0, num_media_packets * mask_bytes_fec_packet);
        memcpy(packet_mask.get(),
               mask_table.fec_packet_mask_table()[num_media_packets - 1]
                                                 [num_fec_packets - 1],
               num_fec_packets * mask_bytes_fec_packet);
        // Convert to bit mask.
        GetPacketMaskConvertToBitMask(packet_mask.get(), num_media_packets,
                                      num_fec_packets, mask_bytes_fec_packet,
                                      code_type);
        if (RejectInvalidMasks(num_media_packets, num_fec_packets) < 0) {
          return -1;
        }
        // Compute the metrics for this code/mask.
        ComputeMetricsForCode(code_type,
                              code_index);
        code_index++;
      }
    }
    assert(code_index == kNumberCodes);
    return 0;
  }

  void ProcessRS(CodeType code_type) {
    int code_index = 0;
    for (int num_media_packets = 1; num_media_packets <= kMaxMediaPacketsTest;
        num_media_packets++) {
      for (int num_fec_packets = 1; num_fec_packets <= num_media_packets;
          num_fec_packets++) {
        // Compute the metrics for this code type.
        ComputeMetricsForCode(code_type,
                              code_index);
        code_index++;
      }
    }
  }

  // Compute metrics for all code types and sizes.
  void ComputeMetricsAllCodes() {
    SetLossModels();
    SetCodeParams();
    // Get metrics for XOR code with packet masks of random type.
    std::string filename = test::OutputPath() + "data_packet_masks";
    fp_mask_ = fopen(filename.c_str(), "wb");
    fprintf(fp_mask_, "MASK OF TYPE RANDOM: \n");
    EXPECT_EQ(ProcessXORPacketMasks(xor_random_code, kFecMaskRandom), 0);
    // Get metrics for XOR code with packet masks of bursty type.
    fprintf(fp_mask_, "MASK OF TYPE BURSTY: \n");
    EXPECT_EQ(ProcessXORPacketMasks(xor_bursty_code, kFecMaskBursty), 0);
    fclose(fp_mask_);
    // Get metrics for Reed-Solomon code.
    ProcessRS(rs_code);
  }
};

// Verify that the average residual loss, averaged over loss models
// appropriate to each mask type, is below some maximum acceptable level. The
// acceptable levels are read in from a file, and correspond to a current set
// of packet masks. The levels for each code may be updated over time.
TEST_F(FecPacketMaskMetricsTest, FecXorMaxResidualLoss) {
  SetLossModels();
  SetCodeParams();
  ComputeMetricsAllCodes();
  WriteOutMetricsAllFecCodes();
  int num_loss_rates = sizeof(kAverageLossRate) /
      sizeof(*kAverageLossRate);
  int num_burst_lengths = sizeof(kAverageBurstLength) /
      sizeof(*kAverageBurstLength);
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    double sum_residual_loss_random_mask_random_loss = 0.0;
    double sum_residual_loss_bursty_mask_bursty_loss = 0.0;
    // Compute the sum residual loss across the models, for each mask type.
    for (int k = 0; k < kNumLossModels; k++) {
      if (loss_model_[k].loss_type == kRandomLossModel) {
        sum_residual_loss_random_mask_random_loss +=
            kMetricsXorRandom[code_index].average_residual_loss[k];
      } else if (loss_model_[k].loss_type == kBurstyLossModel) {
        sum_residual_loss_bursty_mask_bursty_loss +=
            kMetricsXorBursty[code_index].average_residual_loss[k];
      }
    }
    float average_residual_loss_random_mask_random_loss =
        sum_residual_loss_random_mask_random_loss / num_loss_rates;
    float average_residual_loss_bursty_mask_bursty_loss =
        sum_residual_loss_bursty_mask_bursty_loss /
        (num_loss_rates * (num_burst_lengths  - 1));
    const float ref_random_mask = kMaxResidualLossRandomMask[code_index];
    const float ref_bursty_mask = kMaxResidualLossBurstyMask[code_index];
    EXPECT_LE(average_residual_loss_random_mask_random_loss, ref_random_mask);
    EXPECT_LE(average_residual_loss_bursty_mask_bursty_loss, ref_bursty_mask);
  }
}

// Verify the behavior of the XOR codes vs the RS codes.
// For random loss model with average loss rates <= the code protection level,
// the RS code (optimal MDS code) is more efficient than XOR codes.
// However, for larger loss rates (above protection level) and/or bursty
// loss models, the RS is not always more efficient than XOR (though in most
// cases it still is).
TEST_F(FecPacketMaskMetricsTest, FecXorVsRS) {
  SetLossModels();
  SetCodeParams();
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    for (int k = 0; k < kNumLossModels; k++) {
      float loss_rate = loss_model_[k].average_loss_rate;
      float protection_level = code_params_[code_index].protection_level;
      // Under these conditions we expect XOR to not be better than RS.
       if (loss_model_[k].loss_type == kRandomLossModel &&
           loss_rate <= protection_level) {
        EXPECT_GE(kMetricsXorRandom[code_index].average_residual_loss[k],
                  kMetricsReedSolomon[code_index].average_residual_loss[k]);
        EXPECT_GE(kMetricsXorBursty[code_index].average_residual_loss[k],
                  kMetricsReedSolomon[code_index].average_residual_loss[k]);
       }
       // TODO(marpan): There are some cases (for high loss rates and/or
       // burst loss models) where XOR is better than RS. Is there some pattern
       // we can identify and enforce as a constraint?
    }
  }
}

// Verify the trend (change) in the average residual loss, as a function of
// loss rate, of the XOR code relative to the RS code.
// The difference between XOR and RS should not get worse as we increase
// the average loss rate.
TEST_F(FecPacketMaskMetricsTest, FecTrendXorVsRsLossRate) {
  SetLossModels();
  SetCodeParams();
  // TODO(marpan): Examine this further to see if the condition can be strictly
  // satisfied (i.e., scale = 1.0) for all codes with different/better masks.
  double scale = 0.90;
  int num_loss_rates = sizeof(kAverageLossRate) /
      sizeof(*kAverageLossRate);
  int num_burst_lengths = sizeof(kAverageBurstLength) /
      sizeof(*kAverageBurstLength);
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    for (int i = 0; i < num_burst_lengths; i++) {
      for (int j = 0; j < num_loss_rates - 1; j++) {
        int k = num_loss_rates * i + j;
        // For XOR random.
        if (kMetricsXorRandom[code_index].average_residual_loss[k] >
        kMetricsReedSolomon[code_index].average_residual_loss[k]) {
          double diff_rs_xor_random_loss1 =
              (kMetricsXorRandom[code_index].average_residual_loss[k] -
               kMetricsReedSolomon[code_index].average_residual_loss[k]) /
               kMetricsXorRandom[code_index].average_residual_loss[k];
          double diff_rs_xor_random_loss2 =
              (kMetricsXorRandom[code_index].average_residual_loss[k+1] -
               kMetricsReedSolomon[code_index].average_residual_loss[k+1]) /
               kMetricsXorRandom[code_index].average_residual_loss[k+1];
          EXPECT_GE(diff_rs_xor_random_loss1, scale * diff_rs_xor_random_loss2);
        }
        // TODO(marpan): Investigate the cases for the bursty mask where
        // this trend is not strictly satisfied.
      }
    }
  }
}

// Verify the average residual loss behavior via the protection level and
// the code length. The average residual loss for a given (k1,m1) code
// should generally be higher than that of another code (k2,m2), which has
// either of the two conditions satisfied:
// 1) higher protection & code length at least as large: (k2+m2) >= (k1+m1),
// 2) equal protection and larger code length: (k2+m2) > (k1+m1).
// Currently does not hold for some cases of the XOR code with random mask.
TEST_F(FecPacketMaskMetricsTest, FecBehaviorViaProtectionLevelAndLength) {
  SetLossModels();
  SetCodeParams();
  for (int code_index1 = 0; code_index1 < max_num_codes_; code_index1++) {
    float protection_level1 = code_params_[code_index1].protection_level;
    int length1 = code_params_[code_index1].num_media_packets +
        code_params_[code_index1].num_fec_packets;
    for (int code_index2 = 0; code_index2 < max_num_codes_; code_index2++) {
      float protection_level2 = code_params_[code_index2].protection_level;
      int length2 = code_params_[code_index2].num_media_packets +
          code_params_[code_index2].num_fec_packets;
      // Codes with higher protection are more efficient, conditioned on the
      // length of the code (higher protection but shorter length codes are
      // generally not more efficient). For two codes with equal protection,
      // the longer code is generally more efficient. For high loss rate
      // models, this condition may be violated for some codes with equal or
      // very close protection levels. High loss rate case is excluded below.
      if ((protection_level2 > protection_level1 && length2 >= length1) ||
          (protection_level2 == protection_level1 && length2 > length1)) {
        for (int k = 0; k < kNumLossModels; k++) {
          float loss_rate = loss_model_[k].average_loss_rate;
          if (loss_rate < loss_rate_upper_threshold) {
            EXPECT_LT(
                kMetricsReedSolomon[code_index2].average_residual_loss[k],
                kMetricsReedSolomon[code_index1].average_residual_loss[k]);
            // TODO(marpan): There are some corner cases where this is not
            // satisfied with the current packet masks. Look into updating
            // these cases to see if this behavior should/can be satisfied,
            // with overall lower residual loss for those XOR codes.
            // EXPECT_LT(
            //    kMetricsXorBursty[code_index2].average_residual_loss[k],
            //    kMetricsXorBursty[code_index1].average_residual_loss[k]);
            // EXPECT_LT(
            //   kMetricsXorRandom[code_index2].average_residual_loss[k],
            //   kMetricsXorRandom[code_index1].average_residual_loss[k]);
          }
        }
      }
    }
  }
}

// Verify the beheavior of the variance of the XOR codes.
// The partial recovery of the XOR versus the all or nothing behavior of the RS
// code means that the variance of the residual loss for XOR should generally
// not be worse than RS.
TEST_F(FecPacketMaskMetricsTest, FecVarianceBehaviorXorVsRs) {
  SetLossModels();
  SetCodeParams();
  // The condition is not strictly satisfied with the current masks,
  // i.e., for some codes, the variance of XOR may be slightly higher than RS.
  // TODO(marpan): Examine this further to see if the condition can be strictly
  // satisfied (i.e., scale = 1.0) for all codes with different/better masks.
  double scale = 0.95;
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    for (int k = 0; k < kNumLossModels; k++) {
      EXPECT_LE(scale *
                kMetricsXorRandom[code_index].variance_residual_loss[k],
                kMetricsReedSolomon[code_index].variance_residual_loss[k]);
      EXPECT_LE(scale *
                kMetricsXorBursty[code_index].variance_residual_loss[k],
                kMetricsReedSolomon[code_index].variance_residual_loss[k]);
    }
  }
}

// For the bursty mask type, the residual loss must be strictly zero for all
// consecutive losses (i.e, gap = 0) with number of losses <= num_fec_packets.
// This is a design property of the bursty mask type.
TEST_F(FecPacketMaskMetricsTest, FecXorBurstyPerfectRecoveryConsecutiveLoss) {
  SetLossModels();
  SetCodeParams();
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    int num_fec_packets = code_params_[code_index].num_fec_packets;
    for (int loss = 1; loss <= num_fec_packets; loss++) {
      int index = loss;  // |gap| is zero.
      EXPECT_EQ(kMetricsXorBursty[code_index].
                residual_loss_per_loss_gap[index], 0.0);
    }
  }
}

// The XOR codes with random mask type are generally better than the ones with
// bursty mask type, for random loss models at low loss rates.
// The XOR codes with bursty mask types are generally better than the one with
// random mask type, for bursty loss models and/or high loss rates.
// TODO(marpan): Enable this test when some of the packet masks are updated.
// Some isolated cases of the codes don't pass this currently.
/*
TEST_F(FecPacketMaskMetricsTest, FecXorRandomVsBursty) {
  SetLossModels();
  SetCodeParams();
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    double sum_residual_loss_random_mask_random_loss = 0.0;
    double sum_residual_loss_bursty_mask_random_loss = 0.0;
    double sum_residual_loss_random_mask_bursty_loss = 0.0;
    double sum_residual_loss_bursty_mask_bursty_loss = 0.0;
    // Compute the sum residual loss across the models, for each mask type.
    for (int k = 0; k < kNumLossModels; k++) {
      float loss_rate = loss_model_[k].average_loss_rate;
      if (loss_model_[k].loss_type == kRandomLossModel &&
          loss_rate < loss_rate_upper_threshold) {
        sum_residual_loss_random_mask_random_loss +=
            kMetricsXorRandom[code_index].average_residual_loss[k];
        sum_residual_loss_bursty_mask_random_loss +=
            kMetricsXorBursty[code_index].average_residual_loss[k];
      } else if (loss_model_[k].loss_type == kBurstyLossModel &&
          loss_rate > loss_rate_lower_threshold) {
        sum_residual_loss_random_mask_bursty_loss +=
            kMetricsXorRandom[code_index].average_residual_loss[k];
        sum_residual_loss_bursty_mask_bursty_loss +=
            kMetricsXorBursty[code_index].average_residual_loss[k];
      }
    }
    EXPECT_LE(sum_residual_loss_random_mask_random_loss,
              sum_residual_loss_bursty_mask_random_loss);
    EXPECT_LE(sum_residual_loss_bursty_mask_bursty_loss,
              sum_residual_loss_random_mask_bursty_loss);
  }
}
*/

// Verify that the average recovery rate for each code is equal or above some
// threshold, for certain loss number conditions.
TEST_F(FecPacketMaskMetricsTest, FecRecoveryRateUnderLossConditions) {
  SetLossModels();
  SetCodeParams();
  for (int code_index = 0; code_index < max_num_codes_; code_index++) {
    int num_media_packets = code_params_[code_index].num_media_packets;
    int num_fec_packets = code_params_[code_index].num_fec_packets;
    // Perfect recovery (|recovery_rate_per_loss| == 1) is expected for
    // |loss_number| = 1, for all codes.
    int loss_number = 1;
    EXPECT_EQ(kMetricsReedSolomon[code_index].
              recovery_rate_per_loss[loss_number], 1.0);
    EXPECT_EQ(kMetricsXorRandom[code_index].
              recovery_rate_per_loss[loss_number], 1.0);
    EXPECT_EQ(kMetricsXorBursty[code_index].
              recovery_rate_per_loss[loss_number], 1.0);
    // For |loss_number| = |num_fec_packets| / 2, we expect the following:
    // Perfect recovery for RS, and recovery for XOR above the threshold.
    loss_number = num_fec_packets / 2 > 0 ? num_fec_packets / 2 : 1;
    EXPECT_EQ(kMetricsReedSolomon[code_index].
              recovery_rate_per_loss[loss_number], 1.0);
    EXPECT_GE(kMetricsXorRandom[code_index].
              recovery_rate_per_loss[loss_number], kRecoveryRateXorRandom[0]);
    EXPECT_GE(kMetricsXorBursty[code_index].
              recovery_rate_per_loss[loss_number], kRecoveryRateXorBursty[0]);
    // For |loss_number| = |num_fec_packets|, we expect the following:
    // Perfect recovery for RS, and recovery for XOR above the threshold.
    loss_number = num_fec_packets;
    EXPECT_EQ(kMetricsReedSolomon[code_index].
              recovery_rate_per_loss[loss_number], 1.0);
    EXPECT_GE(kMetricsXorRandom[code_index].
              recovery_rate_per_loss[loss_number], kRecoveryRateXorRandom[1]);
    EXPECT_GE(kMetricsXorBursty[code_index].
              recovery_rate_per_loss[loss_number], kRecoveryRateXorBursty[1]);
    // For |loss_number| = |num_fec_packets| + 1, we expect the following:
    // Zero recovery for RS, but non-zero recovery for XOR.
    if (num_fec_packets > 1 && num_media_packets > 2) {
      loss_number =  num_fec_packets + 1;
      EXPECT_EQ(kMetricsReedSolomon[code_index].
                recovery_rate_per_loss[loss_number], 0.0);
      EXPECT_GE(kMetricsXorRandom[code_index].
                recovery_rate_per_loss[loss_number],
                kRecoveryRateXorRandom[2]);
      EXPECT_GE(kMetricsXorBursty[code_index].
                recovery_rate_per_loss[loss_number],
                kRecoveryRateXorBursty[2]);
    }
  }
}

}  // namespace webrtc
