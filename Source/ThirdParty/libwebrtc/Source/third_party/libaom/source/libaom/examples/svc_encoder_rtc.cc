/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//  This is an example demonstrating how to implement a multi-layer AOM
//  encoding scheme for RTC video applications.

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "config/aom_config.h"

#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#endif
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "common/args.h"
#include "common/tools_common.h"
#include "common/video_writer.h"
#include "examples/encoder_util.h"
#include "aom_ports/aom_timer.h"
#include "av1/ratectrl_rtc.h"

#define OPTION_BUFFER_SIZE 1024

typedef struct {
  const char *output_filename;
  char options[OPTION_BUFFER_SIZE];
  struct AvxInputContext input_ctx;
  int speed;
  int aq_mode;
  int layering_mode;
  int output_obu;
  int decode;
  int tune_content;
  int show_psnr;
  bool use_external_rc;
} AppInput;

typedef enum {
  QUANTIZER = 0,
  BITRATE,
  SCALE_FACTOR,
  AUTO_ALT_REF,
  ALL_OPTION_TYPES
} LAYER_OPTION_TYPE;

static const arg_def_t outputfile =
    ARG_DEF("o", "output", 1, "Output filename");
static const arg_def_t frames_arg =
    ARG_DEF("f", "frames", 1, "Number of frames to encode");
static const arg_def_t threads_arg =
    ARG_DEF("th", "threads", 1, "Number of threads to use");
static const arg_def_t width_arg = ARG_DEF("w", "width", 1, "Source width");
static const arg_def_t height_arg = ARG_DEF("h", "height", 1, "Source height");
static const arg_def_t timebase_arg =
    ARG_DEF("t", "timebase", 1, "Timebase (num/den)");
static const arg_def_t bitrate_arg = ARG_DEF(
    "b", "target-bitrate", 1, "Encoding bitrate, in kilobits per second");
static const arg_def_t spatial_layers_arg =
    ARG_DEF("sl", "spatial-layers", 1, "Number of spatial SVC layers");
static const arg_def_t temporal_layers_arg =
    ARG_DEF("tl", "temporal-layers", 1, "Number of temporal SVC layers");
static const arg_def_t layering_mode_arg =
    ARG_DEF("lm", "layering-mode", 1, "Temporal layering scheme.");
static const arg_def_t kf_dist_arg =
    ARG_DEF("k", "kf-dist", 1, "Number of frames between keyframes");
static const arg_def_t scale_factors_arg =
    ARG_DEF("r", "scale-factors", 1, "Scale factors (lowest to highest layer)");
static const arg_def_t min_q_arg =
    ARG_DEF(NULL, "min-q", 1, "Minimum quantizer");
static const arg_def_t max_q_arg =
    ARG_DEF(NULL, "max-q", 1, "Maximum quantizer");
static const arg_def_t speed_arg =
    ARG_DEF("sp", "speed", 1, "Speed configuration");
static const arg_def_t aqmode_arg =
    ARG_DEF("aq", "aqmode", 1, "AQ mode off/on");
static const arg_def_t bitrates_arg =
    ARG_DEF("bl", "bitrates", 1,
            "Bitrates[spatial_layer * num_temporal_layer + temporal_layer]");
static const arg_def_t dropframe_thresh_arg =
    ARG_DEF(NULL, "drop-frame", 1, "Temporal resampling threshold (buf %)");
static const arg_def_t error_resilient_arg =
    ARG_DEF(NULL, "error-resilient", 1, "Error resilient flag");
static const arg_def_t output_obu_arg =
    ARG_DEF(NULL, "output-obu", 1,
            "Write OBUs when set to 1. Otherwise write IVF files.");
static const arg_def_t test_decode_arg =
    ARG_DEF(NULL, "test-decode", 1,
            "Attempt to test decoding the output when set to 1. Default is 1.");
static const arg_def_t psnr_arg =
    ARG_DEF(NULL, "psnr", -1, "Show PSNR in status line.");
static const arg_def_t ext_rc_arg =
    ARG_DEF(NULL, "use-ext-rc", 0, "Use external rate control.");
static const struct arg_enum_list tune_content_enum[] = {
  { "default", AOM_CONTENT_DEFAULT },
  { "screen", AOM_CONTENT_SCREEN },
  { "film", AOM_CONTENT_FILM },
  { NULL, 0 }
};
static const arg_def_t tune_content_arg = ARG_DEF_ENUM(
    NULL, "tune-content", 1, "Tune content type", tune_content_enum);

#if CONFIG_AV1_HIGHBITDEPTH
static const struct arg_enum_list bitdepth_enum[] = { { "8", AOM_BITS_8 },
                                                      { "10", AOM_BITS_10 },
                                                      { NULL, 0 } };

static const arg_def_t bitdepth_arg = ARG_DEF_ENUM(
    "d", "bit-depth", 1, "Bit depth for codec 8 or 10. ", bitdepth_enum);
#endif  // CONFIG_AV1_HIGHBITDEPTH

static const arg_def_t *svc_args[] = {
  &frames_arg,          &outputfile,     &width_arg,
  &height_arg,          &timebase_arg,   &bitrate_arg,
  &spatial_layers_arg,  &kf_dist_arg,    &scale_factors_arg,
  &min_q_arg,           &max_q_arg,      &temporal_layers_arg,
  &layering_mode_arg,   &threads_arg,    &aqmode_arg,
#if CONFIG_AV1_HIGHBITDEPTH
  &bitdepth_arg,
#endif
  &speed_arg,           &bitrates_arg,   &dropframe_thresh_arg,
  &error_resilient_arg, &output_obu_arg, &test_decode_arg,
  &tune_content_arg,    &psnr_arg,       NULL,
};

#define zero(Dest) memset(&(Dest), 0, sizeof(Dest))

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <options> input_filename -o output_filename\n",
          exec_name);
  fprintf(stderr, "Options:\n");
  arg_show_usage(stderr, svc_args);
  exit(EXIT_FAILURE);
}

static int file_is_y4m(const char detect[4]) {
  return memcmp(detect, "YUV4", 4) == 0;
}

static int fourcc_is_ivf(const char detect[4]) {
  if (memcmp(detect, "DKIF", 4) == 0) {
    return 1;
  }
  return 0;
}

static const int option_max_values[ALL_OPTION_TYPES] = { 63, INT_MAX, INT_MAX,
                                                         1 };

static const int option_min_values[ALL_OPTION_TYPES] = { 0, 0, 1, 0 };

static void open_input_file(struct AvxInputContext *input,
                            aom_chroma_sample_position_t csp) {
  /* Parse certain options from the input file, if possible */
  input->file = strcmp(input->filename, "-") ? fopen(input->filename, "rb")
                                             : set_binary_mode(stdin);

  if (!input->file) fatal("Failed to open input file");

  if (!fseeko(input->file, 0, SEEK_END)) {
    /* Input file is seekable. Figure out how long it is, so we can get
     * progress info.
     */
    input->length = ftello(input->file);
    rewind(input->file);
  }

  /* Default to 1:1 pixel aspect ratio. */
  input->pixel_aspect_ratio.numerator = 1;
  input->pixel_aspect_ratio.denominator = 1;

  /* For RAW input sources, these bytes will applied on the first frame
   *  in read_frame().
   */
  input->detect.buf_read = fread(input->detect.buf, 1, 4, input->file);
  input->detect.position = 0;

  if (input->detect.buf_read == 4 && file_is_y4m(input->detect.buf)) {
    if (y4m_input_open(&input->y4m, input->file, input->detect.buf, 4, csp,
                       input->only_i420) >= 0) {
      input->file_type = FILE_TYPE_Y4M;
      input->width = input->y4m.pic_w;
      input->height = input->y4m.pic_h;
      input->pixel_aspect_ratio.numerator = input->y4m.par_n;
      input->pixel_aspect_ratio.denominator = input->y4m.par_d;
      input->framerate.numerator = input->y4m.fps_n;
      input->framerate.denominator = input->y4m.fps_d;
      input->fmt = input->y4m.aom_fmt;
      input->bit_depth = static_cast<aom_bit_depth_t>(input->y4m.bit_depth);
    } else {
      fatal("Unsupported Y4M stream.");
    }
  } else if (input->detect.buf_read == 4 && fourcc_is_ivf(input->detect.buf)) {
    fatal("IVF is not supported as input.");
  } else {
    input->file_type = FILE_TYPE_RAW;
  }
}

