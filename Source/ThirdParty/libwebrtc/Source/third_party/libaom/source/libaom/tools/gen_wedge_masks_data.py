#!/usr/bin/env python3
## Copyright (c) 2026, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
# Precompute libaom AV1 wedge / inter-intra blending masks at codegen time
# so they can live in `.rdata` and be COW shared across processes instead
# of being computed at runtime into writable `.data` buffers.
#
# Output is a header fragment included by av1/common/reconinter.c that
# defines:
#   - wedge_mask_obl            (input to wedge_mask_buf generation)
#   - wedge_mask_buf            (compound prediction masks, indexed by offset)
#   - smooth_interintra_mask_buf
#   - wedge_masks               (offsets into wedge_mask_buf)
#
# The algorithms in this file mirror init_wedge_master_masks(),
# init_wedge_masks() and init_smooth_interintra_masks() in
# av1/common/reconinter.c. They must stay in sync with that source. If those
# helpers, the master tables, the wedge codebooks, the block-size enum, or
# ii_weights1d ever change upstream, regenerate the .inc by running this
# script and committing the result.
#
# Regenerate:
#   python3 source/libaom/tools/gen_wedge_masks_data.py
#       --output source/libaom/av1/common/wedge_masks_data.inc

import argparse
import os
import sys
import textwrap

# -----------------------------------------------------------------------------
# Constants (must match reconinter.h / enums.h).
# -----------------------------------------------------------------------------
MAX_WEDGE_TYPES = 16
MAX_WEDGE_SIZE_LOG2 = 5
MAX_WEDGE_SIZE = 1 << MAX_WEDGE_SIZE_LOG2  # 32
MAX_WEDGE_SQUARE = MAX_WEDGE_SIZE * MAX_WEDGE_SIZE  # 1024
MASK_MASTER_SIZE = MAX_WEDGE_SIZE << 1  # 64
MASK_MASTER_STRIDE = MASK_MASTER_SIZE
WEDGE_WEIGHT_BITS = 6
INTERINTRA_MODES = 4
MAX_SB_SIZE_LOG2 = 7
MAX_SB_SIZE = 1 << MAX_SB_SIZE_LOG2  # 128

# WedgeDirectionType.
WEDGE_HORIZONTAL = 0
WEDGE_VERTICAL = 1
WEDGE_OBLIQUE27 = 2
WEDGE_OBLIQUE63 = 3
WEDGE_OBLIQUE117 = 4
WEDGE_OBLIQUE153 = 5
WEDGE_DIRECTIONS = 6

# BLOCK_SIZE enum order (must match enums.h).
BLOCK_NAMES = [
    "BLOCK_4X4",     "BLOCK_4X8",     "BLOCK_8X4",     "BLOCK_8X8",
    "BLOCK_8X16",    "BLOCK_16X8",    "BLOCK_16X16",   "BLOCK_16X32",
    "BLOCK_32X16",   "BLOCK_32X32",   "BLOCK_32X64",   "BLOCK_64X32",
    "BLOCK_64X64",   "BLOCK_64X128",  "BLOCK_128X64",  "BLOCK_128X128",
    "BLOCK_4X16",    "BLOCK_16X4",    "BLOCK_8X32",    "BLOCK_32X8",
    "BLOCK_16X64",   "BLOCK_64X16",
]
BLOCK_SIZES_ALL = len(BLOCK_NAMES)


def _parse_block_dims(name):
    # e.g. "BLOCK_8X16" -> (8, 16)
    body = name.split("_", 1)[1]
    w, h = body.split("X")
    return int(w), int(h)


BLOCK_WIDTH = [_parse_block_dims(n)[0] for n in BLOCK_NAMES]
BLOCK_HEIGHT = [_parse_block_dims(n)[1] for n in BLOCK_NAMES]


def _bi(name):
    return BLOCK_NAMES.index(name)


# Inter-intra modes (must match enums.h).
II_DC_PRED, II_V_PRED, II_H_PRED, II_SMOOTH_PRED = 0, 1, 2, 3


