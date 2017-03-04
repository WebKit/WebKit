/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <numeric>
#include <vector>

#include "webrtc/tools/frame_analyzer/reference_less_video_analysis_lib.h"
#include "webrtc/tools/frame_analyzer/video_quality_analysis.h"

#define STATS_LINE_LENGTH 28
#define PSNR_FREEZE_THRESHOLD 47
#define SSIM_FREEZE_THRESHOLD .999

#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#endif

void get_height_width_fps(int *height, int *width, int *fps,
                          const std::string& video_file) {
  // File header looks like :
  //  YUV4MPEG2 W1280 H720 F25:1 Ip A0:0 C420mpeg2 XYSCSS=420MPEG2.
  char frame_header[STATS_LINE_LENGTH];
  FILE* input_file = fopen(video_file.c_str(), "rb");

  size_t bytes_read = fread(frame_header, 1, STATS_LINE_LENGTH - 1, input_file);

  frame_header[bytes_read] = '\0';
  std::string file_header_stats[5];
  int no_of_stats = 0;
  char *save_ptr;
  char *token = strtok_r(frame_header, " ", &save_ptr);

  while (token != NULL) {
    file_header_stats[no_of_stats++] = token;
    token = strtok_r(NULL, " ", &save_ptr);
  }

  *width = std::stoi(file_header_stats[1].erase(0, 1));
  *height = std::stoi(file_header_stats[2].erase(0, 1));
  *fps = std::stoi(file_header_stats[3].erase(0, 1));

  printf("Height: %d Width: %d fps:%d \n", *height, *width, *fps);
  fclose(input_file);
}

bool frozen_frame(std::vector<double> psnr_per_frame,
                  std::vector<double> ssim_per_frame, size_t frame) {
  if (psnr_per_frame[frame] >= PSNR_FREEZE_THRESHOLD ||
      ssim_per_frame[frame] >= SSIM_FREEZE_THRESHOLD)
    return true;
  return false;
}

std::vector<int> find_frame_clusters(const std::vector<double>& psnr_per_frame,
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
   *  Printing metrics for file: /src/webrtc/tools/test_3.y4m
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
  std::vector<int> identical_frame_clusters = find_frame_clusters(
        psnr_per_frame, ssim_per_frame);
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

void compute_metrics(const std::string& video_file_name,
                     std::vector<double>* psnr_per_frame,
                     std::vector<double>* ssim_per_frame) {
  int height = 0, width = 0, fps = 0;
  get_height_width_fps(&height, &width, &fps, video_file_name);

  int no_of_frames = 0;
  int size = webrtc::test::GetI420FrameSize(width, height);

  // Allocate buffers for test and reference frames.
  uint8_t* current_frame = new uint8_t[size];
  uint8_t* next_frame = new uint8_t[size];

  while (true) {
    if (!(webrtc::test::ExtractFrameFromY4mFile (video_file_name.c_str(),
                                                 width, height,
                                                 no_of_frames,
                                                 current_frame))) {
      break;
    }

    if (!(webrtc::test::ExtractFrameFromY4mFile (video_file_name.c_str(),
                                                 width, height,
                                                 no_of_frames + 1,
                                                 next_frame))) {
      break;
    }

    double result_psnr = webrtc::test::CalculateMetrics(webrtc::test::kPSNR,
                                                        current_frame,
                                                        next_frame,
                                                        width, height);
    double result_ssim = webrtc::test::CalculateMetrics(webrtc::test::kSSIM,
                                                        current_frame,
                                                        next_frame,
                                                        width, height);

    psnr_per_frame->push_back(result_psnr);
    ssim_per_frame->push_back(result_ssim);
    no_of_frames++;
  }
  // Cleanup.
  delete[] current_frame;
  delete[] next_frame;
}

bool check_file_extension(const std::string& video_file_name) {
  if (video_file_name.substr(video_file_name.length()-3, 3) != "y4m") {
    printf("Only y4m video file format is supported. Given: %s\n",
           video_file_name.c_str());
    return false;
  }
  return true;
}

int run_analysis(const std::string& video_file) {
  std::vector<double> psnr_per_frame;
  std::vector<double> ssim_per_frame;
  if (check_file_extension(video_file)) {
    compute_metrics(video_file, &psnr_per_frame, &ssim_per_frame);
  } else {
    return -1;
  }
  printf("=============================\n");
  printf("Printing metrics for file: %s\n", video_file.c_str());
  printf("=============================\n");
  print_freezing_metrics(psnr_per_frame, ssim_per_frame);
  return 0;
}
