/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Get PSNR or SSIM for video sequence. Assuming RAW 4:2:0 Y:Cb:Cr format
// To build: g++ -O3 -o psnr psnr.cc ssim.cc psnr_main.cc
// or VisualC: cl /Ox psnr.cc ssim.cc psnr_main.cc
//
// To enable OpenMP and SSE2
// gcc: g++ -msse2 -O3 -fopenmp -o psnr psnr.cc ssim.cc psnr_main.cc
// vc:  cl /arch:SSE2 /Ox /openmp psnr.cc ssim.cc psnr_main.cc
//
// Usage: psnr org_seq rec_seq -s width height [-skip skip_org skip_rec]

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "./psnr.h"
#include "./ssim.h"
#ifdef HAVE_JPEG
#include "libyuv/compare.h"
#include "libyuv/convert.h"
#endif

struct metric {
  double y, u, v, all;
  double min_y, min_u, min_v, min_all;
  double global_y, global_u, global_v, global_all;
  int min_frame;
};

// options
bool verbose = false;
bool quiet = false;
bool show_name = false;
bool do_swap_uv = false;
bool do_psnr = false;
bool do_ssim = false;
bool do_mse = false;
bool do_lssim = false;
int image_width = 0, image_height = 0;
int fileindex_org = 0;  // argv argument contains the source file name.
int fileindex_rec = 0;  // argv argument contains the destination file name.
int num_rec = 0;
int num_skip_org = 0;
int num_skip_rec = 0;
int num_frames = 0;
#ifdef _OPENMP
int num_threads = 0;
#endif

// Parse PYUV format. ie name.1920x800_24Hz_P420.yuv
bool ExtractResolutionFromFilename(const char* name,
                                   int* width_ptr,
                                   int* height_ptr) {
  // Isolate the .width_height. section of the filename by searching for a
  // dot or underscore followed by a digit.
  for (int i = 0; name[i]; ++i) {
    if ((name[i] == '.' || name[i] == '_') && name[i + 1] >= '0' &&
        name[i + 1] <= '9') {
      int n = sscanf(name + i + 1, "%dx%d", width_ptr, height_ptr);  // NOLINT
      if (2 == n) {
        return true;
      }
    }
  }

#ifdef HAVE_JPEG
  // Try parsing file as a jpeg.
  FILE* const file_org = fopen(name, "rb");
  if (file_org == NULL) {
    fprintf(stderr, "Cannot open %s\n", name);
    return false;
  }
  fseek(file_org, 0, SEEK_END);
  size_t total_size = ftell(file_org);
  fseek(file_org, 0, SEEK_SET);
  uint8* const ch_org = new uint8[total_size];
  memset(ch_org, 0, total_size);
  size_t bytes_org = fread(ch_org, sizeof(uint8), total_size, file_org);
  fclose(file_org);
  if (bytes_org == total_size) {
    if (0 == libyuv::MJPGSize(ch_org, total_size, width_ptr, height_ptr)) {
      delete[] ch_org;
      return true;
    }
  }
  delete[] ch_org;
#endif  // HAVE_JPEG
  return false;
}

// Scale Y channel from 16..240 to 0..255.
// This can be useful when comparing codecs that are inconsistant about Y
uint8 ScaleY(uint8 y) {
  int ny = (y - 16) * 256 / 224;
  if (ny < 0)
    ny = 0;
  if (ny > 255)
    ny = 255;
  return static_cast<uint8>(ny);
}

// MSE = Mean Square Error
double GetMSE(double sse, double size) {
  return sse / size;
}