# -----------------------------------------------------------------------------
# Master mask tables (see wedge_master_oblique_odd / _even / _vertical in
# av1/common/reconinter.c).
# -----------------------------------------------------------------------------
WEDGE_MASTER_OBLIQUE_ODD = [
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  6,  18,
    37, 53, 60, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
]
WEDGE_MASTER_OBLIQUE_EVEN = [
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  4,  11, 27,
    46, 58, 62, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
]
WEDGE_MASTER_VERTICAL = [
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  7,  21,
    43, 57, 62, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
]

# wedge_signflip_lookup (see av1/common/reconinter.c).
WEDGE_SIGNFLIP_LOOKUP = [
    [0]*16, [0]*16, [0]*16,
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_8X8
    [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_8X16
    [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_16X8
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_16X16
    [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_16X32
    [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_32X16
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_32X32
    [0]*16, [0]*16, [0]*16, [0]*16, [0]*16, [0]*16, [0]*16, [0]*16,
    [1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1],  # BLOCK_8X32
    [1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1],  # BLOCK_32X8
    [0]*16, [0]*16,
]
assert len(WEDGE_SIGNFLIP_LOOKUP) == BLOCK_SIZES_ALL


# Wedge codebooks (see wedge_codebook_16_* in av1/common/reconinter.c).
# Each entry is
# (direction, x_offset, y_offset).
WEDGE_CODEBOOK_16_HGTW = [
    (WEDGE_OBLIQUE27, 4, 4),  (WEDGE_OBLIQUE63, 4, 4),
    (WEDGE_OBLIQUE117, 4, 4), (WEDGE_OBLIQUE153, 4, 4),
    (WEDGE_HORIZONTAL, 4, 2), (WEDGE_HORIZONTAL, 4, 4),
    (WEDGE_HORIZONTAL, 4, 6), (WEDGE_VERTICAL, 4, 4),
    (WEDGE_OBLIQUE27, 4, 2),  (WEDGE_OBLIQUE27, 4, 6),
    (WEDGE_OBLIQUE153, 4, 2), (WEDGE_OBLIQUE153, 4, 6),
    (WEDGE_OBLIQUE63, 2, 4),  (WEDGE_OBLIQUE63, 6, 4),
    (WEDGE_OBLIQUE117, 2, 4), (WEDGE_OBLIQUE117, 6, 4),
]
WEDGE_CODEBOOK_16_HLTW = [
    (WEDGE_OBLIQUE27, 4, 4),  (WEDGE_OBLIQUE63, 4, 4),
    (WEDGE_OBLIQUE117, 4, 4), (WEDGE_OBLIQUE153, 4, 4),
    (WEDGE_VERTICAL, 2, 4),   (WEDGE_VERTICAL, 4, 4),
    (WEDGE_VERTICAL, 6, 4),   (WEDGE_HORIZONTAL, 4, 4),
    (WEDGE_OBLIQUE27, 4, 2),  (WEDGE_OBLIQUE27, 4, 6),
    (WEDGE_OBLIQUE153, 4, 2), (WEDGE_OBLIQUE153, 4, 6),
    (WEDGE_OBLIQUE63, 2, 4),  (WEDGE_OBLIQUE63, 6, 4),
    (WEDGE_OBLIQUE117, 2, 4), (WEDGE_OBLIQUE117, 6, 4),
]
WEDGE_CODEBOOK_16_HEQW = [
    (WEDGE_OBLIQUE27, 4, 4),  (WEDGE_OBLIQUE63, 4, 4),
    (WEDGE_OBLIQUE117, 4, 4), (WEDGE_OBLIQUE153, 4, 4),
    (WEDGE_HORIZONTAL, 4, 2), (WEDGE_HORIZONTAL, 4, 6),
    (WEDGE_VERTICAL, 2, 4),   (WEDGE_VERTICAL, 6, 4),
    (WEDGE_OBLIQUE27, 4, 2),  (WEDGE_OBLIQUE27, 4, 6),
    (WEDGE_OBLIQUE153, 4, 2), (WEDGE_OBLIQUE153, 4, 6),
    (WEDGE_OBLIQUE63, 2, 4),  (WEDGE_OBLIQUE63, 6, 4),
    (WEDGE_OBLIQUE117, 2, 4), (WEDGE_OBLIQUE117, 6, 4),
]

# av1_wedge_params_lookup. Stored here as (wtypes, codebook). The masks/
# signflip pointers are reconstructed from BLOCK index. See
# av1_wedge_params_lookup in av1/common/reconinter.c.
WEDGE_PARAMS_LOOKUP = [
    (0, None),
    (0, None),
    (0, None),
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HEQW),  # BLOCK_8X8
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HGTW),  # BLOCK_8X16
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HLTW),  # BLOCK_16X8
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HEQW),  # BLOCK_16X16
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HGTW),  # BLOCK_16X32
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HLTW),  # BLOCK_32X16
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HEQW),  # BLOCK_32X32
    (0, None), (0, None), (0, None), (0, None),
    (0, None), (0, None), (0, None), (0, None),
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HGTW),  # BLOCK_8X32
    (MAX_WEDGE_TYPES, WEDGE_CODEBOOK_16_HLTW),  # BLOCK_32X8
    (0, None), (0, None),
]
assert len(WEDGE_PARAMS_LOOKUP) == BLOCK_SIZES_ALL

