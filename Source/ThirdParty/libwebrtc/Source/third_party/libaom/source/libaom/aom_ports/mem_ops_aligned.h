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

#ifndef AOM_AOM_PORTS_MEM_OPS_ALIGNED_H_
#define AOM_AOM_PORTS_MEM_OPS_ALIGNED_H_

#include "aom/aom_integer.h"

/* \file
 * \brief Provides portable memory access primitives for operating on aligned
 *        data
 *
 * This file is split from mem_ops.h for easier maintenance. See mem_ops.h
 * for a more detailed description of these primitives.
 */
#ifndef INCLUDED_BY_MEM_OPS_H
#error Include mem_ops.h, not mem_ops_aligned.h directly.
#endif

/* Architectures that provide instructions for doing this byte swapping
 * could redefine these macros.
 */
#define swap_endian_16(val, raw)                                     \
  do {                                                               \
    val = (uint16_t)(((raw >> 8) & 0x00ff) | ((raw << 8) & 0xff00)); \
  } while (0)
#define swap_endian_32(val, raw)                                   \
  do {                                                             \
    val = ((raw >> 24) & 0x000000ff) | ((raw >> 8) & 0x0000ff00) | \
          ((raw << 8) & 0x00ff0000) | ((raw << 24) & 0xff000000);  \
  } while (0)
#define swap_endian_16_se(val, raw) \
  do {                              \
    swap_endian_16(val, raw);       \
    val = ((val << 16) >> 16);      \
  } while (0)
#define swap_endian_32_se(val, raw) swap_endian_32(val, raw)

#define mem_get_ne_aligned_generic(end, sz)                           \
  static AOM_INLINE unsigned MEM_VALUE_T mem_get_##end##sz##_aligned( \
      const void *vmem) {                                             \
    const uint##sz##_t *mem = (const uint##sz##_t *)vmem;             \
    return *mem;                                                      \
  }

#define mem_get_sne_aligned_generic(end, sz)                         \
  static AOM_INLINE signed MEM_VALUE_T mem_get_s##end##sz##_aligned( \
      const void *vmem) {                                            \
    const int##sz##_t *mem = (const int##sz##_t *)vmem;              \
    return *mem;                                                     \
  }

#define mem_get_se_aligned_generic(end, sz)                           \
  static AOM_INLINE unsigned MEM_VALUE_T mem_get_##end##sz##_aligned( \
      const void *vmem) {                                             \
    const uint##sz##_t *mem = (const uint##sz##_t *)vmem;             \
    unsigned MEM_VALUE_T val, raw = *mem;                             \
    swap_endian_##sz(val, raw);                                       \
    return val;                                                       \
  }

#define mem_get_sse_aligned_generic(end, sz)                         \
  static AOM_INLINE signed MEM_VALUE_T mem_get_s##end##sz##_aligned( \
      const void *vmem) {                                            \
    const int##sz##_t *mem = (const int##sz##_t *)vmem;              \
    unsigned MEM_VALUE_T val, raw = *mem;                            \
    swap_endian_##sz##_se(val, raw);                                 \
    return val;                                                      \
  }

