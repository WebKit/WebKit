/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_BINARY_CODES_WRITER_H_
#define AOM_AOM_DSP_BINARY_CODES_WRITER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/bitwriter.h"
#include "aom_dsp/bitwriter_buffer.h"

// Codes a symbol v in [-2^mag_bits, 2^mag_bits]
// mag_bits is number of bits for magnitude. The alphabet is of size
// 2 * 2^mag_bits + 1, symmetric around 0, where one bit is used to
// indicate 0 or non-zero, mag_bits bits are used to indicate magnitide
// and 1 more bit for the sign if non-zero.
void aom_write_primitive_symmetric(aom_writer *w, int16_t v,
                                   unsigned int mag_bits);

// Encodes a value v in [0, n-1] quasi-uniformly
void aom_write_primitive_quniform(aom_writer *w, uint16_t n, uint16_t v);

// Finite subexponential code that codes a symbol v in [0, n-1] with parameter k
void aom_write_primitive_subexpfin(aom_writer *w, uint16_t n, uint16_t k,
                                   uint16_t v);

// Finite subexponential code that codes a symbol v in [0, n-1] with parameter k
// based on a reference ref also in [0, n-1].
void aom_write_primitive_refsubexpfin(aom_writer *w, uint16_t n, uint16_t k,
                                      uint16_t ref, uint16_t v);

// Finite subexponential code that codes a symbol v in [-(n-1), n-1] with
// parameter k based on a reference ref also in [-(n-1), n-1].
void aom_write_signed_primitive_refsubexpfin(aom_writer *w, uint16_t n,
                                             uint16_t k, int16_t ref,
                                             int16_t v);

// Functions that counts bits for the above primitives
int aom_count_primitive_symmetric(int16_t v, unsigned int mag_bits);
int aom_count_primitive_quniform(uint16_t n, uint16_t v);
int aom_count_primitive_subexpfin(uint16_t n, uint16_t k, uint16_t v);
int aom_count_primitive_refsubexpfin(uint16_t n, uint16_t k, uint16_t ref,
                                     uint16_t v);
int aom_count_signed_primitive_refsubexpfin(uint16_t n, uint16_t k, int16_t ref,
                                            int16_t v);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_BINARY_CODES_WRITER_H_