# ii_weights1d (see av1/common/reconinter.c).
II_WEIGHTS_1D = [
    60, 58, 56, 54, 52, 50, 48, 47, 45, 44, 42, 41, 39, 38, 37, 35, 34, 33, 32,
    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 22, 21, 20, 19, 19, 18, 18, 17, 16,
    16, 15, 15, 14, 14, 13, 13, 12, 12, 12, 11, 11, 10, 10, 10,  9,  9,  9,  8,
    8,  8,  8,  7,  7,  7,  7,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  4,  4,
    4,  4,  4,  4,  4,  4,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
]
assert len(II_WEIGHTS_1D) == MAX_SB_SIZE

# ii_size_scales (see av1/common/reconinter.c).
II_SIZE_SCALES = [
    32, 16, 16, 16, 8, 8, 8, 4,
    4,  4,  2,  2,  2, 1, 1, 1,
    8,  8,  4,  4,  2, 2,
]
assert len(II_SIZE_SCALES) == BLOCK_SIZES_ALL


# -----------------------------------------------------------------------------
# Algorithm: build wedge_mask_obl (init_wedge_master_masks in reconinter.c).
# -----------------------------------------------------------------------------
def shift_copy(src, dst, dst_off, shift, width):
    """Mirrors shift_copy() in reconinter.c."""
    if shift >= 0:
        for i in range(width - shift):
            dst[dst_off + shift + i] = src[i]
        for i in range(shift):
            dst[dst_off + i] = src[0]
    else:
        n = -shift
        for i in range(width - n):
            dst[dst_off + i] = src[n + i]
        for i in range(n):
            dst[dst_off + width - n + i] = src[width - 1]


