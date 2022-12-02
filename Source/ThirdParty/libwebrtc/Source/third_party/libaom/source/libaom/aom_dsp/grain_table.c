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

/*!\file
 * \brief This file has the implementation details of the grain table.
 *
 * The file format is an ascii representation for readability and
 * editability. Array parameters are separated from the non-array
 * parameters and prefixed with a few characters to make for easy
 * localization with a parameter set. Each entry is prefixed with "E"
 * and the other parameters are only specified if "update-parms" is
 * non-zero.
 *
 * filmgrn1
 * E <start-time> <end-time> <apply-grain> <random-seed> <update-parms>
 *  p <ar_coeff_lag> <ar_coeff_shift> <grain_scale_shift> ...
 *  sY <num_y_points> <point_0_x> <point_0_y> ...
 *  sCb <num_cb_points> <point_0_x> <point_0_y> ...
 *  sCr <num_cr_points> <point_0_x> <point_0_y> ...
 *  cY <ar_coeff_y_0> ....
 *  cCb <ar_coeff_cb_0> ....
 *  cCr <ar_coeff_cr_0> ....
 * E <start-time> ...
 */
#include <string.h>
#include <stdio.h>
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/grain_table.h"
#include "aom_mem/aom_mem.h"

static const char kFileMagic[8] = "filmgrn1";

static void grain_table_entry_read(FILE *file,
                                   struct aom_internal_error_info *error_info,
                                   aom_film_grain_table_entry_t *entry) {
  aom_film_grain_t *pars = &entry->params;
  int num_read =
      fscanf(file, "E %" PRId64 " %" PRId64 " %d %hd %d\n", &entry->start_time,
             &entry->end_time, &pars->apply_grain, &pars->random_seed,
             &pars->update_parameters);
  if (num_read == 0 && feof(file)) return;
  if (num_read != 5) {
    aom_internal_error(error_info, AOM_CODEC_ERROR,
                       "Unable to read entry header. Read %d != 5", num_read);
    return;
  }
  if (pars->update_parameters) {
    num_read = fscanf(file, "p %d %d %d %d %d %d %d %d %d %d %d %d\n",
                      &pars->ar_coeff_lag, &pars->ar_coeff_shift,
                      &pars->grain_scale_shift, &pars->scaling_shift,
                      &pars->chroma_scaling_from_luma, &pars->overlap_flag,
                      &pars->cb_mult, &pars->cb_luma_mult, &pars->cb_offset,
                      &pars->cr_mult, &pars->cr_luma_mult, &pars->cr_offset);
    if (num_read != 12) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read entry params. Read %d != 12",
                         num_read);
      return;
    }
    if (!fscanf(file, "\tsY %d ", &pars->num_y_points)) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read num y points");
      return;
    }
    for (int i = 0; i < pars->num_y_points; ++i) {
      if (2 != fscanf(file, "%d %d", &pars->scaling_points_y[i][0],
                      &pars->scaling_points_y[i][1])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read y scaling points");
        return;
      }
    }
    if (!fscanf(file, "\n\tsCb %d", &pars->num_cb_points)) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read num cb points");
      return;
    }
    for (int i = 0; i < pars->num_cb_points; ++i) {
      if (2 != fscanf(file, "%d %d", &pars->scaling_points_cb[i][0],
                      &pars->scaling_points_cb[i][1])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read cb scaling points");
        return;
      }
    }
    if (!fscanf(file, "\n\tsCr %d", &pars->num_cr_points)) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read num cr points");
      return;
    }
    for (int i = 0; i < pars->num_cr_points; ++i) {
      if (2 != fscanf(file, "%d %d", &pars->scaling_points_cr[i][0],
                      &pars->scaling_points_cr[i][1])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read cr scaling points");
        return;
      }
    }

    if (fscanf(file, "\n\tcY")) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read Y coeffs header (cY)");
      return;
    }
    const int n = 2 * pars->ar_coeff_lag * (pars->ar_coeff_lag + 1);
    for (int i = 0; i < n; ++i) {
      if (1 != fscanf(file, "%d", &pars->ar_coeffs_y[i])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read Y coeffs");
        return;
      }
    }
    if (fscanf(file, "\n\tcCb")) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable to read Cb coeffs header (cCb)");
      return;
    }
    for (int i = 0; i <= n; ++i) {
      if (1 != fscanf(file, "%d", &pars->ar_coeffs_cb[i])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read Cb coeffs");
        return;
      }
    }
    if (fscanf(file, "\n\tcCr")) {
      aom_internal_error(error_info, AOM_CODEC_ERROR,
                         "Unable read to Cr coeffs header (cCr)");
      return;
    }
    for (int i = 0; i <= n; ++i) {
      if (1 != fscanf(file, "%d", &pars->ar_coeffs_cr[i])) {
        aom_internal_error(error_info, AOM_CODEC_ERROR,
                           "Unable to read Cr coeffs");
        return;
      }
    }
    (void)fscanf(file, "\n");
  }
}