void PrintHelp(const char* program) {
  printf("%s [-options] org_seq rec_seq [rec_seq2.. etc]\n", program);
#ifdef HAVE_JPEG
  printf("jpeg or raw YUV 420 supported.\n");
#endif
  printf("options:\n");
  printf(
      " -s <width> <height> .... specify YUV size, mandatory if none of the "
      "sequences have the\n");
  printf(
      "                          resolution embedded in their filename (ie. "
      "name.1920x800_24Hz_P420.yuv)\n");
  printf(" -psnr .................. compute PSNR (default)\n");
  printf(" -ssim .................. compute SSIM\n");
  printf(" -mse ................... compute MSE\n");
  printf(" -swap .................. Swap U and V plane\n");
  printf(" -skip <org> <rec> ...... Number of frame to skip of org and rec\n");
  printf(" -frames <num> .......... Number of frames to compare\n");
#ifdef _OPENMP
  printf(" -t <num> ............... Number of threads\n");
#endif
  printf(" -n ..................... Show file name\n");
  printf(" -v ..................... verbose++\n");
  printf(" -q ..................... quiet\n");
  printf(" -h ..................... this help\n");
  exit(0);
}

void ParseOptions(int argc, const char* argv[]) {
  if (argc <= 1)
    PrintHelp(argv[0]);
  for (int c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-v")) {
      verbose = true;
    } else if (!strcmp(argv[c], "-q")) {
      quiet = true;
    } else if (!strcmp(argv[c], "-n")) {
      show_name = true;
    } else if (!strcmp(argv[c], "-psnr")) {
      do_psnr = true;
    } else if (!strcmp(argv[c], "-mse")) {
      do_mse = true;
    } else if (!strcmp(argv[c], "-ssim")) {
      do_ssim = true;
    } else if (!strcmp(argv[c], "-lssim")) {
      do_ssim = true;
      do_lssim = true;
    } else if (!strcmp(argv[c], "-swap")) {
      do_swap_uv = true;
    } else if (!strcmp(argv[c], "-h") || !strcmp(argv[c], "-help")) {
      PrintHelp(argv[0]);
    } else if (!strcmp(argv[c], "-s") && c + 2 < argc) {
      image_width = atoi(argv[++c]);   // NOLINT
      image_height = atoi(argv[++c]);  // NOLINT
    } else if (!strcmp(argv[c], "-skip") && c + 2 < argc) {
      num_skip_org = atoi(argv[++c]);  // NOLINT
      num_skip_rec = atoi(argv[++c]);  // NOLINT
    } else if (!strcmp(argv[c], "-frames") && c + 1 < argc) {
      num_frames = atoi(argv[++c]);  // NOLINT
#ifdef _OPENMP
    } else if (!strcmp(argv[c], "-t") && c + 1 < argc) {
      num_threads = atoi(argv[++c]);  // NOLINT
#endif
    } else if (argv[c][0] == '-') {
      fprintf(stderr, "Unknown option. %s\n", argv[c]);
    } else if (fileindex_org == 0) {
      fileindex_org = c;
    } else if (fileindex_rec == 0) {
      fileindex_rec = c;
      num_rec = 1;
    } else {
      ++num_rec;
    }
  }
  if (fileindex_org == 0 || fileindex_rec == 0) {
    fprintf(stderr, "Missing filenames\n");
    PrintHelp(argv[0]);
  }
  if (num_skip_org < 0 || num_skip_rec < 0) {
    fprintf(stderr, "Skipped frames incorrect\n");
    PrintHelp(argv[0]);
  }
  if (num_frames < 0) {
    fprintf(stderr, "Number of frames incorrect\n");
    PrintHelp(argv[0]);
  }
  if (image_width == 0 || image_height == 0) {
    int org_width, org_height;
    int rec_width, rec_height;
    bool org_res_avail = ExtractResolutionFromFilename(argv[fileindex_org],
                                                       &org_width, &org_height);
    bool rec_res_avail = ExtractResolutionFromFilename(argv[fileindex_rec],
                                                       &rec_width, &rec_height);
    if (org_res_avail) {
      if (rec_res_avail) {
        if ((org_width == rec_width) && (org_height == rec_height)) {
          image_width = org_width;
          image_height = org_height;
        } else {
          fprintf(stderr, "Sequences have different resolutions.\n");
          PrintHelp(argv[0]);
        }
      } else {
        image_width = org_width;
        image_height = org_height;
      }
    } else if (rec_res_avail) {
      image_width = rec_width;
      image_height = rec_height;
    } else {
      fprintf(stderr, "Missing dimensions.\n");
      PrintHelp(argv[0]);
    }
  }
}