def build_wedge_mask_obl():
    # [negative][direction] indexed flat per direction, MASK_MASTER_SIZE^2 bytes.
    obl = [[[0] * (MASK_MASTER_SIZE * MASK_MASTER_SIZE)
            for _ in range(WEDGE_DIRECTIONS)]
           for _ in range(2)]

    w = MASK_MASTER_SIZE
    h = MASK_MASTER_SIZE
    stride = MASK_MASTER_STRIDE
    shift = h // 4
    i = 0
    while i < h:
        shift_copy(WEDGE_MASTER_OBLIQUE_EVEN,
                   obl[0][WEDGE_OBLIQUE63], i * stride, shift, MASK_MASTER_SIZE)
        shift -= 1
        shift_copy(WEDGE_MASTER_OBLIQUE_ODD,
                   obl[0][WEDGE_OBLIQUE63], (i + 1) * stride, shift,
                   MASK_MASTER_SIZE)
        # Vertical rows are just the vertical master copied each row.
        for k in range(MASK_MASTER_SIZE):
            obl[0][WEDGE_VERTICAL][i * stride + k] = WEDGE_MASTER_VERTICAL[k]
            obl[0][WEDGE_VERTICAL][(i + 1) * stride + k] = (
                WEDGE_MASTER_VERTICAL[k])
        i += 2

    max_val = 1 << WEDGE_WEIGHT_BITS  # 64
    for i in range(h):
        for j in range(w):
            msk = obl[0][WEDGE_OBLIQUE63][i * stride + j]
            obl[0][WEDGE_OBLIQUE27][j * stride + i] = msk
            obl[0][WEDGE_OBLIQUE117][i * stride + w - 1 - j] = max_val - msk
            obl[0][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] = max_val - msk
            obl[1][WEDGE_OBLIQUE63][i * stride + j] = max_val - msk
            obl[1][WEDGE_OBLIQUE27][j * stride + i] = max_val - msk
            obl[1][WEDGE_OBLIQUE117][i * stride + w - 1 - j] = msk
            obl[1][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] = msk
            mskx = obl[0][WEDGE_VERTICAL][i * stride + j]
            obl[0][WEDGE_HORIZONTAL][j * stride + i] = mskx
            obl[1][WEDGE_VERTICAL][i * stride + j] = max_val - mskx
            obl[1][WEDGE_HORIZONTAL][j * stride + i] = max_val - mskx

    return obl


# -----------------------------------------------------------------------------
# Algorithm: build wedge_mask_buf and wedge_masks offset table
# (init_wedge_masks + get_wedge_mask_inplace + aom_convolve_copy).
# -----------------------------------------------------------------------------
def get_wedge_mask_inplace_offset(wedge_obl, w_idx, neg, bsize):
    """Compute the starting (row, col) into wedge_mask_obl[neg^wsignflip][dir]
    for a given wedge index/sign/block-size; mirrors get_wedge_mask_inplace().
    Returns (which_neg, direction, row_off, col_off)."""
    bw = BLOCK_WIDTH[bsize]
    bh = BLOCK_HEIGHT[bsize]
    wtypes, codebook = WEDGE_PARAMS_LOOKUP[bsize]
    assert wtypes > 0
    direction, x_off, y_off = codebook[w_idx]
    woff = (x_off * bw) >> 3
    hoff = (y_off * bh) >> 3
    wsignflip = WEDGE_SIGNFLIP_LOOKUP[bsize][w_idx]
    which = neg ^ wsignflip
    row_off = MASK_MASTER_SIZE // 2 - hoff
    col_off = MASK_MASTER_SIZE // 2 - woff
    return which, direction, row_off, col_off


def build_wedge_mask_buf(obl):
    buf_size = 2 * MAX_WEDGE_TYPES * 4 * MAX_WEDGE_SQUARE
    buf = [0] * buf_size

    # Offset table: wedge_masks[bsize][sign][wedge_index] -> uint32 offset.
    offsets = [[[0] * MAX_WEDGE_TYPES for _ in range(2)]
               for _ in range(BLOCK_SIZES_ALL)]

    dst = 0
    for bsize in range(BLOCK_SIZES_ALL):
        wtypes, _ = WEDGE_PARAMS_LOOKUP[bsize]
        if wtypes == 0:
            continue
        bw = BLOCK_WIDTH[bsize]
        bh = BLOCK_HEIGHT[bsize]
        for w_idx in range(wtypes):
            # sign 0
            which, direction, row_off, col_off = (
                get_wedge_mask_inplace_offset(obl, w_idx, 0, bsize))
            src = obl[which][direction]
            for r in range(bh):
                for c in range(bw):
                    buf[dst + r * bw + c] = src[
                        (row_off + r) * MASK_MASTER_STRIDE + (col_off + c)]
            offsets[bsize][0][w_idx] = dst
            dst += bw * bh

            # sign 1
            which, direction, row_off, col_off = (
                get_wedge_mask_inplace_offset(obl, w_idx, 1, bsize))
            src = obl[which][direction]
            for r in range(bh):
                for c in range(bw):
                    buf[dst + r * bw + c] = src[
                        (row_off + r) * MASK_MASTER_STRIDE + (col_off + c)]
            offsets[bsize][1][w_idx] = dst
            dst += bw * bh

    assert dst <= buf_size, (dst, buf_size)
    return buf, offsets


