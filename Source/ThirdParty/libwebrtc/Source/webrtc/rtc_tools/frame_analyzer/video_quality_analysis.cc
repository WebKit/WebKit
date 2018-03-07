/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <map>
#include <utility>

#define STATS_LINE_LENGTH 32
#define Y4M_FILE_HEADER_MAX_SIZE 200
#define Y4M_FRAME_DELIMITER "FRAME"
#define Y4M_FRAME_HEADER_SIZE 6

namespace webrtc {
namespace test {

ResultsContainer::ResultsContainer() {}
ResultsContainer::~ResultsContainer() {}

int GetI420FrameSize(int width, int height) {
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;

  int y_plane = width * height;  // I420 Y plane.
  int u_plane = half_width * half_height;  // I420 U plane.
  int v_plane = half_width * half_height;  // I420 V plane.

  return y_plane + u_plane + v_plane;
}

int ExtractFrameSequenceNumber(std::string line) {
  size_t space_position = line.find(' ');
  if (space_position == std::string::npos) {
    return -1;
  }
  std::string frame = line.substr(0, space_position);

  size_t underscore_position = frame.find('_');
  if (underscore_position == std::string::npos) {
    return -1;
  }
  std::string frame_number = frame.substr(underscore_position + 1);

  return strtol(frame_number.c_str(), NULL, 10);
}

int ExtractDecodedFrameNumber(std::string line) {
  size_t space_position = line.find(' ');
  if (space_position == std::string::npos) {
    return -1;
  }
  std::string decoded_number = line.substr(space_position + 1);

  return strtol(decoded_number.c_str(), NULL, 10);
}

bool IsThereBarcodeError(std::string line) {
  size_t barcode_error_position = line.find("Barcode error");
  if (barcode_error_position != std::string::npos) {
    return true;
  }
  return false;
}

bool GetNextStatsLine(FILE* stats_file, char* line) {
  int chars = 0;
  char buf = 0;

  while (buf != '\n') {
    size_t chars_read = fread(&buf, 1, 1, stats_file);
    if (chars_read != 1 || feof(stats_file)) {
      return false;
    }
    line[chars] = buf;
    ++chars;
  }
  line[chars-1] = '\0';  // Strip the trailing \n and put end of string.
  return true;
}

bool ExtractFrameFromYuvFile(const char* i420_file_name,
                             int width,
                             int height,
                             int frame_number,
                             uint8_t* result_frame) {
  int frame_size = GetI420FrameSize(width, height);
  int offset = frame_number * frame_size;  // Calculate offset for the frame.
  bool errors = false;

  FILE* input_file = fopen(i420_file_name, "rb");
  if (input_file == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            i420_file_name);
    return false;
  }

  // Change stream pointer to new offset.
  fseek(input_file, offset, SEEK_SET);

  size_t bytes_read = fread(result_frame, 1, frame_size, input_file);
  if (bytes_read != static_cast<size_t>(frame_size) &&
      ferror(input_file)) {
    fprintf(stdout, "Error while reading frame no %d from file %s\n",
            frame_number, i420_file_name);
    errors = true;
  }
  fclose(input_file);
  return !errors;
}

bool ExtractFrameFromY4mFile(const char* y4m_file_name,
                             int width,
                             int height,
                             int frame_number,
                             uint8_t* result_frame) {
  int frame_size = GetI420FrameSize(width, height);
  int inital_offset = frame_number * (frame_size + Y4M_FRAME_HEADER_SIZE);
  int frame_offset = 0;

  FILE* input_file = fopen(y4m_file_name, "rb");
  if (input_file == NULL) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            y4m_file_name);
    return false;
  }

  // YUV4MPEG2, a.k.a. Y4M File format has a file header and a frame header. The
  // file header has the aspect: "YUV4MPEG2 C420 W640 H360 Ip F30:1 A1:1".
  char frame_header[Y4M_FILE_HEADER_MAX_SIZE];
  size_t bytes_read =
      fread(frame_header, 1, Y4M_FILE_HEADER_MAX_SIZE - 1, input_file);
  if (bytes_read != static_cast<size_t>(frame_size) && ferror(input_file)) {
    fprintf(stdout, "Error while reading frame from file %s\n",
        y4m_file_name);
    fclose(input_file);
    return false;
  }
  frame_header[bytes_read] = '\0';
  std::string header_contents(frame_header);
  std::size_t found = header_contents.find(Y4M_FRAME_DELIMITER);
  if (found == std::string::npos) {
    fprintf(stdout, "Corrupted Y4M header, could not find \"FRAME\" in %s\n",
        header_contents.c_str());
    fclose(input_file);
    return false;
  }
  frame_offset = static_cast<int>(found);

  // Change stream pointer to new offset, skipping the frame header as well.
  fseek(input_file, inital_offset + frame_offset + Y4M_FRAME_HEADER_SIZE,
        SEEK_SET);

  bytes_read = fread(result_frame, 1, frame_size, input_file);
  if (feof(input_file)) {
    fclose(input_file);
    return false;
  }
  if (bytes_read != static_cast<size_t>(frame_size) && ferror(input_file)) {
    fprintf(stdout, "Error while reading frame no %d from file %s\n",
            frame_number, y4m_file_name);
    fclose(input_file);
    return false;
  }

  fclose(input_file);
  return true;
}

