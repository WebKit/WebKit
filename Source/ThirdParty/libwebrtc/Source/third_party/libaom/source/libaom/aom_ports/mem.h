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

#ifndef AOM_AOM_PORTS_MEM_H_
#define AOM_AOM_PORTS_MEM_H_

#include "aom/aom_integer.h"
#include "config/aom_config.h"

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif

/* Indicates that the usage of the specified variable has been audited to assure
 * that it's safe to use uninitialized. Silences 'may be used uninitialized'
 * warnings on gcc.
 */
#if defined(__GNUC__) && __GNUC__
#define UNINITIALIZED_IS_SAFE(x) x = x
#else
#define UNINITIALIZED_IS_SAFE(x) x
#endif

#if HAVE_NEON && defined(_MSC_VER)
#define __builtin_prefetch(x)
#endif

/* Shift down with rounding for use when n >= 0. Usually value >= 0, but the
 * macro can be used with a negative value if the direction of rounding is
 * acceptable.
 */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))

/* Shift down with rounding for signed integers, for use when n >= 0 */
#define ROUND_POWER_OF_TWO_SIGNED(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO(-(value), (n)) \
                 : ROUND_POWER_OF_TWO((value), (n)))

/* Shift down with rounding for use when n >= 0 (64-bit value). Usually
 * value >= 0, but the macro can be used with a negative value if the direction
 * of rounding is acceptable.
 */
#define ROUND_POWER_OF_TWO_64(value, n) \
  (((value) + ((((int64_t)1 << (n)) >> 1))) >> (n))
/* Shift down with rounding for signed integers, for use when n >= 0 (64-bit
 * value)
 */
#define ROUND_POWER_OF_TWO_SIGNED_64(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO_64(-(value), (n)) \
                 : ROUND_POWER_OF_TWO_64((value), (n)))

/* Shift down with ceil() for use when n >= 0 and value >= 0.*/
#define CEIL_POWER_OF_TWO(value, n) (((value) + (1 << (n)) - 1) >> (n))

/* shift right or left depending on sign of n */
#define RIGHT_SIGNED_SHIFT(value, n) \
  ((n) < 0 ? ((value) << (-(n))) : ((value) >> (n)))

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

#define DIVIDE_AND_ROUND(x, y) (((x) + ((y) >> 1)) / (y))

#define CONVERT_TO_SHORTPTR(x) ((uint16_t *)(((uintptr_t)(x)) << 1))
#define CONVERT_TO_BYTEPTR(x) ((uint8_t *)(((uintptr_t)(x)) >> 1))

/*!\brief force enum to be unsigned 1 byte*/
#define UENUM1BYTE(enumvar) \
  ;                         \
  typedef uint8_t enumvar

/*!\brief force enum to be signed 1 byte*/
#define SENUM1BYTE(enumvar) \
  ;                         \
  typedef int8_t enumvar

/*!\brief force enum to be unsigned 2 byte*/
#define UENUM2BYTE(enumvar) \
  ;                         \
  typedef uint16_t enumvar

/*!\brief force enum to be signed 2 byte*/
#define SENUM2BYTE(enumvar) \
  ;                         \
  typedef int16_t enumvar

/*!\brief force enum to be unsigned 4 byte*/
#define UENUM4BYTE(enumvar) \
  ;                         \
  typedef uint32_t enumvar

/*!\brief force enum to be unsigned 4 byte*/
#define SENUM4BYTE(enumvar) \
  ;                         \
  typedef int32_t enumvar

#endif  // AOM_AOM_PORTS_MEM_H_