# -----------------------------------------------------------------------------
# Algorithm: build smooth_interintra_mask_buf
# (init_smooth_interintra_masks + build_smooth_interintra_mask).
# -----------------------------------------------------------------------------
def build_smooth_interintra(mask, mask_off, stride, plane_bsize, mode):
    bw = BLOCK_WIDTH[plane_bsize]
    bh = BLOCK_HEIGHT[plane_bsize]
    size_scale = II_SIZE_SCALES[plane_bsize]
    if mode == II_V_PRED:
        for i in range(bh):
            row = mask_off + i * stride
            val = II_WEIGHTS_1D[i * size_scale]
            for j in range(bw):
                mask[row + j] = val
    elif mode == II_H_PRED:
        for i in range(bh):
            row = mask_off + i * stride
            for j in range(bw):
                mask[row + j] = II_WEIGHTS_1D[j * size_scale]
    elif mode == II_SMOOTH_PRED:
        for i in range(bh):
            row = mask_off + i * stride
            for j in range(bw):
                mask[row + j] = II_WEIGHTS_1D[min(i, j) * size_scale]
    else:  # II_DC_PRED or default
        for i in range(bh):
            row = mask_off + i * stride
            for j in range(bw):
                mask[row + j] = 32


def build_smooth_interintra_mask_buf():
    # [INTERINTRA_MODES][BLOCK_SIZES_ALL][MAX_WEDGE_SQUARE]
    buf = [[[0] * MAX_WEDGE_SQUARE for _ in range(BLOCK_SIZES_ALL)]
           for _ in range(INTERINTRA_MODES)]
    for m in range(INTERINTRA_MODES):
        for bs in range(BLOCK_SIZES_ALL):
            bw = BLOCK_WIDTH[bs]
            bh = BLOCK_HEIGHT[bs]
            if bw > MAX_WEDGE_SIZE or bh > MAX_WEDGE_SIZE:
                continue
            build_smooth_interintra(buf[m][bs], 0, bw, bs, m)
    return buf


# -----------------------------------------------------------------------------
# C emission helpers.
# -----------------------------------------------------------------------------
def emit_bytes_block(values, indent="    ", per_line=16):
    lines = []
    for chunk_start in range(0, len(values), per_line):
        chunk = values[chunk_start:chunk_start + per_line]
        parts = ["{:>3d}".format(v) for v in chunk]
        lines.append(indent + ", ".join(parts) + ",")
    return "\n".join(lines)


def emit_wedge_mask_obl(out, obl):
    out.append(
        "DECLARE_ALIGNED(16, static const uint8_t,")
    out.append(
        "                wedge_mask_obl[2][WEDGE_DIRECTIONS]")
    out.append(
        "                              [MASK_MASTER_SIZE * MASK_MASTER_SIZE]) = {")
    for neg in range(2):
        out.append("  {")
        for direction in range(WEDGE_DIRECTIONS):
            out.append("    {{  // [{}][dir={}]".format(neg, direction))
            out.append(emit_bytes_block(obl[neg][direction], indent="      "))
            out.append("    },")
        out.append("  },")
    out.append("};")


def emit_wedge_mask_buf(out, buf):
    out.append(
        "DECLARE_ALIGNED(16, static const uint8_t,")
    out.append(
        "                wedge_mask_buf[2 * MAX_WEDGE_TYPES * 4 *"
        " MAX_WEDGE_SQUARE]) = {")
    out.append(emit_bytes_block(buf, indent="  "))
    out.append("};")