static aom_codec_err_t extract_option(LAYER_OPTION_TYPE type, char *input,
                                      int *value0, int *value1) {
  if (type == SCALE_FACTOR) {
    *value0 = (int)strtol(input, &input, 10);
    if (*input++ != '/') return AOM_CODEC_INVALID_PARAM;
    *value1 = (int)strtol(input, &input, 10);

    if (*value0 < option_min_values[SCALE_FACTOR] ||
        *value1 < option_min_values[SCALE_FACTOR] ||
        *value0 > option_max_values[SCALE_FACTOR] ||
        *value1 > option_max_values[SCALE_FACTOR] ||
        *value0 > *value1)  // num shouldn't be greater than den
      return AOM_CODEC_INVALID_PARAM;
  } else {
    *value0 = atoi(input);
    if (*value0 < option_min_values[type] || *value0 > option_max_values[type])
      return AOM_CODEC_INVALID_PARAM;
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t parse_layer_options_from_string(
    aom_svc_params_t *svc_params, LAYER_OPTION_TYPE type, const char *input,
    int *option0, int *option1) {
  aom_codec_err_t res = AOM_CODEC_OK;
  char *input_string;
  char *token;
  const char *delim = ",";
  int num_layers = svc_params->number_spatial_layers;
  int i = 0;

  if (type == BITRATE)
    num_layers =
        svc_params->number_spatial_layers * svc_params->number_temporal_layers;

  if (input == NULL || option0 == NULL ||
      (option1 == NULL && type == SCALE_FACTOR))
    return AOM_CODEC_INVALID_PARAM;

  const size_t input_length = strlen(input);
  input_string = reinterpret_cast<char *>(malloc(input_length + 1));
  if (input_string == NULL) return AOM_CODEC_MEM_ERROR;
  memcpy(input_string, input, input_length + 1);
  token = strtok(input_string, delim);  // NOLINT
  for (i = 0; i < num_layers; ++i) {
    if (token != NULL) {
      res = extract_option(type, token, option0 + i, option1 + i);
      if (res != AOM_CODEC_OK) break;
      token = strtok(NULL, delim);  // NOLINT
    } else {
      res = AOM_CODEC_INVALID_PARAM;
      break;
    }
  }
  free(input_string);
  return res;
}

static void parse_command_line(int argc, const char **argv_,
                               AppInput *app_input,
                               aom_svc_params_t *svc_params,
                               aom_codec_enc_cfg_t *enc_cfg) {
  struct arg arg;
  char **argv = NULL;
  char **argi = NULL;
  char **argj = NULL;
  char string_options[1024] = { 0 };

  // Default settings
  svc_params->number_spatial_layers = 1;
  svc_params->number_temporal_layers = 1;
  app_input->layering_mode = 0;
  app_input->output_obu = 0;
  app_input->decode = 1;
  enc_cfg->g_threads = 1;
  enc_cfg->rc_end_usage = AOM_CBR;

  // process command line options
  argv = argv_dup(argc - 1, argv_ + 1);
  if (!argv) {
    fprintf(stderr, "Error allocating argument list\n");
    exit(EXIT_FAILURE);
  }
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

    if (arg_match(&arg, &outputfile, argi)) {
      app_input->output_filename = arg.val;
    } else if (arg_match(&arg, &width_arg, argi)) {
      enc_cfg->g_w = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &height_arg, argi)) {
      enc_cfg->g_h = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &timebase_arg, argi)) {
      enc_cfg->g_timebase = arg_parse_rational(&arg);
    } else if (arg_match(&arg, &bitrate_arg, argi)) {
      enc_cfg->rc_target_bitrate = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &spatial_layers_arg, argi)) {
      svc_params->number_spatial_layers = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &temporal_layers_arg, argi)) {
      svc_params->number_temporal_layers = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &speed_arg, argi)) {
      app_input->speed = arg_parse_uint(&arg);
      if (app_input->speed > 11) {
        aom_tools_warn("Mapping speed %d to speed 11.\n", app_input->speed);
      }
    } else if (arg_match(&arg, &aqmode_arg, argi)) {
      app_input->aq_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &threads_arg, argi)) {
      enc_cfg->g_threads = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &layering_mode_arg, argi)) {
      app_input->layering_mode = arg_parse_int(&arg);
    } else if (arg_match(&arg, &kf_dist_arg, argi)) {
      enc_cfg->kf_min_dist = arg_parse_uint(&arg);
      enc_cfg->kf_max_dist = enc_cfg->kf_min_dist;
    } else if (arg_match(&arg, &scale_factors_arg, argi)) {
      aom_codec_err_t res = parse_layer_options_from_string(
          svc_params, SCALE_FACTOR, arg.val, svc_params->scaling_factor_num,
          svc_params->scaling_factor_den);
      if (res != AOM_CODEC_OK) {
        die("Failed to parse scale factors: %s\n",
            aom_codec_err_to_string(res));
      }
    } else if (arg_match(&arg, &min_q_arg, argi)) {
      enc_cfg->rc_min_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &max_q_arg, argi)) {
      enc_cfg->rc_max_quantizer = arg_parse_uint(&arg);
#if CONFIG_AV1_HIGHBITDEPTH
    } else if (arg_match(&arg, &bitdepth_arg, argi)) {
      enc_cfg->g_bit_depth =
          static_cast<aom_bit_depth_t>(arg_parse_enum_or_int(&arg));
      switch (enc_cfg->g_bit_depth) {
        case AOM_BITS_8:
          enc_cfg->g_input_bit_depth = 8;
          enc_cfg->g_profile = 0;
          break;
        case AOM_BITS_10:
          enc_cfg->g_input_bit_depth = 10;
          enc_cfg->g_profile = 0;
          break;
        default:
          die("Error: Invalid bit depth selected (%d)\n", enc_cfg->g_bit_depth);
      }
#endif  // CONFIG_VP9_HIGHBITDEPTH
    } else if (arg_match(&arg, &dropframe_thresh_arg, argi)) {
      enc_cfg->rc_dropframe_thresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &error_resilient_arg, argi)) {
      enc_cfg->g_error_resilient = arg_parse_uint(&arg);
      if (enc_cfg->g_error_resilient != 0 && enc_cfg->g_error_resilient != 1)
        die("Invalid value for error resilient (0, 1): %d.",
            enc_cfg->g_error_resilient);
    } else if (arg_match(&arg, &output_obu_arg, argi)) {
      app_input->output_obu = arg_parse_uint(&arg);
      if (app_input->output_obu != 0 && app_input->output_obu != 1)
        die("Invalid value for obu output flag (0, 1): %d.",
            app_input->output_obu);
    } else if (arg_match(&arg, &test_decode_arg, argi)) {
      app_input->decode = arg_parse_uint(&arg);
      if (app_input->decode != 0 && app_input->decode != 1)
        die("Invalid value for test decode flag (0, 1): %d.",
            app_input->decode);
    } else if (arg_match(&arg, &tune_content_arg, argi)) {
      app_input->tune_content = arg_parse_enum_or_int(&arg);
      printf("tune content %d\n", app_input->tune_content);
    } else if (arg_match(&arg, &psnr_arg, argi)) {
      app_input->show_psnr = 1;
    } else if (arg_match(&arg, &ext_rc_arg, argi)) {
      app_input->use_external_rc = true;
    } else {
      ++argj;
    }
  }

  // Total bitrate needs to be parsed after the number of layers.
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;
    if (arg_match(&arg, &bitrates_arg, argi)) {
      aom_codec_err_t res = parse_layer_options_from_string(
          svc_params, BITRATE, arg.val, svc_params->layer_target_bitrate, NULL);
      if (res != AOM_CODEC_OK) {
        die("Failed to parse bitrates: %s\n", aom_codec_err_to_string(res));
      }
    } else {
      ++argj;
    }
  }

  // There will be a space in front of the string options
  if (strlen(string_options) > 0)
    strncpy(app_input->options, string_options, OPTION_BUFFER_SIZE);

  // Check for unrecognized options
  for (argi = argv; *argi; ++argi)
    if (argi[0][0] == '-' && strlen(argi[0]) > 1)
      die("Error: Unrecognized option %s\n", *argi);

  if (argv[0] == NULL) {
    usage_exit();
  }

  app_input->input_ctx.filename = argv[0];
  free(argv);

  open_input_file(&app_input->input_ctx, AOM_CSP_UNKNOWN);
  if (app_input->input_ctx.file_type == FILE_TYPE_Y4M) {
    enc_cfg->g_w = app_input->input_ctx.width;
    enc_cfg->g_h = app_input->input_ctx.height;
  }

  if (enc_cfg->g_w < 16 || enc_cfg->g_w % 2 || enc_cfg->g_h < 16 ||
      enc_cfg->g_h % 2)
    die("Invalid resolution: %d x %d\n", enc_cfg->g_w, enc_cfg->g_h);

  printf(
      "Codec %s\n"
      "layers: %d\n"
      "width %u, height: %u\n"
      "num: %d, den: %d, bitrate: %u\n"
      "gop size: %u\n",
      aom_codec_iface_name(aom_codec_av1_cx()),
      svc_params->number_spatial_layers, enc_cfg->g_w, enc_cfg->g_h,
      enc_cfg->g_timebase.num, enc_cfg->g_timebase.den,
      enc_cfg->rc_target_bitrate, enc_cfg->kf_max_dist);
}

static int mode_to_num_temporal_layers[12] = {
  1, 2, 3, 3, 2, 1, 1, 3, 3, 3, 3, 3,
};
static int mode_to_num_spatial_layers[12] = {
  1, 1, 1, 1, 1, 2, 3, 2, 3, 3, 3, 3,
};

// For rate control encoding stats.
struct RateControlMetrics {
  // Number of input frames per layer.
  int layer_input_frames[AOM_MAX_TS_LAYERS];
  // Number of encoded non-key frames per layer.
  int layer_enc_frames[AOM_MAX_TS_LAYERS];
  // Framerate per layer layer (cumulative).
  double layer_framerate[AOM_MAX_TS_LAYERS];
  // Target average frame size per layer (per-frame-bandwidth per layer).
  double layer_pfb[AOM_MAX_LAYERS];
  // Actual average frame size per layer.
  double layer_avg_frame_size[AOM_MAX_LAYERS];
  // Average rate mismatch per layer (|target - actual| / target).
  double layer_avg_rate_mismatch[AOM_MAX_LAYERS];
  // Actual encoding bitrate per layer (cumulative across temporal layers).
  double layer_encoding_bitrate[AOM_MAX_LAYERS];
  // Average of the short-time encoder actual bitrate.
  // TODO(marpan): Should we add these short-time stats for each layer?
  double avg_st_encoding_bitrate;
  // Variance of the short-time encoder actual bitrate.
  double variance_st_encoding_bitrate;
  // Window (number of frames) for computing short-timee encoding bitrate.
  int window_size;
  // Number of window measurements.
  int window_count;
  int layer_target_bitrate[AOM_MAX_LAYERS];
};

static const int REF_FRAMES = 8;

static const int INTER_REFS_PER_FRAME = 7;

// Reference frames used in this example encoder.
enum {
  SVC_LAST_FRAME = 0,
  SVC_LAST2_FRAME,
  SVC_LAST3_FRAME,
  SVC_GOLDEN_FRAME,
  SVC_BWDREF_FRAME,
  SVC_ALTREF2_FRAME,
  SVC_ALTREF_FRAME
};

static int read_frame(struct AvxInputContext *input_ctx, aom_image_t *img) {
  FILE *f = input_ctx->file;
  y4m_input *y4m = &input_ctx->y4m;
  int shortread = 0;

  if (input_ctx->file_type == FILE_TYPE_Y4M) {
    if (y4m_input_fetch_frame(y4m, f, img) < 1) return 0;
  } else {
    shortread = read_yuv_frame(input_ctx, img);
  }

  return !shortread;
}

static void close_input_file(struct AvxInputContext *input) {
  fclose(input->file);
  if (input->file_type == FILE_TYPE_Y4M) y4m_input_close(&input->y4m);
}