double CalculateMetrics(VideoAnalysisMetricsType video_metrics_type,
                        const uint8_t* ref_frame,
                        const uint8_t* test_frame,
                        int width,
                        int height) {
  if (!ref_frame || !test_frame)
    return -1;
  else if (height < 0 || width < 0)
    return -1;
  int half_width = (width + 1) >> 1;
  int half_height = (height + 1) >> 1;
  const uint8_t* src_y_a = ref_frame;
  const uint8_t* src_u_a = src_y_a + width * height;
  const uint8_t* src_v_a = src_u_a + half_width * half_height;
  const uint8_t* src_y_b = test_frame;
  const uint8_t* src_u_b = src_y_b + width * height;
  const uint8_t* src_v_b = src_u_b + half_width * half_height;

  int stride_y = width;
  int stride_uv = half_width;

  double result = 0.0;

  switch (video_metrics_type) {
    case kPSNR:
      // In the following: stride is determined by width.
      result = libyuv::I420Psnr(src_y_a, width, src_u_a, half_width,
                                src_v_a, half_width, src_y_b, width,
                                src_u_b, half_width, src_v_b, half_width,
                                width, height);
      // LibYuv sets the max psnr value to 128, we restrict it to 48.
      // In case of 0 mse in one frame, 128 can skew the results significantly.
      result = (result > 48.0) ? 48.0 : result;
      break;
    case kSSIM:
      result = libyuv::I420Ssim(src_y_a, stride_y, src_u_a, stride_uv,
                                src_v_a, stride_uv, src_y_b, stride_y,
                                src_u_b, stride_uv, src_v_b, stride_uv,
                                width, height);
      break;
    default:
      assert(false);
  }

  return result;
}