bool UpdateMetrics(uint8* ch_org,
                   uint8* ch_rec,
                   const int y_size,
                   const int uv_size,
                   const size_t total_size,
                   int number_of_frames,
                   metric* cur_distortion_psnr,
                   metric* distorted_frame,
                   bool do_psnr) {
  const int uv_offset = (do_swap_uv ? uv_size : 0);
  const uint8* const u_org = ch_org + y_size + uv_offset;
  const uint8* const u_rec = ch_rec + y_size;
  const uint8* const v_org = ch_org + y_size + (uv_size - uv_offset);
  const uint8* const v_rec = ch_rec + y_size + uv_size;
  if (do_psnr) {
#ifdef HAVE_JPEG
    double y_err = static_cast<double>(
        libyuv::ComputeSumSquareError(ch_org, ch_rec, y_size));
    double u_err = static_cast<double>(
        libyuv::ComputeSumSquareError(u_org, u_rec, uv_size));
    double v_err = static_cast<double>(
        libyuv::ComputeSumSquareError(v_org, v_rec, uv_size));
#else
    double y_err = ComputeSumSquareError(ch_org, ch_rec, y_size);
    double u_err = ComputeSumSquareError(u_org, u_rec, uv_size);
    double v_err = ComputeSumSquareError(v_org, v_rec, uv_size);
#endif
    const double total_err = y_err + u_err + v_err;
    cur_distortion_psnr->global_y += y_err;
    cur_distortion_psnr->global_u += u_err;
    cur_distortion_psnr->global_v += v_err;
    cur_distortion_psnr->global_all += total_err;
    distorted_frame->y = ComputePSNR(y_err, static_cast<double>(y_size));
    distorted_frame->u = ComputePSNR(u_err, static_cast<double>(uv_size));
    distorted_frame->v = ComputePSNR(v_err, static_cast<double>(uv_size));
    distorted_frame->all =
        ComputePSNR(total_err, static_cast<double>(total_size));
  } else {
    distorted_frame->y = CalcSSIM(ch_org, ch_rec, image_width, image_height);
    distorted_frame->u =
        CalcSSIM(u_org, u_rec, (image_width + 1) / 2, (image_height + 1) / 2);
    distorted_frame->v =
        CalcSSIM(v_org, v_rec, (image_width + 1) / 2, (image_height + 1) / 2);
    distorted_frame->all =
        (distorted_frame->y + distorted_frame->u + distorted_frame->v) /
        total_size;
    distorted_frame->y /= y_size;
    distorted_frame->u /= uv_size;
    distorted_frame->v /= uv_size;

    if (do_lssim) {
      distorted_frame->all = CalcLSSIM(distorted_frame->all);
      distorted_frame->y = CalcLSSIM(distorted_frame->y);
      distorted_frame->u = CalcLSSIM(distorted_frame->u);
      distorted_frame->v = CalcLSSIM(distorted_frame->v);
    }
  }

  cur_distortion_psnr->y += distorted_frame->y;
  cur_distortion_psnr->u += distorted_frame->u;
  cur_distortion_psnr->v += distorted_frame->v;
  cur_distortion_psnr->all += distorted_frame->all;

  bool ismin = false;
  if (distorted_frame->y < cur_distortion_psnr->min_y)
    cur_distortion_psnr->min_y = distorted_frame->y;
  if (distorted_frame->u < cur_distortion_psnr->min_u)
    cur_distortion_psnr->min_u = distorted_frame->u;
  if (distorted_frame->v < cur_distortion_psnr->min_v)
    cur_distortion_psnr->min_v = distorted_frame->v;
  if (distorted_frame->all < cur_distortion_psnr->min_all) {
    cur_distortion_psnr->min_all = distorted_frame->all;
    cur_distortion_psnr->min_frame = number_of_frames;
    ismin = true;
  }
  return ismin;
}