static void grain_table_entry_write(FILE *file,
                                    aom_film_grain_table_entry_t *entry) {
  const aom_film_grain_t *pars = &entry->params;
  fprintf(file, "E %" PRId64 " %" PRId64 " %d %d %d\n", entry->start_time,
          entry->end_time, pars->apply_grain, pars->random_seed,
          pars->update_parameters);
  if (pars->update_parameters) {
    fprintf(file, "\tp %d %d %d %d %d %d %d %d %d %d %d %d\n",
            pars->ar_coeff_lag, pars->ar_coeff_shift, pars->grain_scale_shift,
            pars->scaling_shift, pars->chroma_scaling_from_luma,
            pars->overlap_flag, pars->cb_mult, pars->cb_luma_mult,
            pars->cb_offset, pars->cr_mult, pars->cr_luma_mult,
            pars->cr_offset);
    fprintf(file, "\tsY %d ", pars->num_y_points);
    for (int i = 0; i < pars->num_y_points; ++i) {
      fprintf(file, " %d %d", pars->scaling_points_y[i][0],
              pars->scaling_points_y[i][1]);
    }
    fprintf(file, "\n\tsCb %d", pars->num_cb_points);
    for (int i = 0; i < pars->num_cb_points; ++i) {
      fprintf(file, " %d %d", pars->scaling_points_cb[i][0],
              pars->scaling_points_cb[i][1]);
    }
    fprintf(file, "\n\tsCr %d", pars->num_cr_points);
    for (int i = 0; i < pars->num_cr_points; ++i) {
      fprintf(file, " %d %d", pars->scaling_points_cr[i][0],
              pars->scaling_points_cr[i][1]);
    }
    fprintf(file, "\n\tcY");
    const int n = 2 * pars->ar_coeff_lag * (pars->ar_coeff_lag + 1);
    for (int i = 0; i < n; ++i) {
      fprintf(file, " %d", pars->ar_coeffs_y[i]);
    }
    fprintf(file, "\n\tcCb");
    for (int i = 0; i <= n; ++i) {
      fprintf(file, " %d", pars->ar_coeffs_cb[i]);
    }
    fprintf(file, "\n\tcCr");
    for (int i = 0; i <= n; ++i) {
      fprintf(file, " %d", pars->ar_coeffs_cr[i]);
    }
    fprintf(file, "\n");
  }
}

// TODO(https://crbug.com/aomedia/3228): Update this function to return an
// integer status.
void aom_film_grain_table_append(aom_film_grain_table_t *t, int64_t time_stamp,
                                 int64_t end_time,
                                 const aom_film_grain_t *grain) {
  if (!t->tail || memcmp(grain, &t->tail->params, sizeof(*grain))) {
    aom_film_grain_table_entry_t *new_tail = aom_malloc(sizeof(*new_tail));
    if (!new_tail) return;
    memset(new_tail, 0, sizeof(*new_tail));
    if (t->tail) t->tail->next = new_tail;
    if (!t->head) t->head = new_tail;
    t->tail = new_tail;

    new_tail->start_time = time_stamp;
    new_tail->end_time = end_time;
    new_tail->params = *grain;
  } else {
    t->tail->end_time = AOMMAX(t->tail->end_time, end_time);
    t->tail->start_time = AOMMIN(t->tail->start_time, time_stamp);
  }
}