void RunAnalysis(const char* reference_file_name,
                 const char* test_file_name,
                 const char* stats_file_reference_name,
                 const char* stats_file_test_name,
                 int width,
                 int height,
                 ResultsContainer* results) {
  // Check if the reference_file_name ends with "y4m".
  bool y4m_mode = false;
  if (std::string(reference_file_name).find("y4m") != std::string::npos) {
    y4m_mode = true;
  }

  int size = GetI420FrameSize(width, height);
  FILE* stats_file_ref = fopen(stats_file_reference_name, "r");
  FILE* stats_file_test = fopen(stats_file_test_name, "r");

  // String buffer for the lines in the stats file.
  char line[STATS_LINE_LENGTH];

  // Allocate buffers for test and reference frames.
  uint8_t* test_frame = new uint8_t[size];
  uint8_t* reference_frame = new uint8_t[size];
  int previous_frame_number = -1;

  // Maps barcode id to the frame id for the reference video.
  // In case two frames have same id, then we only save the first one.
  std::map<int, int> ref_barcode_to_frame;
  // While there are entries in the stats file.
  while (GetNextStatsLine(stats_file_ref, line)) {
    int extracted_ref_frame = ExtractFrameSequenceNumber(line);
    int decoded_frame_number = ExtractDecodedFrameNumber(line);

    // Insert will only add if it is not in map already.
    ref_barcode_to_frame.insert(
        std::make_pair(decoded_frame_number, extracted_ref_frame));
  }

  while (GetNextStatsLine(stats_file_test, line)) {
    int extracted_test_frame = ExtractFrameSequenceNumber(line);
    int decoded_frame_number = ExtractDecodedFrameNumber(line);
    auto it = ref_barcode_to_frame.find(decoded_frame_number);
    if (it == ref_barcode_to_frame.end()) {
      // Not found in the reference video.
      // TODO(mandermo) print
      continue;
    }
    int extracted_ref_frame = it->second;

    // If there was problem decoding the barcode in this frame or the frame has
    // been duplicated, continue.
    if (IsThereBarcodeError(line) ||
        decoded_frame_number == previous_frame_number) {
      continue;
    }

    assert(extracted_test_frame != -1);
    assert(decoded_frame_number != -1);

    ExtractFrameFromYuvFile(test_file_name, width, height, extracted_test_frame,
                            test_frame);
    if (y4m_mode) {
      ExtractFrameFromY4mFile(reference_file_name, width, height,
                              extracted_ref_frame, reference_frame);
    } else {
      ExtractFrameFromYuvFile(reference_file_name, width, height,
                              extracted_ref_frame, reference_frame);
    }

    // Calculate the PSNR and SSIM.
    double result_psnr =
        CalculateMetrics(kPSNR, reference_frame, test_frame, width, height);
    double result_ssim =
        CalculateMetrics(kSSIM, reference_frame, test_frame, width, height);

    previous_frame_number = decoded_frame_number;

    // Fill in the result struct.
    AnalysisResult result;
    result.frame_number = decoded_frame_number;
    result.psnr_value = result_psnr;
    result.ssim_value = result_ssim;

    results->frames.push_back(result);
  }

  // Cleanup.
  fclose(stats_file_ref);
  fclose(stats_file_test);
  delete[] test_frame;
  delete[] reference_frame;
}

void PrintMaxRepeatedAndSkippedFrames(const std::string& label,
                                      const std::string& stats_file_ref_name,
                                      const std::string& stats_file_test_name) {
  PrintMaxRepeatedAndSkippedFrames(stdout, label, stats_file_ref_name,
                                   stats_file_test_name);
}

std::vector<std::pair<int, int> > CalculateFrameClusters(
    FILE* file,
    int* num_decode_errors) {
  if (num_decode_errors) {
    *num_decode_errors = 0;
  }
  std::vector<std::pair<int, int> > frame_cnt;
  char line[STATS_LINE_LENGTH];
  while (GetNextStatsLine(file, line)) {
    int decoded_frame_number;
    if (IsThereBarcodeError(line)) {
      decoded_frame_number = DECODE_ERROR;
      if (num_decode_errors) {
        ++*num_decode_errors;
      }
    } else {
      decoded_frame_number = ExtractDecodedFrameNumber(line);
    }
    if (frame_cnt.size() >= 2 && decoded_frame_number != DECODE_ERROR &&
        frame_cnt.back().first == DECODE_ERROR &&
        frame_cnt[frame_cnt.size() - 2].first == decoded_frame_number) {
      // Handle when there is a decoding error inside a cluster of frames.
      frame_cnt[frame_cnt.size() - 2].second += frame_cnt.back().second + 1;
      frame_cnt.pop_back();
    } else if (frame_cnt.empty() ||
               frame_cnt.back().first != decoded_frame_number) {
      frame_cnt.push_back(std::make_pair(decoded_frame_number, 1));
    } else {
      ++frame_cnt.back().second;
    }
  }
  return frame_cnt;
}