int main(int argc, const char* argv[]) {
  ParseOptions(argc, argv);
  if (!do_psnr && !do_ssim) {
    do_psnr = true;
  }

#ifdef _OPENMP
  if (num_threads) {
    omp_set_num_threads(num_threads);
  }
  if (verbose) {
    printf("OpenMP %d procs\n", omp_get_num_procs());
  }
#endif
  // Open original file (first file argument)
  FILE* const file_org = fopen(argv[fileindex_org], "rb");
  if (file_org == NULL) {
    fprintf(stderr, "Cannot open %s\n", argv[fileindex_org]);
    exit(1);
  }

  // Open all files to compare to
  FILE** file_rec = new FILE*[num_rec];
  memset(file_rec, 0, num_rec * sizeof(FILE*));  // NOLINT
  for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
    file_rec[cur_rec] = fopen(argv[fileindex_rec + cur_rec], "rb");
    if (file_rec[cur_rec] == NULL) {
      fprintf(stderr, "Cannot open %s\n", argv[fileindex_rec + cur_rec]);
      fclose(file_org);
      for (int i = 0; i < cur_rec; ++i) {
        fclose(file_rec[i]);
      }
      delete[] file_rec;
      exit(1);
    }
  }

  const int y_size = image_width * image_height;
  const int uv_size = ((image_width + 1) / 2) * ((image_height + 1) / 2);
  const size_t total_size = y_size + 2 * uv_size;  // NOLINT
#if defined(_MSC_VER)
  _fseeki64(
      file_org,
      static_cast<__int64>(num_skip_org) * static_cast<__int64>(total_size),
      SEEK_SET);
#else
  fseek(file_org, num_skip_org * total_size, SEEK_SET);
#endif
  for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
#if defined(_MSC_VER)
    _fseeki64(
        file_rec[cur_rec],
        static_cast<__int64>(num_skip_rec) * static_cast<__int64>(total_size),
        SEEK_SET);
#else
    fseek(file_rec[cur_rec], num_skip_rec * total_size, SEEK_SET);