def emit_smooth_interintra(out, buf):
    out.append(
        "DECLARE_ALIGNED(16, static const uint8_t,")
    out.append(
        "                smooth_interintra_mask_buf[INTERINTRA_MODES]")
    out.append(
        "                                          [BLOCK_SIZES_ALL]")
    out.append(
        "                                          [MAX_WEDGE_SQUARE]) = {")
    for m in range(INTERINTRA_MODES):
        out.append("  {")
        for bs in range(BLOCK_SIZES_ALL):
            out.append("    {{  // mode={}, bsize={}".format(m, BLOCK_NAMES[bs]))
            out.append(emit_bytes_block(buf[m][bs], indent="      "))
            out.append("    },")
        out.append("  },")
    out.append("};")


def emit_wedge_offsets(out, offsets):
    out.append(
        "static const wedge_masks_type wedge_masks[BLOCK_SIZES_ALL][2] = {")
    for bs in range(BLOCK_SIZES_ALL):
        out.append("  {  // " + BLOCK_NAMES[bs])
        for sign in range(2):
            row = offsets[bs][sign]
            parts = ["{:>6d}u".format(v) for v in row]
            out.append("    { " + ", ".join(parts) + " },")
        out.append("  },")
    out.append("};")


HEADER_BANNER = """\
/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// This file is auto-generated by tools/gen_wedge_masks_data.py. DO NOT EDIT.
//
// It precomputes the libaom AV1 compound-prediction wedge masks and
// inter-intra smooth blending masks so they can live in `.rdata` and be
// COW-shared across processes, instead of being computed at runtime into
// writable `.data` buffers via av1_init_wedge_masks().
//
// `wedge_masks` stores byte offsets into `wedge_mask_buf` (not pointers) so
// no loader relocations are needed.
//
// To regenerate after upstream changes to reconinter.c (wedge codebooks,
// master tables, BLOCK_SIZE enum, ii_weights1d, etc.) run:
//   python3 source/libaom/tools/gen_wedge_masks_data.py
//       --output source/libaom/av1/common/wedge_masks_data.inc
"""


def write_output(path, obl, buf, smooth, offsets):
    out = []
    out.append(HEADER_BANNER)
    # Disable clang-format on the generated data tables. The emitters below
    # produce a deliberately column-aligned, 16-values-per-line layout so the
    # ~14k lines of mask data are visually scannable (you can eyeball the
    # wedge patterns and offset progressions). clang-format would otherwise
    # reflow each table to fit the project column limit, jumbling rows
    # together, destroying the alignment, and producing a several-thousand-
    # line diff every time anyone re-ran it.
    out.append("// The data tables below are emitted by")
    out.append("// tools/gen_wedge_masks_data.py with a fixed 16-values-per-row")
    out.append("// layout for visual readability. clang-format would reflow")
    out.append("// them to fit the project column limit, destroying the")
    out.append("// alignment and producing a many-thousand-line diff. Keep the")
    out.append("// formatter off until the closing `// clang-format on` below.")
    out.append("// clang-format off")
    out.append("")
    # `wedge_mask_obl` is intentionally not emitted: it was only used at
    # runtime as an intermediate to populate `wedge_mask_buf`, which is now
    # precomputed below. Dropping it saves ~48 KB of .rdata.
    _ = obl  # retained for verification harnesses; unused here.
    emit_wedge_mask_buf(out, buf)
    out.append("")
    emit_smooth_interintra(out, smooth)
    out.append("")
    emit_wedge_offsets(out, offsets)
    out.append("")
    out.append("// clang-format on")
    out.append("")
    text = "\n".join(out)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_out = os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "av1", "common",
                     "wedge_masks_data.inc"))
    parser.add_argument("--output", default=default_out,
                        help="Path to wedge_masks_data.inc (default: %(default)s)")
    args = parser.parse_args()

    obl = build_wedge_mask_obl()
    buf, offsets = build_wedge_mask_buf(obl)
    smooth = build_smooth_interintra_mask_buf()
    write_output(args.output, obl, buf, smooth, offsets)
    print("Wrote {}".format(args.output))


if __name__ == "__main__":
    main()
