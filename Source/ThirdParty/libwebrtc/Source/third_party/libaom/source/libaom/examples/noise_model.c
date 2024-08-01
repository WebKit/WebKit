/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\file
 * \brief This is an sample binary to create noise params from input video.
 *
 * To allow for external denoising applications, this sample binary illustrates
 * how to create a film grain table (film grain params as a function of time)
 * from an input video and its corresponding denoised source.
 *
 * The --output-grain-table file can be passed as input to the encoder (in
 * aomenc this is done through the "--film-grain-table" parameter).
 *
 * As an example, where the input source is an 854x480 yuv420p 8-bit video
 * named "input.854_480.yuv" you would use steps similar to the following:
 *
 * # Run your denoiser (e.g, using hqdn3d filter):
 * ffmpeg -vcodec rawvideo -video_size 854x480 -i input.854_480.yuv \
 *    -vf hqdn3d=5:5:5:5 -vcodec rawvideo -an -f rawvideo \
 *    denoised.854_480.yuv
 *
 * # Model the noise between the denoised version and original source:
 * ./examples/noise_model --fps=25/1 --width=854 --height=480 --i420 \
 *    --input-denoised=denoised.854_480.yuv --input=original.854_480.yuv \
 *    --output-grain-table=film_grain.tbl
 *
 * # Encode with your favorite settings (including the grain table):
 * aomenc --limit=100  --cpu-used=4 --input-bit-depth=8                  \
 *    --i420 -w 854 -h 480 --end-usage=q --cq-level=25 --lag-in-frames=25 \
 *    --auto-alt-ref=2 --bit-depth=8 --film-grain-table=film_grain.tbl \
 *    -o denoised_with_grain_params.ivf denoised.854_480.yuv
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom_dsp/aom_dsp_common.h"

#if CONFIG_AV1_DECODER
#include "av1/decoder/grain_synthesis.h"
#endif

#include "aom_dsp/grain_table.h"
#include "aom_dsp/noise_model.h"
#include "aom_dsp/noise_util.h"
#include "aom_mem/aom_mem.h"
#include "common/args.h"
#include "common/tools_common.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s --input=<input> --input-denoised=<denoised> "
          "--output-grain-table=<outfile> "
          "See comments in noise_model.c for more information.\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static const arg_def_t help =
    ARG_DEF(NULL, "help", 0, "Show usage options and exit");
static const arg_def_t width_arg =
    ARG_DEF("w", "width", 1, "Input width (if rawvideo)");
static const arg_def_t height_arg =
    ARG_DEF("h", "height", 1, "Input height (if rawvideo)");
static const arg_def_t skip_frames_arg =
    ARG_DEF("s", "skip-frames", 1, "Number of frames to skip (default = 1)");
static const arg_def_t fps_arg = ARG_DEF(NULL, "fps", 1, "Frame rate");
static const arg_def_t input_arg = ARG_DEF("-i", "input", 1, "Input filename");
static const arg_def_t output_grain_table_arg =
    ARG_DEF("n", "output-grain-table", 1, "Output noise file");
static const arg_def_t input_denoised_arg =
    ARG_DEF("d", "input-denoised", 1, "Input denoised filename (YUV) only");
static const arg_def_t flat_block_finder_arg =
    ARG_DEF("b", "flat-block-finder", 1, "Run the flat block finder");
static const arg_def_t block_size_arg =
    ARG_DEF("b", "block-size", 1, "Block size");
static const arg_def_t bit_depth_arg =
    ARG_DEF(NULL, "bit-depth", 1, "Bit depth of input");
static const arg_def_t use_i420 =
    ARG_DEF(NULL, "i420", 0, "Input file (and denoised) is I420 (default)");
static const arg_def_t use_i422 =
    ARG_DEF(NULL, "i422", 0, "Input file (and denoised) is I422");
static const arg_def_t use_i444 =
    ARG_DEF(NULL, "i444", 0, "Input file (and denoised) is I444");
static const arg_def_t debug_file_arg =
    ARG_DEF(NULL, "debug-file", 1, "File to output debug info");

typedef struct {
  int width;
  int height;
  struct aom_rational fps;
  const char *input;
  const char *input_denoised;
  const char *output_grain_table;
  int img_fmt;
  int block_size;
  int bit_depth;
  int run_flat_block_finder;
  int force_flat_psd;
  int skip_frames;
  const char *debug_file;
} noise_model_args_t;