// Note: these rate control metrics assume only 1 key frame in the
// sequence (i.e., first frame only). So for temporal pattern# 7
// (which has key frame for every frame on base layer), the metrics
// computation will be off/wrong.
// TODO(marpan): Update these metrics to account for multiple key frames
// in the stream.
static void set_rate_control_metrics(struct RateControlMetrics *rc,
                                     double framerate, int ss_number_layers,
                                     int ts_number_layers) {
  int ts_rate_decimator[AOM_MAX_TS_LAYERS] = { 1 };
  ts_rate_decimator[0] = 1;
  if (ts_number_layers == 2) {
    ts_rate_decimator[0] = 2;
    ts_rate_decimator[1] = 1;
  }
  if (ts_number_layers == 3) {
    ts_rate_decimator[0] = 4;
    ts_rate_decimator[1] = 2;
    ts_rate_decimator[2] = 1;
  }
  // Set the layer (cumulative) framerate and the target layer (non-cumulative)
  // per-frame-bandwidth, for the rate control encoding stats below.
  for (int sl = 0; sl < ss_number_layers; ++sl) {
    int i = sl * ts_number_layers;
    rc->layer_framerate[0] = framerate / ts_rate_decimator[0];
    rc->layer_pfb[i] =
        1000.0 * rc->layer_target_bitrate[i] / rc->layer_framerate[0];
    for (int tl = 0; tl < ts_number_layers; ++tl) {
      i = sl * ts_number_layers + tl;
      if (tl > 0) {
        rc->layer_framerate[tl] = framerate / ts_rate_decimator[tl];
        rc->layer_pfb[i] =
            1000.0 *
            (rc->layer_target_bitrate[i] - rc->layer_target_bitrate[i - 1]) /
            (rc->layer_framerate[tl] - rc->layer_framerate[tl - 1]);
      }
      rc->layer_input_frames[tl] = 0;
      rc->layer_enc_frames[tl] = 0;
      rc->layer_encoding_bitrate[i] = 0.0;
      rc->layer_avg_frame_size[i] = 0.0;
      rc->layer_avg_rate_mismatch[i] = 0.0;
    }
  }
  rc->window_count = 0;
  rc->window_size = 15;
  rc->avg_st_encoding_bitrate = 0.0;
  rc->variance_st_encoding_bitrate = 0.0;
}

static void printout_rate_control_summary(struct RateControlMetrics *rc,
                                          int frame_cnt, int ss_number_layers,
                                          int ts_number_layers) {
  int tot_num_frames = 0;
  double perc_fluctuation = 0.0;
  printf("Total number of processed frames: %d\n\n", frame_cnt - 1);
  printf("Rate control layer stats for %d layer(s):\n\n", ts_number_layers);
  for (int sl = 0; sl < ss_number_layers; ++sl) {
    tot_num_frames = 0;
    for (int tl = 0; tl < ts_number_layers; ++tl) {
      int i = sl * ts_number_layers + tl;
      const int num_dropped =
          tl > 0 ? rc->layer_input_frames[tl] - rc->layer_enc_frames[tl]
                 : rc->layer_input_frames[tl] - rc->layer_enc_frames[tl] - 1;
      tot_num_frames += rc->layer_input_frames[tl];
      rc->layer_encoding_bitrate[i] = 0.001 * rc->layer_framerate[tl] *
                                      rc->layer_encoding_bitrate[i] /
                                      tot_num_frames;
      rc->layer_avg_frame_size[i] =
          rc->layer_avg_frame_size[i] / rc->layer_enc_frames[tl];
      rc->layer_avg_rate_mismatch[i] =
          100.0 * rc->layer_avg_rate_mismatch[i] / rc->layer_enc_frames[tl];
      printf("For layer#: %d %d \n", sl, tl);
      printf("Bitrate (target vs actual): %d %f\n", rc->layer_target_bitrate[i],
             rc->layer_encoding_bitrate[i]);
      printf("Average frame size (target vs actual): %f %f\n", rc->layer_pfb[i],
             rc->layer_avg_frame_size[i]);
      printf("Average rate_mismatch: %f\n", rc->layer_avg_rate_mismatch[i]);
      printf(
          "Number of input frames, encoded (non-key) frames, "
          "and perc dropped frames: %d %d %f\n",
          rc->layer_input_frames[tl], rc->layer_enc_frames[tl],
          100.0 * num_dropped / rc->layer_input_frames[tl]);
      printf("\n");
    }
  }
  rc->avg_st_encoding_bitrate = rc->avg_st_encoding_bitrate / rc->window_count;
  rc->variance_st_encoding_bitrate =
      rc->variance_st_encoding_bitrate / rc->window_count -
      (rc->avg_st_encoding_bitrate * rc->avg_st_encoding_bitrate);
  perc_fluctuation = 100.0 * sqrt(rc->variance_st_encoding_bitrate) /
                     rc->avg_st_encoding_bitrate;
  printf("Short-time stats, for window of %d frames:\n", rc->window_size);
  printf("Average, rms-variance, and percent-fluct: %f %f %f\n",
         rc->avg_st_encoding_bitrate, sqrt(rc->variance_st_encoding_bitrate),
         perc_fluctuation);
  if (frame_cnt - 1 != tot_num_frames)
    die("Error: Number of input frames not equal to output!\n");
}