void PrintMaxRepeatedAndSkippedFrames(FILE* output,
                                      const std::string& label,
                                      const std::string& stats_file_ref_name,
                                      const std::string& stats_file_test_name) {
  FILE* stats_file_ref = fopen(stats_file_ref_name.c_str(), "r");
  FILE* stats_file_test = fopen(stats_file_test_name.c_str(), "r");
  if (stats_file_ref == NULL) {
    fprintf(stderr, "Couldn't open reference stats file for reading: %s\n",
            stats_file_ref_name.c_str());
    return;
  }
  if (stats_file_test == NULL) {
    fprintf(stderr, "Couldn't open test stats file for reading: %s\n",
            stats_file_test_name.c_str());
    fclose(stats_file_ref);
    return;
  }

  int max_repeated_frames = 1;
  int max_skipped_frames = 0;

  int decode_errors_ref = 0;
  int decode_errors_test = 0;

  std::vector<std::pair<int, int> > frame_cnt_ref =
      CalculateFrameClusters(stats_file_ref, &decode_errors_ref);

  std::vector<std::pair<int, int> > frame_cnt_test =
      CalculateFrameClusters(stats_file_test, &decode_errors_test);

  fclose(stats_file_ref);
  fclose(stats_file_test);

  auto it_ref = frame_cnt_ref.begin();
  auto it_test = frame_cnt_test.begin();
  auto end_ref = frame_cnt_ref.end();
  auto end_test = frame_cnt_test.end();

  if (it_test == end_test || it_ref == end_ref) {
    fprintf(stderr, "Either test or ref file is empty, nothing to print\n");
    return;
  }

  while (it_test != end_test && it_test->first == DECODE_ERROR) {
    ++it_test;
  }

  if (it_test == end_test) {
    fprintf(stderr, "Test video only has barcode decode errors\n");
    return;
  }

  // Find the first frame in the reference video that match the first frame in
  // the test video.
  while (it_ref != end_ref &&
         (it_ref->first == DECODE_ERROR || it_ref->first != it_test->first)) {
    ++it_ref;
  }
  if (it_ref == end_ref) {
    fprintf(stderr,
            "The barcode in the test video's first frame is not in the "
            "reference video.\n");
    return;
  }

  int total_skipped_frames = 0;
  for (;;) {
    max_repeated_frames =
        std::max(max_repeated_frames, it_test->second - it_ref->second + 1);

    bool passed_error = false;

    ++it_test;
    while (it_test != end_test && it_test->first == DECODE_ERROR) {
      ++it_test;
      passed_error = true;
    }
    if (it_test == end_test) {
      break;
    }

    int skipped_frames = 0;
    ++it_ref;
    for (; it_ref != end_ref; ++it_ref) {
      if (it_ref->first != DECODE_ERROR && it_ref->first >= it_test->first) {
        break;
      }
      ++skipped_frames;
    }
    if (passed_error) {
      // If we pass an error in the test video, then we are conservative
      // and will not calculate skipped frames for that part.
      skipped_frames = 0;
    }
    if (it_ref != end_ref && it_ref->first == it_test->first) {
      total_skipped_frames += skipped_frames;
      if (skipped_frames > max_skipped_frames) {
        max_skipped_frames = skipped_frames;
      }
      continue;
    }
    fprintf(output,
            "Found barcode %d in test video, which is not in reference video\n",
            it_test->first);
    break;
  }

  fprintf(output, "RESULT Max_repeated: %s= %d\n", label.c_str(),
          max_repeated_frames);
  fprintf(output, "RESULT Max_skipped: %s= %d\n", label.c_str(),
          max_skipped_frames);
  fprintf(output, "RESULT Total_skipped: %s= %d\n", label.c_str(),
          total_skipped_frames);
  fprintf(output, "RESULT Decode_errors_reference: %s= %d\n", label.c_str(),
          decode_errors_ref);
  fprintf(output, "RESULT Decode_errors_test: %s= %d\n", label.c_str(),
          decode_errors_test);
}

void PrintAnalysisResults(const std::string& label, ResultsContainer* results) {
  PrintAnalysisResults(stdout, label, results);
}

void PrintAnalysisResults(FILE* output, const std::string& label,
                          ResultsContainer* results) {
  std::vector<AnalysisResult>::iterator iter;

  fprintf(output, "RESULT Unique_frames_count: %s= %u score\n", label.c_str(),
          static_cast<unsigned int>(results->frames.size()));

  if (results->frames.size() > 0u) {
    fprintf(output, "RESULT PSNR: %s= [", label.c_str());
    for (iter = results->frames.begin(); iter != results->frames.end() - 1;
         ++iter) {
      fprintf(output, "%f,", iter->psnr_value);
    }
    fprintf(output, "%f] dB\n", iter->psnr_value);

    fprintf(output, "RESULT SSIM: %s= [", label.c_str());
    for (iter = results->frames.begin(); iter != results->frames.end() - 1;
         ++iter) {
      fprintf(output, "%f,", iter->ssim_value);
    }
    fprintf(output, "%f] score\n", iter->ssim_value);
  }
}

}  // namespace test
}  // namespace webrtc
