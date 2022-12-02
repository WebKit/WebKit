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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "av1/common/enums.h"
#include "av1/encoder/encoder.h"
#include "common/args.h"
#include "common/tools_common.h"
#include "common/video_writer.h"
#include "examples/encoder_util.h"
#include "aom_ports/aom_timer.h"

#define OPTION_BUFFER_SIZE 1024

typedef struct {
  const char *output_filename;
  char options[OPTION_BUFFER_SIZE];
  struct AvxInputContext input_ctx;
  int speed;
  int aq_mode;
  int layering_mode;
  int output_obu;
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

#if CONFIG_AV1_HIGHBITDEPTH
static const struct arg_enum_list bitdepth_enum[] = {
  { "8", AOM_BITS_8 }, { "10", AOM_BITS_10 }, { "12", AOM_BITS_12 }, { NULL, 0 }
};

static const arg_def_t bitdepth_arg = ARG_DEF_ENUM(
    "d", "bit-depth", 1, "Bit depth for codec 8, 10 or 12. ", bitdepth_enum);
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
  &error_resilient_arg, &output_obu_arg, NULL
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
      input->bit_depth = input->y4m.bit_depth;
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

  input_string = malloc(strlen(input));
  if (!input_string) die("Failed to allocate input string.");
  memcpy(input_string, input, strlen(input));
  if (input_string == NULL) return AOM_CODEC_MEM_ERROR;
  token = strtok(input_string, delim);  // NOLINT
  for (i = 0; i < num_layers; ++i) {
    if (token != NULL) {
      res = extract_option(type, token, option0 + i, option1 + i);
      if (res != AOM_CODEC_OK) break;
      token = strtok(NULL, delim);  // NOLINT
    } else {
      break;
    }
  }
  if (res == AOM_CODEC_OK && i != num_layers) {
    res = AOM_CODEC_INVALID_PARAM;
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
      if (app_input->speed > 10) {
        aom_tools_warn("Mapping speed %d to speed 10.\n", app_input->speed);
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
      parse_layer_options_from_string(svc_params, SCALE_FACTOR, arg.val,
                                      svc_params->scaling_factor_num,
                                      svc_params->scaling_factor_den);
    } else if (arg_match(&arg, &min_q_arg, argi)) {
      enc_cfg->rc_min_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &max_q_arg, argi)) {
      enc_cfg->rc_max_quantizer = arg_parse_uint(&arg);
#if CONFIG_AV1_HIGHBITDEPTH
    } else if (arg_match(&arg, &bitdepth_arg, argi)) {
      enc_cfg->g_bit_depth = arg_parse_enum_or_int(&arg);
      switch (enc_cfg->g_bit_depth) {
        case AOM_BITS_8:
          enc_cfg->g_input_bit_depth = 8;
          enc_cfg->g_profile = 0;
          break;
        case AOM_BITS_10:
          enc_cfg->g_input_bit_depth = 10;
          enc_cfg->g_profile = 2;
          break;
        case AOM_BITS_12:
          enc_cfg->g_input_bit_depth = 12;
          enc_cfg->g_profile = 2;
          break;
        default:
          die("Error: Invalid bit depth selected (%d)\n", enc_cfg->g_bit_depth);
          break;
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
    } else {
      ++argj;
    }
  }

  // Total bitrate needs to be parsed after the number of layers.
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;
    if (arg_match(&arg, &bitrates_arg, argi)) {
      parse_layer_options_from_string(svc_params, BITRATE, arg.val,
                                      svc_params->layer_target_bitrate, NULL);
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

  open_input_file(&app_input->input_ctx, 0);
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

static unsigned int mode_to_num_temporal_layers[11] = { 1, 2, 3, 3, 2, 1,
                                                        1, 3, 3, 3, 3 };
static unsigned int mode_to_num_spatial_layers[11] = { 1, 1, 1, 1, 1, 2,
                                                       3, 2, 3, 3, 3 };

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
                                     double framerate,
                                     unsigned int ss_number_layers,
                                     unsigned int ts_number_layers) {
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
  for (unsigned int sl = 0; sl < ss_number_layers; ++sl) {
    unsigned int i = sl * ts_number_layers;
    rc->layer_framerate[0] = framerate / ts_rate_decimator[0];
    rc->layer_pfb[i] =
        1000.0 * rc->layer_target_bitrate[i] / rc->layer_framerate[0];
    for (unsigned int tl = 0; tl < ts_number_layers; ++tl) {
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
                                          int frame_cnt,
                                          unsigned int ss_number_layers,
                                          unsigned int ts_number_layers) {
  int tot_num_frames = 0;
  double perc_fluctuation = 0.0;
  printf("Total number of processed frames: %d\n\n", frame_cnt - 1);
  printf("Rate control layer stats for %u layer(s):\n\n", ts_number_layers);
  for (unsigned int sl = 0; sl < ss_number_layers; ++sl) {
    tot_num_frames = 0;
    for (unsigned int tl = 0; tl < ts_number_layers; ++tl) {
      unsigned int i = sl * ts_number_layers + tl;
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
      printf("For layer#: %u %u \n", sl, tl);
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
  int i;
  int enable_longterm_temporal_ref = 1;
  int shift = (layering_mode == 8) ? 2 : 0;
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
      // 1-layer: update LAST on every frame, reference LAST.
      layer_id->temporal_layer_id = 0;
      ref_frame_config->refresh[0] = 1;
      ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      break;
    case 1:
      // 2-temporal layer.
      //    1    3    5
      //  0    2    4
      if (superframe_cnt % 2 == 0) {
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
      } else {
        layer_id->temporal_layer_id = 1;
        // No updates on layer 1, only reference LAST (TL0).
        ref_frame_config->reference[SVC_LAST_FRAME] = 1;
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
      // Allow for compound prediction using LAST and ALTREF.
      if (speed >= 7) ref_frame_comp_pred->use_comp_pred[2] = 1;
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
      if (layer_id->spatial_layer_id > 0) {
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
      if (enable_longterm_temporal_ref && layer_id->spatial_layer_id == 2 &&
          layering_mode == 8) {
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
static void test_decode(aom_codec_ctx_t *encoder, aom_codec_ctx_t *decoder,
                        const int frames_out, int *mismatch_seen) {
  aom_image_t enc_img, dec_img;

  if (*mismatch_seen) return;

  /* Get the internal reference frame */
  AOM_CODEC_CONTROL_TYPECHECKED(encoder, AV1_GET_NEW_FRAME_IMAGE, &enc_img);
  AOM_CODEC_CONTROL_TYPECHECKED(decoder, AV1_GET_NEW_FRAME_IMAGE, &dec_img);

#if CONFIG_AV1_HIGHBITDEPTH
  if ((enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) !=
      (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t enc_hbd_img;
      aom_img_alloc(&enc_hbd_img, enc_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
                    enc_img.d_w, enc_img.d_h, 16);
      aom_img_truncate_16_to_8(&enc_hbd_img, &enc_img);
      enc_img = enc_hbd_img;
    }
    if (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t dec_hbd_img;
      aom_img_alloc(&dec_hbd_img, dec_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
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
    decoder->err = 1;
    printf(
        "Encode/decode mismatch on frame %d at"
        " Y[%d, %d] {%d/%d},"
        " U[%d, %d] {%d/%d},"
        " V[%d, %d] {%d/%d}",
        frames_out, y[0], y[1], y[2], y[3], u[0], u[1], u[2], u[3], v[0], v[1],
        v[2], v[3]);
    *mismatch_seen = frames_out;
  }

  aom_img_free(&enc_img);
  aom_img_free(&dec_img);
}
#endif  // CONFIG_AV1_DECODER

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
  unsigned i;
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
  int mismatch_seen = 0;
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
  zero(rc.layer_target_bitrate);
  memset(&layer_id, 0, sizeof(aom_svc_layer_id_t));
  memset(&app_input, 0, sizeof(AppInput));
  memset(&svc_params, 0, sizeof(svc_params));

  // Flag to test dynamic scaling of source frames for single
  // spatial stream, using the scaling_mode control.
  const int test_dynamic_scaling_single_layer = 0;

  /* Setup default input stream settings */
  app_input.input_ctx.framerate.numerator = 30;
  app_input.input_ctx.framerate.denominator = 1;
  app_input.input_ctx.only_i420 = 1;
  app_input.input_ctx.bit_depth = 0;
  app_input.speed = 7;
  exec_name = argv[0];

  // start with default encoder configuration
  aom_codec_err_t res = aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg,
                                                     AOM_USAGE_REALTIME);
  if (res) {
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

  unsigned int ts_number_layers = svc_params.number_temporal_layers;
  unsigned int ss_number_layers = svc_params.number_spatial_layers;

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

  aom_codec_iface_t *encoder = get_aom_encoder_by_short_name("av1");

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
  for (unsigned int sl = 0; sl < ss_number_layers; ++sl) {
    for (unsigned tl = 0; tl < ts_number_layers; ++tl) {
      i = sl * ts_number_layers + tl;
      char file_name[PATH_MAX];
      snprintf(file_name, sizeof(file_name), "%s_%u.av1",
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
  if (aom_codec_enc_init(&codec, encoder, &cfg, 0))
    die("Failed to initialize encoder");

#if CONFIG_AV1_DECODER
  if (aom_codec_dec_init(&decoder, get_aom_decoder_by_index(0), NULL, 0)) {
    die("Failed to initialize decoder");
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
  aom_codec_control(&codec, AV1E_SET_TILE_COLUMNS,
                    cfg.g_threads ? get_msb(cfg.g_threads) : 0);
  if (cfg.g_threads > 1) aom_codec_control(&codec, AV1E_SET_ROW_MT, 1);

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

  for (unsigned int lx = 0; lx < ts_number_layers * ss_number_layers; lx++) {
    cx_time_layer[lx] = 0;
    frame_cnt_layer[lx] = 0;
  }

  frame_avail = 1;
  while (frame_avail || got_data) {
    struct aom_usec_timer timer;
    frame_avail = read_frame(&(app_input.input_ctx), &raw);
    // Loop over spatial layers.
    for (unsigned int slx = 0; slx < ss_number_layers; slx++) {
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

      if (set_err_resil_frame) {
        // Set error_resilient per frame: off/0 for base layer and
        // on/1 for enhancement layer frames.
        int err_resil_mode =
            (layer_id.spatial_layer_id > 0 || layer_id.temporal_layer_id > 0);
        aom_codec_control(&codec, AV1E_SET_ERROR_RESILIENT_MODE,
                          err_resil_mode);
      }

      layer = slx * ts_number_layers + layer_id.temporal_layer_id;
      if (frame_avail && slx == 0) ++rc.layer_input_frames[layer];

      if (test_dynamic_scaling_single_layer) {
        if (frame_cnt >= 200 && frame_cnt <= 400) {
          // Scale source down by 2x2.
          struct aom_scaling_mode mode = { AOME_ONETWO, AOME_ONETWO };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        } else {
          // Source back up to original resolution (no scaling).
          struct aom_scaling_mode mode = { AOME_NORMAL, AOME_NORMAL };
          aom_codec_control(&codec, AOME_SET_SCALEMODE, &mode);
        }
      }

      // Do the layer encode.
      aom_usec_timer_start(&timer);
      if (aom_codec_encode(&codec, frame_avail ? &raw : NULL, pts, 1, flags))
        die_codec(&codec, "Failed to encode frame");
      aom_usec_timer_mark(&timer);
      cx_time += aom_usec_timer_elapsed(&timer);
      cx_time_layer[layer] += aom_usec_timer_elapsed(&timer);
      frame_cnt_layer[layer] += 1;

      got_data = 0;
      while ((pkt = aom_codec_get_cx_data(&codec, &iter))) {
        got_data = 1;
        switch (pkt->kind) {
          case AOM_CODEC_CX_FRAME_PKT:
            for (unsigned int sl = layer_id.spatial_layer_id;
                 sl < ss_number_layers; ++sl) {
              for (unsigned tl = layer_id.temporal_layer_id;
                   tl < ts_number_layers; ++tl) {
                unsigned int j = sl * ts_number_layers + tl;
                if (app_input.output_obu) {
                  fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                         obu_files[j]);
                } else {
                  aom_video_writer_write_frame(outfile[j], pkt->data.frame.buf,
                                               pkt->data.frame.sz, pts);
                }
                if (sl == (unsigned int)layer_id.spatial_layer_id)
                  rc.layer_encoding_bitrate[j] += 8.0 * pkt->data.frame.sz;
              }
            }
            // Write everything into the top layer.
            if (app_input.output_obu) {
              fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                     total_layer_obu_file);
            } else {
              aom_video_writer_write_frame(total_layer_file,
                                           pkt->data.frame.buf,
                                           pkt->data.frame.sz, pts);
            }
            // Keep count of rate control stats per layer (for non-key).
            if (!(pkt->data.frame.flags & AOM_FRAME_IS_KEY)) {
              unsigned int j = layer_id.spatial_layer_id * ts_number_layers +
                               layer_id.temporal_layer_id;
              rc.layer_avg_frame_size[j] += 8.0 * pkt->data.frame.sz;
              rc.layer_avg_rate_mismatch[j] +=
                  fabs(8.0 * pkt->data.frame.sz - rc.layer_pfb[j]) /
                  rc.layer_pfb[j];
              if (slx == 0) ++rc.layer_enc_frames[layer_id.temporal_layer_id];
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
            if (aom_codec_decode(&decoder, pkt->data.frame.buf,
                                 (unsigned int)pkt->data.frame.sz, NULL))
              die_codec(&decoder, "Failed to decode frame.");
#endif

            break;
          default: break;
        }
      }
#if CONFIG_AV1_DECODER
      // Don't look for mismatch on top spatial and top temporal layers as they
      // are non reference frames.
      if ((ss_number_layers > 1 || ts_number_layers > 1) &&
          !(layer_id.temporal_layer_id > 0 &&
            layer_id.temporal_layer_id == (int)ts_number_layers - 1)) {
        test_decode(&codec, &decoder, frame_cnt, &mismatch_seen);
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
  for (unsigned int slx = 0; slx < ss_number_layers; slx++)
    for (unsigned int tlx = 0; tlx < ts_number_layers; tlx++) {
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

  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");

#if CONFIG_INTERNAL_STATS
  if (mismatch_seen) {
    fprintf(stats_file, "First mismatch occurred in frame %d\n", mismatch_seen);
  } else {
    fprintf(stats_file, "No mismatch detected in recon buffers\n");
  }
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