// Layer pattern configuration.
static void set_layer_pattern(
    int layering_mode, int superframe_cnt, aom_svc_layer_id_t *layer_id,
    aom_svc_ref_frame_config_t *ref_frame_config,
    aom_svc_ref_frame_comp_pred_t *ref_frame_comp_pred, int *use_svc_control,
    int spatial_layer_id, int is_key_frame, int ksvc_mode, int speed) {
  // Setting this flag to 1 enables simplex example of
  // RPS (Reference Picture Selection) for 1 layer.
  int use_rps_example = 0;
  int i;
  int enable_longterm_temporal_ref = 1;
  int shift = (layering_mode == 8) ? 2 : 0;
  int simulcast_mode = (layering_mode == 11);
  *use_svc_control = 1;
  layer_id->spatial_layer_id = spatial_layer_id;
  int lag_index = 0;
  int base_count = superframe_cnt >> 2;
  ref_frame_comp_pred->use_comp_pred[0] = 0;  // GOLDEN_LAST
  ref_frame_comp_pred->use_comp_pred[1] = 0;  // LAST2_LAST
  ref_frame_comp_pred->use_comp_pred[2] = 0;  // ALTREF_LAST
  // Set the reference map buffer idx for the 7 references:
  // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
  // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
  for (i = 0; i < INTER_REFS_PER_FRAME; i++) ref_frame_config->ref_idx[i] = i;
  for (i = 0; i < INTER_REFS_PER_FRAME; i++) ref_frame_config->reference[i] = 0;
  for (i = 0; i < REF_FRAMES; i++) ref_frame_config->refresh[i] = 0;

  if (ksvc_mode) {
    // Same pattern as case 9, but the reference strucutre will be constrained
    // below.
    layering_mode = 9;
  }
  switch (layering_mode) {
    case 0:
      if (use_rps_example == 0) {
        // 1-layer: update LAST on every frame, reference LAST.
        layer_id->temporal_layer_id = 0;
        layer_id->spatial_layer_id = 0;
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else {
        // Pattern of 2 references (ALTREF and GOLDEN) trailing
        // LAST by 4 and 8 frames, with some switching logic to
        // sometimes only predict from the longer-term reference
        //(golden here). This is simple example to test RPS
        // (reference picture selection).
        int last_idx = 0;
        int last_idx_refresh = 0;
        int gld_idx = 0;
        int alt_ref_idx = 0;
        int lag_alt = 4;
        int lag_gld = 8;
        layer_id->temporal_layer_id = 0;
        layer_id->spatial_layer_id = 0;
        int sh = 8;  // slots 0 - 7.
        // Moving index slot for last: 0 - (sh - 1)
        if (superframe_cnt > 1) last_idx = (superframe_cnt - 1) % sh;
        // Moving index for refresh of last: one ahead for next frame.
        last_idx_refresh = superframe_cnt % sh;
        // Moving index for gld_ref, lag behind current by lag_gld
        if (superframe_cnt > lag_gld) gld_idx = (superframe_cnt - lag_gld) % sh;
        // Moving index for alt_ref, lag behind LAST by lag_alt frames.
        if (superframe_cnt > lag_alt)
          alt_ref_idx = (superframe_cnt - lag_alt) % sh;
        // Set the ref_idx.
        // Default all references to slot for last.
        for (i = 0; i < INTER_REFS_PER_FRAME; i++)
          ref_frame_config->ref_idx[i] = last_idx;
        // Set the ref_idx for the relevant references.
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = last_idx;
        ref_frame_config->ref_idx[SVC_LAST2_FRAME] = last_idx_refresh;
        ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = gld_idx;
        ref_frame_config->ref_idx[SVC_ALTREF_FRAME] = alt_ref_idx;
        // Refresh this slot, which will become LAST on next frame.
        ref_frame_config->refresh[last_idx_refresh] = 1;
        // Reference LAST, ALTREF, and GOLDEN
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        ref_frame_config->reference[SVC_ALTREF_FRAME] = 1;
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
        // Switch to only GOLDEN every 300 frames.
        if (superframe_cnt % 200 == 0 && superframe_cnt > 0) {
          ref_frame_config->reference[SVC_LAST_FRAME] = 0;
          ref_frame_config->reference[SVC_ALTREF_FRAME] = 0;
          ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
          // Test if the long-term is LAST instead, this is just a renaming
          // but its tests if encoder behaves the same, whether its
          // LAST or GOLDEN.
          if (superframe_cnt % 400 == 0 && superframe_cnt > 0) {
            ref_frame_config->ref_idx[SVC_LAST_FRAME] = gld_idx;
            ref_frame_config->reference[SVC_LAST_FRAME] = 1;
            ref_frame_config->reference[SVC_ALTREF_FRAME] = 0;
            ref_frame_config->reference[SVC_GOLDEN_FRAME] = 0;
          }
        }
      }
      break;
    case 1:
      // 2-temporal layer.
      //    1    3    5
      //  0    2    4
      // Keep golden fixed at slot 3.
      base_count = superframe_cnt >> 1;
      ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
      // Cyclically refresh slots 5, 6, 7, for lag alt ref.
      lag_index = 5;
      if (base_count > 0) {
        lag_index = 5 + (base_count % 3);
        if (superframe_cnt % 2 != 0) lag_index = 5 + ((base_count + 1) % 3);
      }
      // Set the altref slot to lag_index.
      ref_frame_config->ref_idx[SVC_ALTREF_FRAME] = lag_index;
      if (superframe_cnt % 2 == 0) {
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        // Refresh lag_index slot, needed for lagging golen.
        ref_frame_config->refresh[lag_index] = 1;
        // Refresh GOLDEN every x base layer frames.
        if (base_count % 32 == 0) ref_frame_config->refresh[3] = 1;
      } else {
        layer_id->temporal_layer_id = 1;
        // No updates on layer 1, reference LAST (TL0).
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      }
      // Always reference golden and altref on TL0.
      if (layer_id->temporal_layer_id == 0) {
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
        ref_frame_config->reference[SVC_ALTREF_FRAME] = 1;
      }
      break;
    case 2:
      // 3-temporal layer:
      //   1    3   5    7
      //     2        6
      // 0        4        8
      if (superframe_cnt % 4 == 0) {
        // Base layer.
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 1) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // First top layer: no updates, only reference LAST (TL0).
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 2) % 4 == 0) {
        layer_id->temporal_layer_id = 1;
        // Middle layer (TL1): update LAST2, only reference LAST (TL0).
        ref_frame_config->refresh[1] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 3) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // Second top layer: no updates, only reference LAST.
        // Set buffer idx for LAST to slot 1, since that was the slot
        // updated in previous frame. So LAST is TL1 frame.
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        ref_frame_config->ref_idx[SVC_LAST2_FRAME] = 0;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      }
      break;
    case 3:
      // 3 TL, same as above, except allow for predicting
      // off 2 more references (GOLDEN and ALTREF), with
      // GOLDEN updated periodically, and ALTREF lagging from
      // LAST from ~4 frames. Both GOLDEN and ALTREF
      // can only be updated on base temporal layer.

      // Keep golden fixed at slot 3.
      ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
      // Cyclically refresh slots 5, 6, 7, for lag altref.
      lag_index = 5;
      if (base_count > 0) {
        lag_index = 5 + (base_count % 3);
        if (superframe_cnt % 4 != 0) lag_index = 5 + ((base_count + 1) % 3);
      }
      // Set the altref slot to lag_index.
      ref_frame_config->ref_idx[SVC_ALTREF_FRAME] = lag_index;
      if (superframe_cnt % 4 == 0) {
        // Base layer.
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        // Refresh GOLDEN every x ~10 base layer frames.
        if (base_count % 10 == 0) ref_frame_config->refresh[3] = 1;
        // Refresh lag_index slot, needed for lagging altref.
        ref_frame_config->refresh[lag_index] = 1;
      } else if ((superframe_cnt - 1) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // First top layer: no updates, only reference LAST (TL0).
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 2) % 4 == 0) {
        layer_id->temporal_layer_id = 1;
        // Middle layer (TL1): update LAST2, only reference LAST (TL0).
        ref_frame_config->refresh[1] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 3) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // Second top layer: no updates, only reference LAST.
        // Set buffer idx for LAST to slot 1, since that was the slot
        // updated in previous frame. So LAST is TL1 frame.
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        ref_frame_config->ref_idx[SVC_LAST2_FRAME] = 0;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      }
      // Every frame can reference GOLDEN AND ALTREF.
      ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
      ref_frame_config->reference[SVC_ALTREF_FRAME] = 1;
      // Allow for compound prediction for LAST-ALTREF and LAST-GOLDEN.
      if (speed >= 7) {
        ref_frame_comp_pred->use_comp_pred[2] = 1;
        ref_frame_comp_pred->use_comp_pred[0] = 1;
      }
      break;
    case 4:
      // 3-temporal layer: but middle layer updates GF, so 2nd TL2 will
      // only reference GF (not LAST). Other frames only reference LAST.
      //   1    3   5    7
      //     2        6
      // 0        4        8
      if (superframe_cnt % 4 == 0) {
        // Base layer.
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, only reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 1) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // First top layer: no updates, only reference LAST (TL0).
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 2) % 4 == 0) {
        layer_id->temporal_layer_id = 1;
        // Middle layer (TL1): update GF, only reference LAST (TL0).
        ref_frame_config->refresh[3] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if ((superframe_cnt - 3) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // Second top layer: no updates, only reference GF.
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
      }
      break;
    case 5:
      // 2 spatial layers, 1 temporal.
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1
        // and GOLDEN to slot 0. Update slot 1 (LAST).
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 0;
        ref_frame_config->refresh[1] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
      }
      break;
    case 6:
      // 3 spatial layers, 1 temporal.
      // Note for this case, we set the buffer idx for all references to be
      // either LAST or GOLDEN, which are always valid references, since decoder
      // will check if any of the 7 references is valid scale in
      // valid_ref_frame_size().
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST. Set all buffer_idx to 0.
        for (i = 0; i < INTER_REFS_PER_FRAME; i++)
          ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1
        // and GOLDEN (and all other refs) to slot 0.
        // Update slot 1 (LAST).
        for (i = 0; i < INTER_REFS_PER_FRAME; i++)
          ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        ref_frame_config->refresh[1] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
      } else if (layer_id->spatial_layer_id == 2) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2
        // and GOLDEN (and all other refs) to slot 1.
        // Update slot 2 (LAST).
        for (i = 0; i < INTER_REFS_PER_FRAME; i++)
          ref_frame_config->ref_idx[i] = 1;
        ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
        ref_frame_config->refresh[2] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
        // For 3 spatial layer case: allow for top spatial layer to use
        // additional temporal reference. Update every 10 frames.
        if (enable_longterm_temporal_ref) {
          ref_frame_config->ref_idx[SVC_ALTREF_FRAME] = REF_FRAMES - 1;
          ref_frame_config->reference[SVC_ALTREF_FRAME] = 1;
          if (base_count % 10 == 0)
            ref_frame_config->refresh[REF_FRAMES - 1] = 1;
        }
      }
      break;
    case 7:
      // 2 spatial and 3 temporal layer.
      ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      if (superframe_cnt % 4 == 0) {
        // Base temporal layer
        layer_id->temporal_layer_id = 0;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST, update LAST
          // Set all buffer_idx to 0
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->refresh[0] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
          ref_frame_config->refresh[1] = 1;
        }
      } else if ((superframe_cnt - 1) % 4 == 0) {
        // First top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 3.
          // No update.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        }
      } else if ((superframe_cnt - 2) % 4 == 0) {
        // Middle temporal enhancement layer.
        layer_id->temporal_layer_id = 1;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST.
          // Set all buffer_idx to 0.
          // Set GOLDEN to slot 5 and update slot 5.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 5 - shift;
          ref_frame_config->refresh[5 - shift] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 5.
          // Set LAST3 to slot 6 and update slot 6.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 5 - shift;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
          ref_frame_config->ref_idx[SVC_LAST3_FRAME] = 6 - shift;
          ref_frame_config->refresh[6 - shift] = 1;
        }
      } else if ((superframe_cnt - 3) % 4 == 0) {
        // Second top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Set LAST to slot 5 and reference LAST.
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to 0.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 5 - shift;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 6,
          // GOLDEN to slot 3. No update.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 6 - shift;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
        }
      }
      break;
    case 8:
      // 3 spatial and 3 temporal layer.
      // Same as case 9 but overalap in the buffer slot updates.
      // (shift = 2). The slots 3 and 4 updated by first TL2 are
      // reused for update in TL1 superframe.
      // Note for this case, frame order hint must be disabled for
      // lower resolutios (operating points > 0) to be decoedable.
    case 9:
      // 3 spatial and 3 temporal layer.
      // No overlap in buffer updates between TL2 and TL1.
      // TL2 updates slot 3 and 4, TL1 updates 5, 6, 7.
      // Set the references via the svc_ref_frame_config control.
      // Always reference LAST.
      ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      if (superframe_cnt % 4 == 0) {
        // Base temporal layer.
        layer_id->temporal_layer_id = 0;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST, update LAST.
          // Set all buffer_idx to 0.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->refresh[0] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 0.
          // Update slot 1 (LAST).
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
          ref_frame_config->refresh[1] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 1.
          // Update slot 2 (LAST).
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
          ref_frame_config->refresh[2] = 1;
        }
      } else if ((superframe_cnt - 1) % 4 == 0) {
        // First top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST (slot 0).
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to slot 0.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 3.
          // Set LAST2 to slot 4 and Update slot 4.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
          ref_frame_config->ref_idx[SVC_LAST2_FRAME] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 4.
          // No update.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 4;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
        }
      } else if ((superframe_cnt - 2) % 4 == 0) {
        // Middle temporal enhancement layer.
        layer_id->temporal_layer_id = 1;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST.
          // Set all buffer_idx to 0.
          // Set GOLDEN to slot 5 and update slot 5.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 5 - shift;
          ref_frame_config->refresh[5 - shift] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 5.
          // Set LAST3 to slot 6 and update slot 6.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 5 - shift;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
          ref_frame_config->ref_idx[SVC_LAST3_FRAME] = 6 - shift;
          ref_frame_config->refresh[6 - shift] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 6.
          // Set LAST3 to slot 7 and update slot 7.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 6 - shift;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
          ref_frame_config->ref_idx[SVC_LAST3_FRAME] = 7 - shift;
          ref_frame_config->refresh[7 - shift] = 1;
        }
      } else if ((superframe_cnt - 3) % 4 == 0) {
        // Second top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Set LAST to slot 5 and reference LAST.
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to 0.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 5 - shift;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 6,
          // GOLDEN to slot 3. Set LAST2 to slot 4 and update slot 4.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 6 - shift;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->ref_idx[SVC_LAST2_FRAME] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 7,
          // GOLDEN to slot 4. No update.
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 7 - shift;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 4;
        }
      }
      break;
    case 11:
      // Simulcast mode for 3 spatial and 3 temporal layers.
      // No inter-layer predicton, only prediction is temporal and single
      // reference (LAST).
      // No overlap in buffer slots between spatial layers. So for example,
      // SL0 only uses slots 0 and 1.
      // SL1 only uses slots 2 and 3.
      // SL2 only uses slots 4 and 5.
      // All 7 references for each inter-frame must only access buffer slots
      // for that spatial layer.
      // On key (super)frames: SL1 and SL2 must have no references set
      // and must refresh all the slots for that layer only (so 2 and 3
      // for SL1, 4 and 5 for SL2). The base SL0 will be labelled internally
      // as a Key frame (refresh all slots). SL1/SL2 will be labelled
      // internally as Intra-only frames that allow that stream to be decoded.
      // These conditions will allow for each spatial stream to be
      // independently decodeable.

      // Initialize all references to 0 (don't use reference).
      for (i = 0; i < INTER_REFS_PER_FRAME; i++)
        ref_frame_config->reference[i] = 0;
      // Initialize as no refresh/update for all slots.
      for (i = 0; i < REF_FRAMES; i++) ref_frame_config->refresh[i] = 0;
      for (i = 0; i < INTER_REFS_PER_FRAME; i++)
        ref_frame_config->ref_idx[i] = 0;

      if (is_key_frame) {
        if (layer_id->spatial_layer_id == 0) {
          // Assign LAST/GOLDEN to slot 0/1.
          // Refesh slots 0 and 1 for SL0.
          // SL0: this will get set to KEY frame internally.
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 0;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 1;
          ref_frame_config->refresh[0] = 1;
          ref_frame_config->refresh[1] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Assign LAST/GOLDEN to slot 2/3.
          // Refesh slots 2 and 3 for SL1.
          // This will get set to Intra-only frame internally.
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 3;
          ref_frame_config->refresh[2] = 1;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Assign LAST/GOLDEN to slot 4/5.
          // Refresh slots 4 and 5 for SL2.
          // This will get set to Intra-only frame internally.
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 4;
          ref_frame_config->ref_idx[SVC_GOLDEN_FRAME] = 5;
          ref_frame_config->refresh[4] = 1;
          ref_frame_config->refresh[5] = 1;
        }
      } else if (superframe_cnt % 4 == 0) {
        // Base temporal layer: TL0
        layer_id->temporal_layer_id = 0;
        if (layer_id->spatial_layer_id == 0) {  // SL0
          // Reference LAST. Assign all references to either slot
          // 0 or 1. Here we assign LAST to slot 0, all others to 1.
          // Update slot 0 (LAST).
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 0;
          ref_frame_config->refresh[0] = 1;
        } else if (layer_id->spatial_layer_id == 1) {  // SL1
          // Reference LAST. Assign all references to either slot
          // 2 or 3. Here we assign LAST to slot 2, all others to 3.
          // Update slot 2 (LAST).
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
          ref_frame_config->refresh[2] = 1;
        } else if (layer_id->spatial_layer_id == 2) {  // SL2
          // Reference LAST. Assign all references to either slot
          // 4 or 5. Here we assign LAST to slot 4, all others to 5.
          // Update slot 4 (LAST).
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 5;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 4;
          ref_frame_config->refresh[4] = 1;
        }
      } else if ((superframe_cnt - 1) % 4 == 0) {
        // First top temporal enhancement layer: TL2
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {  // SL0
          // Reference LAST (slot 0). Assign other references to slot 1.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 0;
        } else if (layer_id->spatial_layer_id == 1) {  // SL1
          // Reference LAST (slot 2). Assign other references to slot 3.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
        } else if (layer_id->spatial_layer_id == 2) {  // SL2
          // Reference LAST (slot 4). Assign other references to slot 4.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 5;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 4;
        }
      } else if ((superframe_cnt - 2) % 4 == 0) {
        // Middle temporal enhancement layer: TL1
        layer_id->temporal_layer_id = 1;
        if (layer_id->spatial_layer_id == 0) {  // SL0
          // Reference LAST (slot 0).
          // Set GOLDEN to slot 1 and update slot 1.
          // This will be used as reference for next TL2.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 0;
          ref_frame_config->refresh[1] = 1;
        } else if (layer_id->spatial_layer_id == 1) {  // SL1
          // Reference LAST (slot 2).
          // Set GOLDEN to slot 3 and update slot 3.
          // This will be used as reference for next TL2.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 2;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 2) {  // SL2
          // Reference LAST (slot 4).
          // Set GOLDEN to slot 5 and update slot 5.
          // This will be used as reference for next TL2.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 5;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 4;
          ref_frame_config->refresh[5] = 1;
        }
      } else if ((superframe_cnt - 3) % 4 == 0) {
        // Second top temporal enhancement layer: TL2
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {  // SL0
          // Reference LAST (slot 1). Assign other references to slot 0.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 1;
        } else if (layer_id->spatial_layer_id == 1) {  // SL1
          // Reference LAST (slot 3). Assign other references to slot 2.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 2;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 3;
        } else if (layer_id->spatial_layer_id == 2) {  // SL2
          // Reference LAST (slot 5). Assign other references to slot 4.
          // No update/refresh on any slots.
          ref_frame_config->reference[SVC_LAST_FRAME] = 1;
          for (i = 0; i < INTER_REFS_PER_FRAME; i++)
            ref_frame_config->ref_idx[i] = 4;
          ref_frame_config->ref_idx[SVC_LAST_FRAME] = 5;
        }
      }
      if (!simulcast_mode && layer_id->spatial_layer_id > 0) {
        // Always reference GOLDEN (inter-layer prediction).
        ref_frame_config->reference[SVC_GOLDEN_FRAME] = 1;
        if (ksvc_mode) {
          // KSVC: only keep the inter-layer reference (GOLDEN) for
          // superframes whose base is key.
          if (!is_key_frame) ref_frame_config->reference[SVC_GOLDEN_FRAME] = 0;
        }
        if (is_key_frame && layer_id->spatial_layer_id > 1) {
          // On superframes whose base is key: remove LAST to avoid prediction
          // off layer two levels below.
          ref_frame_config->reference[SVC_LAST_FRAME] = 0;
        }
      }
      // For 3 spatial layer case 8 (where there is free buffer slot):
      // allow for top spatial layer to use additional temporal reference.
      // Additional reference is only updated on base temporal layer, every
      // 10 TL0 frames here.
      if (!simulcast_mode && enable_longterm_temporal_ref &&
          layer_id->spatial_layer_id == 2 && layering_mode == 8) {
        ref_frame_config->ref_idx[SVC_ALTREF_FRAME] = REF_FRAMES - 1;
        if (!is_key_frame) ref_frame_config->reference[SVC_ALTREF_FRAME] = 1;
        if (base_count % 10 == 0 && layer_id->temporal_layer_id == 0)
          ref_frame_config->refresh[REF_FRAMES - 1] = 1;
      }
      break;
    default: assert(0); die("Error: Unsupported temporal layering mode!\n");
  }
}