static void parse_args(noise_model_args_t *noise_args, char **argv) {
  struct arg arg;
  static const arg_def_t *main_args[] = { &help,
                                          &input_arg,
                                          &fps_arg,
                                          &width_arg,
                                          &height_arg,
                                          &block_size_arg,
                                          &output_grain_table_arg,
                                          &input_denoised_arg,
                                          &use_i420,
                                          &use_i422,
                                          &use_i444,
                                          &debug_file_arg,
                                          NULL };
  for (; *argv; argv++) {
    if (arg_match(&arg, &help, argv)) {
      fprintf(stdout, "\nOptions:\n");
      arg_show_usage(stdout, main_args);
      exit(0);
    } else if (arg_match(&arg, &width_arg, argv)) {
      noise_args->width = atoi(arg.val);
    } else if (arg_match(&arg, &height_arg, argv)) {
      noise_args->height = atoi(arg.val);
    } else if (arg_match(&arg, &input_arg, argv)) {
      noise_args->input = arg.val;
    } else if (arg_match(&arg, &input_denoised_arg, argv)) {
      noise_args->input_denoised = arg.val;
    } else if (arg_match(&arg, &output_grain_table_arg, argv)) {
      noise_args->output_grain_table = arg.val;
    } else if (arg_match(&arg, &block_size_arg, argv)) {
      noise_args->block_size = atoi(arg.val);
    } else if (arg_match(&arg, &bit_depth_arg, argv)) {
      noise_args->bit_depth = atoi(arg.val);
    } else if (arg_match(&arg, &flat_block_finder_arg, argv)) {
      noise_args->run_flat_block_finder = atoi(arg.val);
    } else if (arg_match(&arg, &fps_arg, argv)) {
      noise_args->fps = arg_parse_rational(&arg);
    } else if (arg_match(&arg, &use_i420, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I420;
    } else if (arg_match(&arg, &use_i422, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I422;
    } else if (arg_match(&arg, &use_i444, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I444;
    } else if (arg_match(&arg, &skip_frames_arg, argv)) {
      noise_args->skip_frames = atoi(arg.val);
    } else if (arg_match(&arg, &debug_file_arg, argv)) {
      noise_args->debug_file = arg.val;
    } else {
      fprintf(stdout, "Unknown arg: %s\n\nUsage:\n", *argv);
      arg_show_usage(stdout, main_args);
      exit(0);
    }
  }
  if (noise_args->bit_depth > 8) {
    noise_args->img_fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  }
}

#if CONFIG_AV1_DECODER
static void print_variance_y(FILE *debug_file, aom_image_t *raw,
                             aom_image_t *denoised, const uint8_t *flat_blocks,
                             int block_size, aom_film_grain_t *grain) {
  aom_image_t renoised;
  grain->apply_grain = 1;
  grain->random_seed = 7391;
  grain->bit_depth = raw->bit_depth;
  aom_img_alloc(&renoised, raw->fmt, raw->w, raw->h, 1);

  if (av1_add_film_grain(grain, denoised, &renoised)) {
    fprintf(stderr, "Internal failure in av1_add_film_grain().\n");
    aom_img_free(&renoised);
    return;
  }

  const int num_blocks_w = (raw->w + block_size - 1) / block_size;
  const int num_blocks_h = (raw->h + block_size - 1) / block_size;
  fprintf(debug_file, "x = [");
  for (int by = 0; by < num_blocks_h; by++) {
    for (int bx = 0; bx < num_blocks_w; bx++) {
      double block_mean = 0;
      double noise_std = 0, noise_mean = 0;
      double renoise_std = 0, renoise_mean = 0;
      for (int yi = 0; yi < block_size; ++yi) {
        const int y = by * block_size + yi;
        for (int xi = 0; xi < block_size; ++xi) {
          const int x = bx * block_size + xi;
          const double noise_v = (raw->planes[0][y * raw->stride[0] + x] -
                                  denoised->planes[0][y * raw->stride[0] + x]);
          noise_mean += noise_v;
          noise_std += noise_v * noise_v;

          block_mean += raw->planes[0][y * raw->stride[0] + x];

          const double renoise_v =
              (renoised.planes[0][y * raw->stride[0] + x] -
               denoised->planes[0][y * raw->stride[0] + x]);
          renoise_mean += renoise_v;
          renoise_std += renoise_v * renoise_v;
        }
      }
      int n = (block_size * block_size);
      block_mean /= n;
      noise_mean /= n;
      renoise_mean /= n;
      noise_std = sqrt(noise_std / n - noise_mean * noise_mean);
      renoise_std = sqrt(renoise_std / n - renoise_mean * renoise_mean);
      fprintf(debug_file, "%d %3.2lf %3.2lf %3.2lf  ",
              flat_blocks[by * num_blocks_w + bx], block_mean, noise_std,
              renoise_std);
    }
    fprintf(debug_file, "\n");
  }
  fprintf(debug_file, "];\n");

  if (raw->fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    fprintf(stderr,
            "Detailed debug info not supported for high bit"
            "depth formats\n");
  } else {
    fprintf(debug_file, "figure(2); clf;\n");
    fprintf(debug_file,
            "scatter(x(:, 2:4:end), x(:, 3:4:end), 'r'); hold on;\n");
    fprintf(debug_file, "scatter(x(:, 2:4:end), x(:, 4:4:end), 'b');\n");
    fprintf(debug_file,
            "plot(linspace(0, 255, length(noise_strength_0)), "
            "noise_strength_0, 'b');\n");
    fprintf(debug_file,
            "title('Scatter plot of intensity vs noise strength');\n");
    fprintf(debug_file,
            "legend('Actual', 'Estimated', 'Estimated strength');\n");
    fprintf(debug_file, "figure(3); clf;\n");
    fprintf(debug_file, "scatter(x(:, 3:4:end), x(:, 4:4:end), 'k');\n");
    fprintf(debug_file, "title('Actual vs Estimated');\n");
    fprintf(debug_file, "pause(3);\n");
  }
  aom_img_free(&renoised);
}
#endif

static void print_debug_info(FILE *debug_file, aom_image_t *raw,
                             aom_image_t *denoised, uint8_t *flat_blocks,
                             int block_size, aom_noise_model_t *noise_model) {
  (void)raw;
  (void)denoised;
  (void)flat_blocks;
  (void)block_size;
  fprintf(debug_file, "figure(3); clf;\n");
  fprintf(debug_file, "figure(2); clf;\n");
  fprintf(debug_file, "figure(1); clf;\n");
  for (int c = 0; c < 3; ++c) {
    fprintf(debug_file, "noise_strength_%d = [\n", c);
    const aom_equation_system_t *eqns =
        &noise_model->combined_state[c].strength_solver.eqns;
    for (int k = 0; k < eqns->n; ++k) {
      fprintf(debug_file, "%lf ", eqns->x[k]);
    }
    fprintf(debug_file, "];\n");
    fprintf(debug_file, "plot(noise_strength_%d); hold on;\n", c);
  }
  fprintf(debug_file, "legend('Y', 'cb', 'cr');\n");
  fprintf(debug_file, "title('Noise strength function');\n");

#if CONFIG_AV1_DECODER
  aom_film_grain_t grain;
  aom_noise_model_get_grain_parameters(noise_model, &grain);
  print_variance_y(debug_file, raw, denoised, flat_blocks, block_size, &grain);
#endif
  fflush(debug_file);
}

int main(int argc, char *argv[]) {
  noise_model_args_t args = { 0,  0, { 25, 1 }, 0, 0, 0,   AOM_IMG_FMT_I420,
                              32, 8, 1,         0, 1, NULL };
  aom_image_t raw, denoised;
  FILE *infile = NULL;
  AvxVideoInfo info;

  memset(&info, 0, sizeof(info));

  (void)argc;
  exec_name = argv[0];
  parse_args(&args, argv + 1);

  info.frame_width = args.width;
  info.frame_height = args.height;
  info.time_base.numerator = args.fps.den;
  info.time_base.denominator = args.fps.num;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }
  if (!aom_img_alloc(&raw, args.img_fmt, info.frame_width, info.frame_height,
                     1)) {
    die("Failed to allocate image.");
  }
  if (!aom_img_alloc(&denoised, args.img_fmt, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image.");
  }
  infile = fopen(args.input, "rb");
  if (!infile) {
    die("Failed to open input file: %s", args.input);
  }
  fprintf(stderr, "Bit depth: %d  stride:%d\n", args.bit_depth, raw.stride[0]);

  const int high_bd = args.bit_depth > 8;
  const int block_size = args.block_size;
  aom_flat_block_finder_t block_finder;
  aom_flat_block_finder_init(&block_finder, block_size, args.bit_depth,
                             high_bd);

  const int num_blocks_w = (info.frame_width + block_size - 1) / block_size;
  const int num_blocks_h = (info.frame_height + block_size - 1) / block_size;
  uint8_t *flat_blocks = (uint8_t *)aom_malloc(num_blocks_w * num_blocks_h);
  if (!flat_blocks) die("Failed to allocate block data.");
  // Sets the random seed on the first entry in the output table
  int16_t random_seed = 7391;
  aom_noise_model_t noise_model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 3, args.bit_depth,
                                      high_bd };
  aom_noise_model_init(&noise_model, params);

  FILE *denoised_file = 0;
  if (args.input_denoised) {
    denoised_file = fopen(args.input_denoised, "rb");
    if (!denoised_file)
      die("Unable to open input_denoised: %s", args.input_denoised);
  } else {
    die("--input-denoised file must be specified");
  }
  FILE *debug_file = 0;
  if (args.debug_file) {
    debug_file = fopen(args.debug_file, "w");
  }
  aom_film_grain_table_t grain_table = { 0, 0 };

  int64_t prev_timestamp = 0;
  int frame_count = 0;
  while (aom_img_read(&raw, infile)) {
    if (args.input_denoised) {
      if (!aom_img_read(&denoised, denoised_file)) {
        die("Unable to read input denoised file");
      }
    }
    if (frame_count % args.skip_frames == 0) {
      int num_flat_blocks = num_blocks_w * num_blocks_h;
      memset(flat_blocks, 1, num_flat_blocks);
      if (args.run_flat_block_finder) {
        memset(flat_blocks, 0, num_flat_blocks);
        num_flat_blocks = aom_flat_block_finder_run(
            &block_finder, raw.planes[0], info.frame_width, info.frame_height,
            info.frame_width, flat_blocks);
        fprintf(stdout, "Num flat blocks %d\n", num_flat_blocks);
      }

      const uint8_t *planes[3] = { raw.planes[0], raw.planes[1],
                                   raw.planes[2] };
      uint8_t *denoised_planes[3] = { denoised.planes[0], denoised.planes[1],
                                      denoised.planes[2] };
      int strides[3] = { raw.stride[0] >> high_bd, raw.stride[1] >> high_bd,
                         raw.stride[2] >> high_bd };
      int chroma_sub[3] = { raw.x_chroma_shift, raw.y_chroma_shift, 0 };

      fprintf(stdout, "Updating noise model...\n");
      aom_noise_status_t status = aom_noise_model_update(
          &noise_model, (const uint8_t *const *)planes,
          (const uint8_t *const *)denoised_planes, info.frame_width,
          info.frame_height, strides, chroma_sub, flat_blocks, block_size);

      int64_t cur_timestamp =
          frame_count * 10000000ULL * args.fps.den / args.fps.num;
      if (status == AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE) {
        fprintf(stdout,
                "Noise type is different, updating parameters for time "
                "[ %" PRId64 ", %" PRId64 ")\n",
                prev_timestamp, cur_timestamp);
        aom_film_grain_t grain;
        aom_noise_model_get_grain_parameters(&noise_model, &grain);
        grain.random_seed = random_seed;
        random_seed = 0;
        aom_film_grain_table_append(&grain_table, prev_timestamp, cur_timestamp,
                                    &grain);
        aom_noise_model_save_latest(&noise_model);
        prev_timestamp = cur_timestamp;
      }
      if (debug_file) {
        print_debug_info(debug_file, &raw, &denoised, flat_blocks, block_size,
                         &noise_model);
      }
      fprintf(stdout, "Done noise model update, status = %d\n", status);
    }
    frame_count++;
  }

  aom_film_grain_t grain;
  aom_noise_model_get_grain_parameters(&noise_model, &grain);
  grain.random_seed = random_seed;
  aom_film_grain_table_append(&grain_table, prev_timestamp, INT64_MAX, &grain);
  if (args.output_grain_table) {
    struct aom_internal_error_info error_info;
    if (AOM_CODEC_OK != aom_film_grain_table_write(&grain_table,
                                                   args.output_grain_table,
                                                   &error_info)) {
      die("Unable to write output film grain table");
    }
  }
  aom_film_grain_table_free(&grain_table);

  if (infile) fclose(infile);
  if (denoised_file) fclose(denoised_file);
  if (debug_file) fclose(debug_file);
  aom_img_free(&raw);
  aom_img_free(&denoised);

  return EXIT_SUCCESS;
}