#endif
  }

  uint8* const ch_org = new uint8[total_size];
  uint8* const ch_rec = new uint8[total_size];
  if (ch_org == NULL || ch_rec == NULL) {
    fprintf(stderr, "No memory available\n");
    fclose(file_org);
    for (int i = 0; i < num_rec; ++i) {
      fclose(file_rec[i]);
    }
    delete[] ch_org;
    delete[] ch_rec;
    delete[] file_rec;
    exit(1);
  }

  metric* const distortion_psnr = new metric[num_rec];
  metric* const distortion_ssim = new metric[num_rec];
  for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
    metric* cur_distortion_psnr = &distortion_psnr[cur_rec];
    cur_distortion_psnr->y = 0.0;
    cur_distortion_psnr->u = 0.0;
    cur_distortion_psnr->v = 0.0;
    cur_distortion_psnr->all = 0.0;
    cur_distortion_psnr->min_y = kMaxPSNR;
    cur_distortion_psnr->min_u = kMaxPSNR;
    cur_distortion_psnr->min_v = kMaxPSNR;
    cur_distortion_psnr->min_all = kMaxPSNR;
    cur_distortion_psnr->min_frame = 0;
    cur_distortion_psnr->global_y = 0.0;
    cur_distortion_psnr->global_u = 0.0;
    cur_distortion_psnr->global_v = 0.0;
    cur_distortion_psnr->global_all = 0.0;
    distortion_ssim[cur_rec] = cur_distortion_psnr[cur_rec];
  }

  if (verbose) {
    printf("Size: %dx%d\n", image_width, image_height);
  }

  if (!quiet) {
    printf("Frame");
    if (do_psnr) {
      printf("\t PSNR-Y \t PSNR-U \t PSNR-V \t PSNR-All \t Frame");
    }
    if (do_ssim) {
      printf("\t  SSIM-Y\t  SSIM-U\t  SSIM-V\t  SSIM-All\t Frame");
    }
    if (show_name) {
      printf("\tName\n");
    } else {
      printf("\n");
    }
  }

  int number_of_frames;
  for (number_of_frames = 0;; ++number_of_frames) {
    if (num_frames && number_of_frames >= num_frames)
      break;

    size_t bytes_org = fread(ch_org, sizeof(uint8), total_size, file_org);
    if (bytes_org < total_size) {
#ifdef HAVE_JPEG
      // Try parsing file as a jpeg.
      uint8* const ch_jpeg = new uint8[bytes_org];
      memcpy(ch_jpeg, ch_org, bytes_org);
      memset(ch_org, 0, total_size);

      if (0 != libyuv::MJPGToI420(ch_jpeg, bytes_org, ch_org, image_width,
                                  ch_org + y_size, (image_width + 1) / 2,
                                  ch_org + y_size + uv_size,
                                  (image_width + 1) / 2, image_width,
                                  image_height, image_width, image_height)) {
        delete[] ch_jpeg;
        break;
      }
      delete[] ch_jpeg;
#else
      break;
#endif  // HAVE_JPEG
    }

    for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
      size_t bytes_rec =
          fread(ch_rec, sizeof(uint8), total_size, file_rec[cur_rec]);
      if (bytes_rec < total_size) {
#ifdef HAVE_JPEG
        // Try parsing file as a jpeg.
        uint8* const ch_jpeg = new uint8[bytes_rec];
        memcpy(ch_jpeg, ch_rec, bytes_rec);
        memset(ch_rec, 0, total_size);

        if (0 != libyuv::MJPGToI420(ch_jpeg, bytes_rec, ch_rec, image_width,
                                    ch_rec + y_size, (image_width + 1) / 2,
                                    ch_rec + y_size + uv_size,
                                    (image_width + 1) / 2, image_width,
                                    image_height, image_width, image_height)) {
          delete[] ch_jpeg;
          break;
        }
        delete[] ch_jpeg;
#else
        break;
#endif  // HAVE_JPEG
      }

      if (verbose) {
        printf("%5d", number_of_frames);
      }
      if (do_psnr) {
        metric distorted_frame;
        metric* cur_distortion_psnr = &distortion_psnr[cur_rec];
        bool ismin = UpdateMetrics(ch_org, ch_rec, y_size, uv_size, total_size,
                                   number_of_frames, cur_distortion_psnr,
                                   &distorted_frame, true);
        if (verbose) {
          printf("\t%10.6f", distorted_frame.y);
          printf("\t%10.6f", distorted_frame.u);
          printf("\t%10.6f", distorted_frame.v);
          printf("\t%10.6f", distorted_frame.all);
          printf("\t%5s", ismin ? "min" : "");
        }
      }
      if (do_ssim) {
        metric distorted_frame;
        metric* cur_distortion_ssim = &distortion_ssim[cur_rec];
        bool ismin = UpdateMetrics(ch_org, ch_rec, y_size, uv_size, total_size,
                                   number_of_frames, cur_distortion_ssim,
                                   &distorted_frame, false);
        if (verbose) {
          printf("\t%10.6f", distorted_frame.y);
          printf("\t%10.6f", distorted_frame.u);
          printf("\t%10.6f", distorted_frame.v);
          printf("\t%10.6f", distorted_frame.all);
          printf("\t%5s", ismin ? "min" : "");
        }
      }
      if (verbose) {
        if (show_name) {
          printf("\t%s", argv[fileindex_rec + cur_rec]);
        }
        printf("\n");
      }
    }
  }

  // Final PSNR computation.
  for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
    metric* cur_distortion_psnr = &distortion_psnr[cur_rec];
    metric* cur_distortion_ssim = &distortion_ssim[cur_rec];
    if (number_of_frames > 0) {
      const double norm = 1. / static_cast<double>(number_of_frames);
      cur_distortion_psnr->y *= norm;
      cur_distortion_psnr->u *= norm;
      cur_distortion_psnr->v *= norm;
      cur_distortion_psnr->all *= norm;
      cur_distortion_ssim->y *= norm;
      cur_distortion_ssim->u *= norm;
      cur_distortion_ssim->v *= norm;
      cur_distortion_ssim->all *= norm;
    }

    if (do_psnr) {
      const double global_psnr_y =
          ComputePSNR(cur_distortion_psnr->global_y,
                      static_cast<double>(y_size) * number_of_frames);
      const double global_psnr_u =
          ComputePSNR(cur_distortion_psnr->global_u,
                      static_cast<double>(uv_size) * number_of_frames);
      const double global_psnr_v =
          ComputePSNR(cur_distortion_psnr->global_v,
                      static_cast<double>(uv_size) * number_of_frames);
      const double global_psnr_all =
          ComputePSNR(cur_distortion_psnr->global_all,
                      static_cast<double>(total_size) * number_of_frames);
      printf("Global:\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d", global_psnr_y,
             global_psnr_u, global_psnr_v, global_psnr_all, number_of_frames);
      if (show_name) {
        printf("\t%s", argv[fileindex_rec + cur_rec]);
      }
      printf("\n");
    }

    if (!quiet) {
      printf("Avg:");
      if (do_psnr) {
        printf("\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d", cur_distortion_psnr->y,
               cur_distortion_psnr->u, cur_distortion_psnr->v,
               cur_distortion_psnr->all, number_of_frames);
      }
      if (do_ssim) {
        printf("\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d", cur_distortion_ssim->y,
               cur_distortion_ssim->u, cur_distortion_ssim->v,
               cur_distortion_ssim->all, number_of_frames);
      }
      if (show_name) {
        printf("\t%s", argv[fileindex_rec + cur_rec]);
      }
      printf("\n");
    }
    if (!quiet) {
      printf("Min:");
      if (do_psnr) {
        printf("\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d",
               cur_distortion_psnr->min_y, cur_distortion_psnr->min_u,
               cur_distortion_psnr->min_v, cur_distortion_psnr->min_all,
               cur_distortion_psnr->min_frame);
      }
      if (do_ssim) {
        printf("\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d",
               cur_distortion_ssim->min_y, cur_distortion_ssim->min_u,
               cur_distortion_ssim->min_v, cur_distortion_ssim->min_all,
               cur_distortion_ssim->min_frame);
      }
      if (show_name) {
        printf("\t%s", argv[fileindex_rec + cur_rec]);
      }
      printf("\n");
    }

    if (do_mse) {
      double global_mse_y =
          GetMSE(cur_distortion_psnr->global_y,
                 static_cast<double>(y_size) * number_of_frames);
      double global_mse_u =
          GetMSE(cur_distortion_psnr->global_u,
                 static_cast<double>(uv_size) * number_of_frames);
      double global_mse_v =
          GetMSE(cur_distortion_psnr->global_v,
                 static_cast<double>(uv_size) * number_of_frames);
      double global_mse_all =
          GetMSE(cur_distortion_psnr->global_all,
                 static_cast<double>(total_size) * number_of_frames);
      printf("MSE:\t%10.6f\t%10.6f\t%10.6f\t%10.6f\t%5d", global_mse_y,
             global_mse_u, global_mse_v, global_mse_all, number_of_frames);
      if (show_name) {
        printf("\t%s", argv[fileindex_rec + cur_rec]);
      }
      printf("\n");
    }
  }
  fclose(file_org);
  for (int cur_rec = 0; cur_rec < num_rec; ++cur_rec) {
    fclose(file_rec[cur_rec]);
  }
  delete[] distortion_psnr;
  delete[] distortion_ssim;
  delete[] ch_org;
  delete[] ch_rec;
  delete[] file_rec;
  return 0;
}