#if CONFIG_AV1_DECODER
// Returns whether there is a mismatch between the encoder's new frame and the
// decoder's new frame.
static int test_decode(aom_codec_ctx_t *encoder, aom_codec_ctx_t *decoder,
                       const int frames_out) {
  aom_image_t enc_img, dec_img;
  int mismatch = 0;

  /* Get the internal new frame */
  AOM_CODEC_CONTROL_TYPECHECKED(encoder, AV1_GET_NEW_FRAME_IMAGE, &enc_img);
  AOM_CODEC_CONTROL_TYPECHECKED(decoder, AV1_GET_NEW_FRAME_IMAGE, &dec_img);

#if CONFIG_AV1_HIGHBITDEPTH
  if ((enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) !=
      (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t enc_hbd_img;
      aom_img_alloc(
          &enc_hbd_img,
          static_cast<aom_img_fmt_t>(enc_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH),
          enc_img.d_w, enc_img.d_h, 16);
      aom_img_truncate_16_to_8(&enc_hbd_img, &enc_img);
      enc_img = enc_hbd_img;
    }
    if (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t dec_hbd_img;
      aom_img_alloc(
          &dec_hbd_img,
          static_cast<aom_img_fmt_t>(dec_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH),
          dec_img.d_w, dec_img.d_h, 16);
      aom_img_truncate_16_to_8(&dec_hbd_img, &dec_img);
      dec_img = dec_hbd_img;
    }
  }
#endif

  if (!aom_compare_img(&enc_img, &dec_img)) {
    int y[4], u[4], v[4];
#if CONFIG_AV1_HIGHBITDEPTH
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_find_mismatch_high(&enc_img, &dec_img, y, u, v);
    } else {
      aom_find_mismatch(&enc_img, &dec_img, y, u, v);
    }
#else
    aom_find_mismatch(&enc_img, &dec_img, y, u, v);
#endif
    fprintf(stderr,
            "Encode/decode mismatch on frame %d at"
            " Y[%d, %d] {%d/%d},"
            " U[%d, %d] {%d/%d},"
            " V[%d, %d] {%d/%d}\n",
            frames_out, y[0], y[1], y[2], y[3], u[0], u[1], u[2], u[3], v[0],
            v[1], v[2], v[3]);
    mismatch = 1;
  }

  aom_img_free(&enc_img);
  aom_img_free(&dec_img);
  return mismatch;
}
#endif  // CONFIG_AV1_DECODER

struct psnr_stats {
  // The second element of these arrays is reserved for high bitdepth.
  uint64_t psnr_sse_total[2];
  uint64_t psnr_samples_total[2];
  double psnr_totals[2][4];
  int psnr_count[2];
};

static void show_psnr(struct psnr_stats *psnr_stream, double peak) {
  double ovpsnr;

  if (!psnr_stream->psnr_count[0]) return;

  fprintf(stderr, "\nPSNR (Overall/Avg/Y/U/V)");
  ovpsnr = sse_to_psnr((double)psnr_stream->psnr_samples_total[0], peak,
                       (double)psnr_stream->psnr_sse_total[0]);
  fprintf(stderr, " %.3f", ovpsnr);

  for (int i = 0; i < 4; i++) {
    fprintf(stderr, " %.3f",
            psnr_stream->psnr_totals[0][i] / psnr_stream->psnr_count[0]);
  }
  fprintf(stderr, "\n");
}

