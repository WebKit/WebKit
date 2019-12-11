/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/frame_analyzer/reference_less_video_analysis_lib.h"

#include <stdio.h>

#include <numeric>
#include <vector>

#include "api/video/video_frame_buffer.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"

#define STATS_LINE_LENGTH 28
#define PSNR_FREEZE_THRESHOLD 47
#define SSIM_FREEZE_THRESHOLD .999

#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#endif

bool frozen_frame(std::vector<double> psnr_per_frame,
                  std::vector<double> ssim_per_frame,
                  size_t frame) {
  if (psnr_per_frame[frame] >= PSNR_FREEZE_THRESHOLD ||
      ssim_per_frame[frame] >= SSIM_FREEZE_THRESHOLD)
    return true;
  return false;
}

std::vector<int> find_frame_clusters(
    const std::vector<double>& psnr_per_frame,
    const std::vector<double>& ssim_per_frame) {
  std::vector<int> identical_frame_clusters;
  int num_frozen = 0;
  size_t total_no_of_frames = psnr_per_frame.size();

  for (size_t each_frame = 0; each_frame < total_no_of_frames; each_frame++) {
    if (frozen_frame(psnr_per_frame, ssim_per_frame, each_frame)) {
      num_frozen++;
    } else if (num_frozen > 0) {
      // Not frozen anymore.
      identical_frame_clusters.push_back(num_frozen);
      num_frozen = 0;
    }
  }
  return identical_frame_clusters;
}

void print_freezing_metrics(const std::vector<double>& psnr_per_frame,
                            const std::vector<double>& ssim_per_frame) {
  /*
   * Prints the different metrics mainly:
   * 1) Identical frame number, PSNR and SSIM values.
   * 2) Length of continuous frozen frames.
   * 3) Max length of continuous freezed frames.
   * 4) No of unique frames found.
   * 5) Total different identical frames found.
   *
   * Sample output:
   *  Printing metrics for file: /src/rtc_tools/test_3.y4m
      =============================
      Total number of frames received: 74
      Total identical frames: 5
      Number of unique frames: 69
      Printing Identical Frames:
        Frame Number: 29 PSNR: 48.000000 SSIM: 0.999618
        Frame Number: 30 PSNR: 48.000000 SSIM: 0.999898
        Frame Number: 60 PSNR: 48.000000 SSIM: 0.999564
        Frame Number: 64 PSNR: 48.000000 SSIM: 0.999651
        Frame Number: 69 PSNR: 48.000000 SSIM: 0.999684
      Print identical frame which appears in clusters :
        2 1 1 1
   *
   */
  size_t total_no_of_frames = psnr_per_frame.size();
  std::vector<int> identical_frame_clusters =
      find_frame_clusters(psnr_per_frame, ssim_per_frame);
  int total_identical_frames = std::accumulate(
      identical_frame_clusters.begin(), identical_frame_clusters.end(), 0);
  size_t unique_frames = total_no_of_frames - total_identical_frames;

  printf("Total number of frames received: %zu\n", total_no_of_frames);
  printf("Total identical frames: %d\n", total_identical_frames);
  printf("Number of unique frames: %zu\n", unique_frames);

  printf("Printing Identical Frames: \n");
  for (size_t frame = 0; frame < total_no_of_frames; frame++) {
    if (frozen_frame(psnr_per_frame, ssim_per_frame, frame)) {
      printf("  Frame Number: %zu PSNR: %f SSIM: %f \n", frame,
             psnr_per_frame[frame], ssim_per_frame[frame]);
    }
  }

  printf("Print identical frame which appears in clusters : \n");
  for (int cluster = 0;
       cluster < static_cast<int>(identical_frame_clusters.size()); cluster++)
    printf("%d ", identical_frame_clusters[cluster]);
  printf("\n");
}

void compute_metrics(const rtc::scoped_refptr<webrtc::test::Video>& video,
                     std::vector<double>* psnr_per_frame,
                     std::vector<double>* ssim_per_frame) {
  for (size_t i = 0; i < video->number_of_frames() - 1; ++i) {
    const rtc::scoped_refptr<webrtc::I420BufferInterface> current_frame =
        video->GetFrame(i);
    const rtc::scoped_refptr<webrtc::I420BufferInterface> next_frame =
        video->GetFrame(i + 1);
    double result_psnr = webrtc::test::Psnr(current_frame, next_frame);
    double result_ssim = webrtc::test::Ssim(current_frame, next_frame);

    psnr_per_frame->push_back(result_psnr);
    ssim_per_frame->push_back(result_ssim);
  }
}

int run_analysis(const std::string& video_file) {
  std::vector<double> psnr_per_frame;
  std::vector<double> ssim_per_frame;
  rtc::scoped_refptr<webrtc::test::Video> video =
      webrtc::test::OpenY4mFile(video_file);
  if (video) {
    compute_metrics(video, &psnr_per_frame, &ssim_per_frame);
  } else {
    return -1;
  }
  printf("=============================\n");
  printf("Printing metrics for file: %s\n", video_file.c_str());
  printf("=============================\n");
  print_freezing_metrics(psnr_per_frame, ssim_per_frame);
  return 0;
}