int aom_film_grain_table_lookup(aom_film_grain_table_t *t, int64_t time_stamp,
                                int64_t end_time, int erase,
                                aom_film_grain_t *grain) {
  aom_film_grain_table_entry_t *entry = t->head;
  aom_film_grain_table_entry_t *prev_entry = NULL;
  uint16_t random_seed = grain ? grain->random_seed : 0;
  if (grain) memset(grain, 0, sizeof(*grain));

  while (entry) {
    aom_film_grain_table_entry_t *next = entry->next;
    if (time_stamp >= entry->start_time && time_stamp < entry->end_time) {
      if (grain) {
        *grain = entry->params;
        if (time_stamp != 0) grain->random_seed = random_seed;
      }
      if (!erase) return 1;

      const int64_t entry_end_time = entry->end_time;
      if (time_stamp <= entry->start_time && end_time >= entry->end_time) {
        if (t->tail == entry) t->tail = prev_entry;
        if (prev_entry) {
          prev_entry->next = entry->next;
        } else {
          t->head = entry->next;
        }
        aom_free(entry);
      } else if (time_stamp <= entry->start_time &&
                 end_time < entry->end_time) {
        entry->start_time = end_time;
      } else if (time_stamp > entry->start_time &&
                 end_time >= entry->end_time) {
        entry->end_time = time_stamp;
      } else {
        aom_film_grain_table_entry_t *new_entry =
            aom_malloc(sizeof(*new_entry));
        if (!new_entry) return 0;
        new_entry->next = entry->next;
        new_entry->start_time = end_time;
        new_entry->end_time = entry->end_time;
        new_entry->params = entry->params;
        entry->next = new_entry;
        entry->end_time = time_stamp;
        if (t->tail == entry) t->tail = new_entry;
      }
      // If segments aren't aligned, delete from the beginning of subsequent
      // segments
      if (end_time > entry_end_time) {
        // Ignoring the return value here is safe since we're erasing from the
        // beginning of subsequent entries.
        aom_film_grain_table_lookup(t, entry_end_time, end_time, /*erase=*/1,
                                    NULL);
      }
      return 1;
    }
    prev_entry = entry;
    entry = next;
  }
  return 0;
}

aom_codec_err_t aom_film_grain_table_read(
    aom_film_grain_table_t *t, const char *filename,
    struct aom_internal_error_info *error_info) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    aom_internal_error(error_info, AOM_CODEC_ERROR, "Unable to open %s",
                       filename);
    return error_info->error_code;
  }
  error_info->error_code = AOM_CODEC_OK;

  // Read in one extra character as there should be white space after
  // the header.
  char magic[9];
  if (!fread(magic, 9, 1, file) || memcmp(magic, kFileMagic, 8)) {
    aom_internal_error(error_info, AOM_CODEC_ERROR,
                       "Unable to read (or invalid) file magic");
    fclose(file);
    return error_info->error_code;
  }

  aom_film_grain_table_entry_t *prev_entry = NULL;
  while (!feof(file)) {
    aom_film_grain_table_entry_t *entry = aom_malloc(sizeof(*entry));
    if (!entry) {
      aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                         "Unable to allocate grain table entry");
      break;
    }
    memset(entry, 0, sizeof(*entry));
    grain_table_entry_read(file, error_info, entry);
    entry->next = NULL;

    if (prev_entry) prev_entry->next = entry;
    if (!t->head) t->head = entry;
    t->tail = entry;
    prev_entry = entry;

    if (error_info->error_code != AOM_CODEC_OK) break;
  }

  fclose(file);
  return error_info->error_code;
}

aom_codec_err_t aom_film_grain_table_write(
    const aom_film_grain_table_t *t, const char *filename,
    struct aom_internal_error_info *error_info) {
  error_info->error_code = AOM_CODEC_OK;

  FILE *file = fopen(filename, "wb");
  if (!file) {
    aom_internal_error(error_info, AOM_CODEC_ERROR, "Unable to open file %s",
                       filename);
    return error_info->error_code;
  }

  if (!fwrite(kFileMagic, 8, 1, file)) {
    aom_internal_error(error_info, AOM_CODEC_ERROR,
                       "Unable to write file magic");
    fclose(file);
    return error_info->error_code;
  }

  fprintf(file, "\n");
  aom_film_grain_table_entry_t *entry = t->head;
  while (entry) {
    grain_table_entry_write(file, entry);
    entry = entry->next;
  }
  fclose(file);
  return error_info->error_code;
}

void aom_film_grain_table_free(aom_film_grain_table_t *t) {
  aom_film_grain_table_entry_t *entry = t->head;
  while (entry) {
    aom_film_grain_table_entry_t *next = entry->next;
    aom_free(entry);
    entry = next;
  }
  memset(t, 0, sizeof(*t));
}