static aom::AV1RateControlRtcConfig create_rtc_rc_config(
    const aom_codec_enc_cfg_t &cfg, const AppInput &app_input) {
  aom::AV1RateControlRtcConfig rc_cfg;
  rc_cfg.width = cfg.g_w;
  rc_cfg.height = cfg.g_h;
  rc_cfg.max_quantizer = cfg.rc_max_quantizer;
  rc_cfg.min_quantizer = cfg.rc_min_quantizer;
  rc_cfg.target_bandwidth = cfg.rc_target_bitrate;
  rc_cfg.buf_initial_sz = cfg.rc_buf_initial_sz;
  rc_cfg.buf_optimal_sz = cfg.rc_buf_optimal_sz;
  rc_cfg.buf_sz = cfg.rc_buf_sz;
  rc_cfg.overshoot_pct = cfg.rc_overshoot_pct;
  rc_cfg.undershoot_pct = cfg.rc_undershoot_pct;
  // This is hardcoded as AOME_SET_MAX_INTRA_BITRATE_PCT
  rc_cfg.max_intra_bitrate_pct = 300;
  rc_cfg.framerate = cfg.g_timebase.den;
  // TODO(jianj): Add suppor for SVC.
  rc_cfg.ss_number_layers = 1;
  rc_cfg.ts_number_layers = 1;
  rc_cfg.scaling_factor_num[0] = 1;
  rc_cfg.scaling_factor_den[0] = 1;
  rc_cfg.layer_target_bitrate[0] = static_cast<int>(rc_cfg.target_bandwidth);
  rc_cfg.max_quantizers[0] = rc_cfg.max_quantizer;
  rc_cfg.min_quantizers[0] = rc_cfg.min_quantizer;
  rc_cfg.aq_mode = app_input.aq_mode;

  return rc_cfg;
}

static int qindex_to_quantizer(int qindex) {
  // Table that converts 0-63 range Q values passed in outside to the 0-255
  // range Qindex used internally.
  static const int quantizer_to_qindex[] = {
    0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
    52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
    104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
    156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
    208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
  };
  for (int quantizer = 0; quantizer < 64; ++quantizer)
    if (quantizer_to_qindex[quantizer] >= qindex) return quantizer;

  return 63;
}

static void set_active_map(const aom_codec_enc_cfg_t *cfg,
                           aom_codec_ctx_t *codec, int frame_cnt) {
  aom_active_map_t map = { 0, 0, 0 };

  map.rows = (cfg->g_h + 15) / 16;
  map.cols = (cfg->g_w + 15) / 16;

  map.active_map = (uint8_t *)malloc(map.rows * map.cols);
  if (!map.active_map) die("Failed to allocate active map");

  // Example map for testing.
  for (unsigned int i = 0; i < map.rows; ++i) {
    for (unsigned int j = 0; j < map.cols; ++j) {
      int index = map.cols * i + j;
      map.active_map[index] = 1;
      if (frame_cnt < 300) {
        if (i < map.rows / 2 && j < map.cols / 2) map.active_map[index] = 0;
      } else if (frame_cnt >= 300) {
        if (i < map.rows / 2 && j >= map.cols / 2) map.active_map[index] = 0;
      }
    }
  }

  if (aom_codec_control(codec, AOME_SET_ACTIVEMAP, &map))
    die_codec(codec, "Failed to set active map");

  free(map.active_map);
}

int main(int argc, const char **argv) {
  AppInput app_input;
  AvxVideoWriter *outfile[AOM_MAX_LAYERS] = { NULL };
  FILE *obu_files[AOM_MAX_LAYERS] = { NULL };
  AvxVideoWriter *total_layer_file = NULL;
  FILE *total_layer_obu_file = NULL;
  aom_codec_enc_cfg_t cfg;
  int frame_cnt = 0;
  aom_image_t raw;
  int frame_avail;
  int got_data = 0;
  int flags = 0;
  int i;
  int pts = 0;             // PTS starts at 0.
  int frame_duration = 1;  // 1 timebase tick per frame.
  aom_svc_layer_id_t layer_id;
  aom_svc_params_t svc_params;
  aom_svc_ref_frame_config_t ref_frame_config;
  aom_svc_ref_frame_comp_pred_t ref_frame_comp_pred;

#if CONFIG_INTERNAL_STATS
  FILE *stats_file = fopen("opsnr.stt", "a");
  if (stats_file == NULL) {
    die("Cannot open opsnr.stt\n");
  }
#endif
#if CONFIG_AV1_DECODER
  aom_codec_ctx_t decoder;
#endif

  struct RateControlMetrics rc;
  int64_t cx_time = 0;
  int64_t cx_time_layer[AOM_MAX_LAYERS];  // max number of layers.
  int frame_cnt_layer[AOM_MAX_LAYERS];
  double sum_bitrate = 0.0;
  double sum_bitrate2 = 0.0;
  double framerate = 30.0;
  int use_svc_control = 1;
  int set_err_resil_frame = 0;
  int test_changing_bitrate = 0;
  zero(rc.layer_target_bitrate);
  memset(&layer_id, 0, sizeof(aom_svc_layer_id_t));
  memset(&app_input, 0, sizeof(AppInput));
  memset(&svc_params, 0, sizeof(svc_params));

  // Flag to test dynamic scaling of source frames for single
  // spatial stream, using the scaling_mode control.
  const int test_dynamic_scaling_single_layer = 0;

  // Flag to test setting speed per layer.
  const int test_speed_per_layer = 0;

  // Flag for testing active maps.
  const int test_active_maps = 0;

  /* Setup default input stream settings */
  app_input.input_ctx.framerate.numerator = 30;
  app_input.input_ctx.framerate.denominator = 1;
  app_input.input_ctx.only_i420 = 0;
  app_input.input_ctx.bit_depth = AOM_BITS_8;
  app_input.speed = 7;
  exec_name = argv[0];

  // start with default encoder configuration
  aom_codec_err_t res = aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg,
                                                     AOM_USAGE_REALTIME);
  if (res != AOM_CODEC_OK) {
    die("Failed to get config: %s\n", aom_codec_err_to_string(res));
  }

  // Real time parameters.
  cfg.g_usage = AOM_USAGE_REALTIME;

  cfg.rc_end_usage = AOM_CBR;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 52;
  cfg.rc_undershoot_pct = 50;
  cfg.rc_overshoot_pct = 50;
  cfg.rc_buf_initial_sz = 600;
  cfg.rc_buf_optimal_sz = 600;
  cfg.rc_buf_sz = 1000;
  cfg.rc_resize_mode = 0;  // Set to RESIZE_DYNAMIC for dynamic resize.
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_AUTO;

  parse_command_line(argc, argv, &app_input, &svc_params, &cfg);

  int ts_number_layers = svc_params.number_temporal_layers;
  int ss_number_layers = svc_params.number_spatial_layers;

  unsigned int width = cfg.g_w;
  unsigned int height = cfg.g_h;

  if (app_input.layering_mode >= 0) {
    if (ts_number_layers !=
            mode_to_num_temporal_layers[app_input.layering_mode] ||
        ss_number_layers !=
            mode_to_num_spatial_layers[app_input.layering_mode]) {
      die("Number of layers doesn't match layering mode.");
    }
  }

  // Y4M reader has its own allocation.
  if (app_input.input_ctx.file_type != FILE_TYPE_Y4M) {
    if (!aom_img_alloc(&raw, AOM_IMG_FMT_I420, width, height, 32)) {
      die("Failed to allocate image (%dx%d)", width, height);
    }
  }

  aom_codec_iface_t *encoder = aom_codec_av1_cx();

  memcpy(&rc.layer_target_bitrate[0], &svc_params.layer_target_bitrate[0],
         sizeof(svc_params.layer_target_bitrate));

  unsigned int total_rate = 0;
  for (i = 0; i < ss_number_layers; i++) {
    total_rate +=
        svc_params
            .layer_target_bitrate[i * ts_number_layers + ts_number_layers - 1];
  }
  if (total_rate != cfg.rc_target_bitrate) {
    die("Incorrect total target bitrate");
  }

  svc_params.framerate_factor[0] = 1;
  if (ts_number_layers == 2) {
    svc_params.framerate_factor[0] = 2;
    svc_params.framerate_factor[1] = 1;
  } else if (ts_number_layers == 3) {
    svc_params.framerate_factor[0] = 4;
    svc_params.framerate_factor[1] = 2;
    svc_params.framerate_factor[2] = 1;
  }

  if (app_input.input_ctx.file_type == FILE_TYPE_Y4M) {
    // Override these settings with the info from Y4M file.
    cfg.g_w = app_input.input_ctx.width;
    cfg.g_h = app_input.input_ctx.height;
    // g_timebase is the reciprocal of frame rate.
    cfg.g_timebase.num = app_input.input_ctx.framerate.denominator;
    cfg.g_timebase.den = app_input.input_ctx.framerate.numerator;
  }
  framerate = cfg.g_timebase.den / cfg.g_timebase.num;
  set_rate_control_metrics(&rc, framerate, ss_number_layers, ts_number_layers);

  AvxVideoInfo info;
  info.codec_fourcc = get_fourcc_by_aom_encoder(encoder);
  info.frame_width = cfg.g_w;
  info.frame_height = cfg.g_h;
  info.time_base.numerator = cfg.g_timebase.num;
  info.time_base.denominator = cfg.g_timebase.den;
  // Open an output file for each stream.
  for (int sl = 0; sl < ss_number_layers; ++sl) {
    for (int tl = 0; tl < ts_number_layers; ++tl) {
      i = sl * ts_number_layers + tl;
      char file_name[PATH_MAX];
      snprintf(file_name, sizeof(file_name), "%s_%d.av1",
               app_input.output_filename, i);
      if (app_input.output_obu) {
        obu_files[i] = fopen(file_name, "wb");
        if (!obu_files[i]) die("Failed to open %s for writing", file_name);
      } else {
        outfile[i] = aom_video_writer_open(file_name, kContainerIVF, &info);
        if (!outfile[i]) die("Failed to open %s for writing", file_name);
      }
    }
  }
  if (app_input.output_obu) {
    total_layer_obu_file = fopen(app_input.output_filename, "wb");
    if (!total_layer_obu_file)
      die("Failed to open %s for writing", app_input.output_filename);
  } else {
    total_layer_file =
        aom_video_writer_open(app_input.output_filename, kContainerIVF, &info);
    if (!total_layer_file)
      die("Failed to open %s for writing", app_input.output_filename);
  }

  // Initialize codec.
  aom_codec_ctx_t codec;
  aom_codec_flags_t flag = 0;
  flag |= cfg.g_input_bit_depth == AOM_BITS_8 ? 0 : AOM_CODEC_USE_HIGHBITDEPTH;
  flag |= app_input.show_psnr ? AOM_CODEC_USE_PSNR : 0;
  if (aom_codec_enc_init(&codec, encoder, &cfg, flag))
    die_codec(&codec, "Failed to initialize encoder");