#define mem_put_ne_aligned_generic(end, sz)                             \
  static AOM_INLINE void mem_put_##end##sz##_aligned(void *vmem,        \
                                                     MEM_VALUE_T val) { \
    uint##sz##_t *mem = (uint##sz##_t *)vmem;                           \
    *mem = (uint##sz##_t)val;                                           \
  }

#define mem_put_se_aligned_generic(end, sz)                             \
  static AOM_INLINE void mem_put_##end##sz##_aligned(void *vmem,        \
                                                     MEM_VALUE_T val) { \
    uint##sz##_t *mem = (uint##sz##_t *)vmem, raw;                      \
    swap_endian_##sz(raw, val);                                         \
    *mem = (uint##sz##_t)raw;                                           \
  }

#include "config/aom_config.h"

#if CONFIG_BIG_ENDIAN
#define mem_get_be_aligned_generic(sz) mem_get_ne_aligned_generic(be, sz)
#define mem_get_sbe_aligned_generic(sz) mem_get_sne_aligned_generic(be, sz)
#define mem_get_le_aligned_generic(sz) mem_get_se_aligned_generic(le, sz)
#define mem_get_sle_aligned_generic(sz) mem_get_sse_aligned_generic(le, sz)
#define mem_put_be_aligned_generic(sz) mem_put_ne_aligned_generic(be, sz)
#define mem_put_le_aligned_generic(sz) mem_put_se_aligned_generic(le, sz)
#else
#define mem_get_be_aligned_generic(sz) mem_get_se_aligned_generic(be, sz)
#define mem_get_sbe_aligned_generic(sz) mem_get_sse_aligned_generic(be, sz)
#define mem_get_le_aligned_generic(sz) mem_get_ne_aligned_generic(le, sz)
#define mem_get_sle_aligned_generic(sz) mem_get_sne_aligned_generic(le, sz)
#define mem_put_be_aligned_generic(sz) mem_put_se_aligned_generic(be, sz)
#define mem_put_le_aligned_generic(sz) mem_put_ne_aligned_generic(le, sz)
#endif

/* clang-format off */
#undef  mem_get_be16_aligned
#define mem_get_be16_aligned mem_ops_wrap_symbol(mem_get_be16_aligned)
mem_get_be_aligned_generic(16)

#undef  mem_get_be32_aligned
#define mem_get_be32_aligned mem_ops_wrap_symbol(mem_get_be32_aligned)
mem_get_be_aligned_generic(32)

#undef  mem_get_le16_aligned
#define mem_get_le16_aligned mem_ops_wrap_symbol(mem_get_le16_aligned)
mem_get_le_aligned_generic(16)

#undef  mem_get_le32_aligned
#define mem_get_le32_aligned mem_ops_wrap_symbol(mem_get_le32_aligned)
mem_get_le_aligned_generic(32)

#undef  mem_get_sbe16_aligned
#define mem_get_sbe16_aligned mem_ops_wrap_symbol(mem_get_sbe16_aligned)
mem_get_sbe_aligned_generic(16)

#undef  mem_get_sbe32_aligned
#define mem_get_sbe32_aligned mem_ops_wrap_symbol(mem_get_sbe32_aligned)
mem_get_sbe_aligned_generic(32)

#undef  mem_get_sle16_aligned
#define mem_get_sle16_aligned mem_ops_wrap_symbol(mem_get_sle16_aligned)
mem_get_sle_aligned_generic(16)

#undef  mem_get_sle32_aligned
#define mem_get_sle32_aligned mem_ops_wrap_symbol(mem_get_sle32_aligned)
mem_get_sle_aligned_generic(32)

#undef  mem_put_be16_aligned
#define mem_put_be16_aligned mem_ops_wrap_symbol(mem_put_be16_aligned)
mem_put_be_aligned_generic(16)

#undef  mem_put_be32_aligned
#define mem_put_be32_aligned mem_ops_wrap_symbol(mem_put_be32_aligned)
mem_put_be_aligned_generic(32)

#undef  mem_put_le16_aligned
#define mem_put_le16_aligned mem_ops_wrap_symbol(mem_put_le16_aligned)
mem_put_le_aligned_generic(16)

#undef  mem_put_le32_aligned
#define mem_put_le32_aligned mem_ops_wrap_symbol(mem_put_le32_aligned)
mem_put_le_aligned_generic(32)

#undef mem_get_ne_aligned_generic
#undef mem_get_se_aligned_generic
#undef mem_get_sne_aligned_generic
#undef mem_get_sse_aligned_generic
#undef mem_put_ne_aligned_generic
#undef mem_put_se_aligned_generic
#undef swap_endian_16
#undef swap_endian_32
#undef swap_endian_16_se
#undef swap_endian_32_se
/* clang-format on */

#endif  // AOM_AOM_PORTS_MEM_OPS_ALIGNED_H_