#if CONFIG_AV1_DECODER
  if (app_input.decode) {
    if (aom_codec_dec_init(&decoder, get_aom_decoder_by_index(0), NULL, 0))
      die_codec(&decoder, "Failed to initialize decoder");
  }
#endif

  aom_codec_control(&codec, AOME_SET_CPUUSED, app_input.speed);
  aom_codec_control(&codec, AV1E_SET_AQ_MODE, app_input.aq_mode ? 3 : 0);
  aom_codec_control(&codec, AV1E_SET_GF_CBR_BOOST_PCT, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_CDEF, 1);
  aom_codec_control(&codec, AV1E_SET_LOOPFILTER_CONTROL, 1);
  aom_codec_control(&codec, AV1E_SET_ENABLE_WARPED_MOTION, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_OBMC, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_ORDER_HINT, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_TPL_MODEL, 0);
  aom_codec_control(&codec, AV1E_SET_DELTAQ_MODE, 0);
  aom_codec_control(&codec, AV1E_SET_COEFF_COST_UPD_FREQ, 3);
  aom_codec_control(&codec, AV1E_SET_MODE_COST_UPD_FREQ, 3);
  aom_codec_control(&codec, AV1E_SET_MV_COST_UPD_FREQ, 3);
  aom_codec_control(&codec, AV1E_SET_DV_COST_UPD_FREQ, 3);
  aom_codec_control(&codec, AV1E_SET_CDF_UPDATE_MODE, 1);

  // Settings to reduce key frame encoding time.
  aom_codec_control(&codec, AV1E_SET_ENABLE_CFL_INTRA, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_ANGLE_DELTA, 0);
  aom_codec_control(&codec, AV1E_SET_ENABLE_FILTER_INTRA, 0);
  aom_codec_control(&codec, AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);

  if (cfg.g_threads > 1) {
    aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS,
                      (unsigned int)log2(cfg.g_threads));
  }

  aom_codec_control(&codec, AV1E_SET_TUNE_CONTENT, app_input.tune_content);
  if (app_input.tune_content == AOM_CONTENT_SCREEN) {
    aom_codec_control(&codec, AV1E_SET_ENABLE_PALETTE, 1);
    aom_codec_control(&codec, AV1E_SET_ENABLE_CFL_INTRA, 1);
    // INTRABC is currently disabled for rt mode, as it's too slow.
    aom_codec_control(&codec, AV1E_SET_ENABLE_INTRABC, 0);
  }

  if (app_input.use_external_rc) {
    aom_codec_control(&codec, AV1E_SET_RTC_EXTERNAL_RC, 1);
  }

  aom_codec_control(&codec, AV1E_SET_MAX_CONSEC_FRAME_DROP_CBR, INT_MAX);

  aom_codec_control(&codec, AV1E_SET_SVC_FRAME_DROP_MODE,
                    AOM_FULL_SUPERFRAME_DROP);

  svc_params.number_spatial_layers = ss_number_layers;
  svc_params.number_temporal_layers = ts_number_layers;
  for (i = 0; i < ss_number_layers * ts_number_layers; ++i) {
    svc_params.max_quantizers[i] = cfg.rc_max_quantizer;
    svc_params.min_quantizers[i] = cfg.rc_min_quantizer;
  }
  for (i = 0; i < ss_number_layers; ++i) {
    svc_params.scaling_factor_num[i] = 1;
    svc_params.scaling_factor_den[i] = 1;
  }
  if (ss_number_layers == 2) {
    svc_params.scaling_factor_num[0] = 1;
    svc_params.scaling_factor_den[0] = 2;
  } else if (ss_number_layers == 3) {
    svc_params.scaling_factor_num[0] = 1;
    svc_params.scaling_factor_den[0] = 4;
    svc_params.scaling_factor_num[1] = 1;
    svc_params.scaling_factor_den[1] = 2;
  }
  aom_codec_control(&codec, AV1E_SET_SVC_PARAMS, &svc_params);
  // TODO(aomedia:3032): Configure KSVC in fixed mode.

  // This controls the maximum target size of the key frame.
  // For generating smaller key frames, use a smaller max_intra_size_pct
  // value, like 100 or 200.
  {
    const int max_intra_size_pct = 300;
    aom_codec_control(&codec, AOME_SET_MAX_INTRA_BITRATE_PCT,
                      max_intra_size_pct);
  }

  for (int lx = 0; lx < ts_number_layers * ss_number_layers; lx++) {
    cx_time_layer[lx] = 0;
    frame_cnt_layer[lx] = 0;
  }

  std::unique_ptr<aom::AV1RateControlRTC> rc_api;
  if (app_input.use_external_rc) {
    const aom::AV1RateControlRtcConfig rc_cfg =
        create_rtc_rc_config(cfg, app_input);
    rc_api = aom::AV1RateControlRTC::Create(rc_cfg);
  }

  frame_avail = 1;
  struct psnr_stats psnr_stream;
  memset(&psnr_stream, 0, sizeof(psnr_stream));
  while (frame_avail || got_data) {
    struct aom_usec_timer timer;
    frame_avail = read_frame(&(app_input.input_ctx), &raw);
    // Loop over spatial layers.
    for (int slx = 0; slx < ss_number_layers; slx++) {
      aom_codec_iter_t iter = NULL;
      const aom_codec_cx_pkt_t *pkt;
      int layer = 0;
      // Flag for superframe whose base is key.
      int is_key_frame = (frame_cnt % cfg.kf_max_dist) == 0;
      // For flexible mode:
      if (app_input.layering_mode >= 0) {
        // Set the reference/update flags, layer_id, and reference_map
        // buffer index.
        set_layer_pattern(app_input.layering_mode, frame_cnt, &layer_id,
                          &ref_frame_config, &ref_frame_comp_pred,
                          &use_svc_control, slx, is_key_frame,
                          (app_input.layering_mode == 10), app_input.speed);
        aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id);
        if (use_svc_control) {
          aom_codec_control(&codec, AV1E_SET_SVC_REF_FRAME_CONFIG,
                            &ref_frame_config);
          aom_codec_control(&codec, AV1E_SET_SVC_REF_FRAME_COMP_PRED,
                            &ref_frame_comp_pred);
        }
        // Set the speed per layer.
        if (test_speed_per_layer) {
          int speed_per_layer = 10;
          if (layer_id.spatial_layer_id == 0) {
            if (layer_id.temporal_layer_id == 0) speed_per_layer = 6;
            if (layer_id.temporal_layer_id == 1) speed_per_layer = 7;
            if (layer_id.temporal_layer_id == 2) speed_per_layer = 8;
          } else if (layer_id.spatial_layer_id == 1) {
            if (layer_id.temporal_layer_id == 0) speed_per_layer = 7;
            if (layer_id.temporal_layer_id == 1) speed_per_layer = 8;
            if (layer_id.temporal_layer_id == 2) speed_per_layer = 9;
          } else if (layer_id.spatial_layer_id == 2) {
            if (layer_id.temporal_layer_id == 0) speed_per_layer = 8;
            if (layer_id.temporal_layer_id == 1) speed_per_layer = 9;
            if (layer_id.temporal_layer_id == 2) speed_per_layer = 10;
          }
          aom_codec_control(&codec, AOME_SET_CPUUSED, speed_per_layer);
        }
      } else {
        // Only up to 3 temporal layers supported in fixed mode.
        // Only need to set spatial and temporal layer_id: reference
        // prediction, refresh, and buffer_idx are set internally.
        layer_id.spatial_layer_id = slx;
        layer_id.temporal_layer_id = 0;
        if (ts_number_layers == 2) {
          layer_id.temporal_layer_id = (frame_cnt % 2) != 0;
        } else if (ts_number_layers == 3) {
          if (frame_cnt % 2 != 0)
            layer_id.temporal_layer_id = 2;
          else if ((frame_cnt > 1) && ((frame_cnt - 2) % 4 == 0))
            layer_id.temporal_layer_id = 1;
        }
        aom_codec_control(&codec, AV1E_SET_SVC_LAYER_ID, &layer_id);
      }

      if (set_err_resil_frame && cfg.g_error_resilient == 0) {
        // Set error_resilient per frame: off/0 for base layer and
        // on/1 for enhancement layer frames.
        // Note that this is can only be done on the fly/per-frame/layer
        // if the config error_resilience is off/0. See the logic for updating
        // in set_encoder_config():
        // tool_cfg->error_resilient_mode =
        //     cfg->g_error_resilient | extra_cfg->error_resilient_mode;
        const int err_resil_mode =
            layer_id.spatial_layer_id > 0 || layer_id.temporal_layer_id > 0;
        aom_codec_control(&codec, AV1E_SET_ERROR_RESILIENT_MODE,
                          err_resil_mode);
      }

      layer = slx * ts_number_layers + layer_id.temporal_layer_id;
      if (frame_avail && slx == 0) ++rc.layer_input_frames[layer];

      if (test_dynamic_scaling_single_layer) {
        // Example to scale source down by 2x2, then 4x4, and then back up to
        // 2x2, and then back to original.
        int frame_2x2 = 200;
        int frame_4x4 = 400;
        int frame_2x2up = 600;
        int frame_orig = 800;
        if (frame_cnt >= frame_2x2 && frame_cnt < frame_4x4) {
          // Scale source down by 2x2.
          struct aom_scaling_mode mode = { AOME_ONETWO, AOME_ONETWO };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        } else if (frame_cnt >= frame_4x4 && frame_cnt < frame_2x2up) {
          // Scale source down by 4x4.
          struct aom_scaling_mode mode = { AOME_ONEFOUR, AOME_ONEFOUR };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        } else if (frame_cnt >= frame_2x2up && frame_cnt < frame_orig) {
          // Source back up to 2x2.
          struct aom_scaling_mode mode = { AOME_ONETWO, AOME_ONETWO };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        } else if (frame_cnt >= frame_orig) {
          // Source back up to original resolution (no scaling).
          struct aom_scaling_mode mode = { AOME_NORMAL, AOME_NORMAL };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        }
        if (frame_cnt == frame_2x2 || frame_cnt == frame_4x4 ||
            frame_cnt == frame_2x2up || frame_cnt == frame_orig) {
          // For dynamic resize testing on single layer: refresh all references
          // on the resized frame: this is to avoid decode error:
          // if resize goes down by >= 4x4 then libaom decoder will throw an
          // error that some reference (even though not used) is beyond the
          // limit size (must be smaller than 4x4).
          for (i = 0; i < REF_FRAMES; i++) ref_frame_config.refresh[i] = 1;
          if (use_svc_control) {
            aom_codec_control(&codec, AV1E_SET_SVC_REF_FRAME_CONFIG,
                              &ref_frame_config);
            aom_codec_control(&codec, AV1E_SET_SVC_REF_FRAME_COMP_PRED,
                              &ref_frame_comp_pred);
          }
        }
      }

      // Change target_bitrate every other frame.
      if (test_changing_bitrate && frame_cnt % 2 == 0) {
        if (frame_cnt < 500)
          cfg.rc_target_bitrate += 10;
        else
          cfg.rc_target_bitrate -= 10;
        // Do big increase and decrease.
        if (frame_cnt == 100) cfg.rc_target_bitrate <<= 1;
        if (frame_cnt == 600) cfg.rc_target_bitrate >>= 1;
        if (cfg.rc_target_bitrate < 100) cfg.rc_target_bitrate = 100;
        // Call change_config, or bypass with new control.
        // res = aom_codec_enc_config_set(&codec, &cfg);
        if (aom_codec_control(&codec, AV1E_SET_BITRATE_ONE_PASS_CBR,
                              cfg.rc_target_bitrate))
          die_codec(&codec, "Failed to SET_BITRATE_ONE_PASS_CBR");
      }

      if (rc_api) {
        aom::AV1FrameParamsRTC frame_params;
        // TODO(jianj): Add support for SVC.
        frame_params.spatial_layer_id = 0;
        frame_params.temporal_layer_id = 0;
        frame_params.frame_type =
            is_key_frame ? aom::kKeyFrame : aom::kInterFrame;
        rc_api->ComputeQP(frame_params);
        const int current_qp = rc_api->GetQP();
        if (aom_codec_control(&codec, AV1E_SET_QUANTIZER_ONE_PASS,
                              qindex_to_quantizer(current_qp))) {
          die_codec(&codec, "Failed to SET_QUANTIZER_ONE_PASS");
        }
      }

      if (test_active_maps) set_active_map(&cfg, &codec, frame_cnt);

      // Do the layer encode.
      aom_usec_timer_start(&timer);
      if (aom_codec_encode(&codec, frame_avail ? &raw : NULL, pts, 1, flags))
        die_codec(&codec, "Failed to encode frame");
      aom_usec_timer_mark(&timer);
      cx_time += aom_usec_timer_elapsed(&timer);
      cx_time_layer[layer] += aom_usec_timer_elapsed(&timer);
      frame_cnt_layer[layer] += 1;

      got_data = 0;
      // For simulcast (mode 11): write out each spatial layer to the file.
      int ss_layers_write = (app_input.layering_mode == 11)
                                ? layer_id.spatial_layer_id + 1
                                : ss_number_layers;
      while ((pkt = aom_codec_get_cx_data(&codec, &iter))) {
        switch (pkt->kind) {
          case AOM_CODEC_CX_FRAME_PKT:
            for (int sl = layer_id.spatial_layer_id; sl < ss_layers_write;
                 ++sl) {
              for (int tl = layer_id.temporal_layer_id; tl < ts_number_layers;
                   ++tl) {
                int j = sl * ts_number_layers + tl;
                if (app_input.output_obu) {
                  fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                         obu_files[j]);
                } else {
                  aom_video_writer_write_frame(
                      outfile[j],
                      reinterpret_cast<const uint8_t *>(pkt->data.frame.buf),
                      pkt->data.frame.sz, pts);
                }
                if (sl == layer_id.spatial_layer_id)
                  rc.layer_encoding_bitrate[j] += 8.0 * pkt->data.frame.sz;
              }
            }
            got_data = 1;
            // Write everything into the top layer.
            if (app_input.output_obu) {
              fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                     total_layer_obu_file);
            } else {
              aom_video_writer_write_frame(
                  total_layer_file,
                  reinterpret_cast<const uint8_t *>(pkt->data.frame.buf),
                  pkt->data.frame.sz, pts);
            }
            // Keep count of rate control stats per layer (for non-key).
            if (!(pkt->data.frame.flags & AOM_FRAME_IS_KEY)) {
              int j = layer_id.spatial_layer_id * ts_number_layers +
                      layer_id.temporal_layer_id;
              assert(j >= 0);
              rc.layer_avg_frame_size[j] += 8.0 * pkt->data.frame.sz;
              rc.layer_avg_rate_mismatch[j] +=
                  fabs(8.0 * pkt->data.frame.sz - rc.layer_pfb[j]) /
                  rc.layer_pfb[j];
              if (slx == 0) ++rc.layer_enc_frames[layer_id.temporal_layer_id];
            }

            if (rc_api) {
              rc_api->PostEncodeUpdate(pkt->data.frame.sz);
            }
            // Update for short-time encoding bitrate states, for moving window
            // of size rc->window, shifted by rc->window / 2.
            // Ignore first window segment, due to key frame.
            // For spatial layers: only do this for top/highest SL.
            if (frame_cnt > rc.window_size && slx == ss_number_layers - 1) {
              sum_bitrate += 0.001 * 8.0 * pkt->data.frame.sz * framerate;
              rc.window_size = (rc.window_size <= 0) ? 1 : rc.window_size;
              if (frame_cnt % rc.window_size == 0) {
                rc.window_count += 1;
                rc.avg_st_encoding_bitrate += sum_bitrate / rc.window_size;
                rc.variance_st_encoding_bitrate +=
                    (sum_bitrate / rc.window_size) *
                    (sum_bitrate / rc.window_size);
                sum_bitrate = 0.0;
              }
            }
            // Second shifted window.
            if (frame_cnt > rc.window_size + rc.window_size / 2 &&
                slx == ss_number_layers - 1) {
              sum_bitrate2 += 0.001 * 8.0 * pkt->data.frame.sz * framerate;
              if (frame_cnt > 2 * rc.window_size &&
                  frame_cnt % rc.window_size == 0) {
                rc.window_count += 1;
                rc.avg_st_encoding_bitrate += sum_bitrate2 / rc.window_size;
                rc.variance_st_encoding_bitrate +=
                    (sum_bitrate2 / rc.window_size) *
                    (sum_bitrate2 / rc.window_size);
                sum_bitrate2 = 0.0;
              }
            }

#if CONFIG_AV1_DECODER
            if (app_input.decode) {
              if (aom_codec_decode(
                      &decoder,
                      reinterpret_cast<const uint8_t *>(pkt->data.frame.buf),
                      pkt->data.frame.sz, NULL))
                die_codec(&decoder, "Failed to decode frame");
            }
#endif

            break;
          case AOM_CODEC_PSNR_PKT:
            if (app_input.show_psnr) {
              psnr_stream.psnr_sse_total[0] += pkt->data.psnr.sse[0];
              psnr_stream.psnr_samples_total[0] += pkt->data.psnr.samples[0];
              for (int plane = 0; plane < 4; plane++) {
                psnr_stream.psnr_totals[0][plane] += pkt->data.psnr.psnr[plane];
              }
              psnr_stream.psnr_count[0]++;
            }
            break;
          default: break;
        }
      }
#if CONFIG_AV1_DECODER
      if (got_data && app_input.decode) {
        // Don't look for mismatch on top spatial and top temporal layers as
        // they are non reference frames.
        if ((ss_number_layers > 1 || ts_number_layers > 1) &&
            !(layer_id.temporal_layer_id > 0 &&
              layer_id.temporal_layer_id == ts_number_layers - 1)) {
          if (test_decode(&codec, &decoder, frame_cnt)) {
#if CONFIG_INTERNAL_STATS
            fprintf(stats_file, "First mismatch occurred in frame %d\n",
                    frame_cnt);
            fclose(stats_file);
#endif
            fatal("Mismatch seen");
          }
        }
      }
#endif
    }  // loop over spatial layers
    ++frame_cnt;
    pts += frame_duration;
  }

  close_input_file(&(app_input.input_ctx));
  printout_rate_control_summary(&rc, frame_cnt, ss_number_layers,
                                ts_number_layers);

  printf("\n");
  for (int slx = 0; slx < ss_number_layers; slx++)
    for (int tlx = 0; tlx < ts_number_layers; tlx++) {
      int lx = slx * ts_number_layers + tlx;
      printf("Per layer encoding time/FPS stats for encoder: %d %d %d %f %f \n",
             slx, tlx, frame_cnt_layer[lx],
             (float)cx_time_layer[lx] / (double)(frame_cnt_layer[lx] * 1000),
             1000000 * (double)frame_cnt_layer[lx] / (double)cx_time_layer[lx]);
    }

  printf("\n");
  printf("Frame cnt and encoding time/FPS stats for encoding: %d %f %f\n",
         frame_cnt, 1000 * (float)cx_time / (double)(frame_cnt * 1000000),
         1000000 * (double)frame_cnt / (double)cx_time);

  if (app_input.show_psnr) {
    show_psnr(&psnr_stream, 255.0);
  }

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy encoder");

#if CONFIG_AV1_DECODER
  if (app_input.decode) {
    if (aom_codec_destroy(&decoder))
      die_codec(&decoder, "Failed to destroy decoder");
  }
#endif

#if CONFIG_INTERNAL_STATS
  fprintf(stats_file, "No mismatch detected in recon buffers\n");
  fclose(stats_file);
#endif

  // Try to rewrite the output file headers with the actual frame count.
  for (i = 0; i < ss_number_layers * ts_number_layers; ++i)
    aom_video_writer_close(outfile[i]);
  aom_video_writer_close(total_layer_file);

  if (app_input.input_ctx.file_type != FILE_TYPE_Y4M) {
    aom_img_free(&raw);
  }
  return EXIT_SUCCESS;
}
