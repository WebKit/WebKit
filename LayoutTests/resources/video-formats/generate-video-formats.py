#!/usr/bin/env python3
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

"""Generates the video-formats test videos and video-formats-manifest.js in this directory.

Usage: python3 generate-video-formats.py [name-filter-regexp]

File names: <codec>-<chroma>[-<depth>]-<matrix/transfer>-<range>[-<variant>].<ext>, where the range
suffix is "-videorange" (ffmpeg range=tv, limited) or "-fullrange" (ffmpeg range=pc). The ffmpeg
option values inside this script keep ffmpeg's tv/pc spelling.

Only videos whose name matches the filter are re-encoded; the manifest is always rewritten for
all videos present in this directory. Requires ffmpeg >= 7 (tested with 9.0.2) with libx264,
libx265, libvpx, libvpx-vp9, libsvtav1, prores_ks and the VideoToolbox encoders
(hevc_videotoolbox for the HEVC alpha video, so this has to run on macOS). x264/x265 output is
not bit-exact across versions, so the tests compare against the manifest values with
tolerances, never against hashes.

Pattern v3: 320x192, a 5x3 grid of flat 64x64 patches, 10 fps, 24 frames.

    row 0:  red      (192,64,64)   yellow     (64,192,64)   green
    row 1:  gray16   bit2          bit1       bit0          gray235
    row 2:  blue     (64,64,192)   cyan       gray128       white

bit2..bit0 are white/black bits of frame % 8. Alpha videos: white alpha 128, cyan alpha 0,
(64,64,192) alpha 128, everything else opaque.

The frames are made here as raw RGBA and piped to ffmpeg; swscale converts them with the
matrix/range the video is tagged with. ffmpeg >= 7 takes the colour tags from the frames
(setparams), not from -color_* output options; setparams spells RGB "gbr".

Expected values written to the manifest, per patch:
  native:      the source R'G'B' after the RGB -> Y'CbCr -> R'G'B' round trip with the video's
               matrix, range and bit depth, as 8-bit floats. This is what an un-managed sink
               (WebGL, option A) sees.
  srgb, displayP3: the native values colour-managed from the video's primaries and transfer
               function into sRGB / Display P3 (clipped), for Canvas 2D and WebGPU.
  displayP3ReadAsSrgb: srgb(clip(displayP3)), for a P3 canvas read back as sRGB.
  nativeInDisplayP3: the native values taken as sRGB, converted to Display P3.
PQ and HLG have no defined managed oracle: their srgb/displayP3 are null and "managed" is
"measured" (the tests record measured values with a wide tolerance).
"""

import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FFMPEG = shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg"
FFPROBE = shutil.which("ffprobe") or "/opt/homebrew/bin/ffprobe"

FPS = 10
FRAMES = 24
RAW_FRAMES = 2

COLORS = {
    "red": (255, 0, 0),
    "desat-red": (192, 64, 64),
    "yellow": (255, 255, 0),
    "desat-green": (64, 192, 64),
    "green": (0, 255, 0),
    "gray16": (16, 16, 16),
    "gray235": (235, 235, 235),
    "blue": (0, 0, 255),
    "desat-blue": (64, 64, 192),
    "cyan": (0, 255, 255),
    "gray128": (128, 128, 128),
    "white": (255, 255, 255),
    # The bit patches are black or white.
    "black": (0, 0, 0),
}
ALPHA = {"white": 128, "cyan": 0, "desat-blue": 128}

GRID_V3 = [
    ["red", "desat-red", "yellow", "desat-green", "green"],
    ["gray16", "bit2", "bit1", "bit0", "gray235"],
    ["blue", "desat-blue", "cyan", "gray128", "white"],
]
# Video 27: square, a 3x3 grid that keeps the four corners distinct and the frame counter bits.
GRID_SQUARE = [
    ["red", "yellow", "green"],
    ["bit2", "bit1", "bit0"],
    ["blue", "cyan", "white"],
]


def layout(kind):
    """Returns (width, height, cells), cells = [{name, x, y, w, h}] in pixels."""
    if kind == "v3":
        grid, size, extra = GRID_V3, 64, 0
    elif kind == "v3-odd":
        # 321x193: the last column and row are one pixel wider/taller (odd size for the conformer).
        grid, size, extra = GRID_V3, 64, 1
    elif kind == "v3-small":
        grid, size, extra = GRID_V3, 32, 0
    elif kind == "square":
        grid, size, extra = GRID_SQUARE, 64, 0
    else:
        raise ValueError(kind)
    rows, cols = len(grid), len(grid[0])
    cells = []
    for r in range(rows):
        for c in range(cols):
            w = size + (extra if c == cols - 1 else 0)
            h = size + (extra if r == rows - 1 else 0)
            cells.append({"name": grid[r][c], "x": c * size, "y": r * size, "w": w, "h": h})
    return cols * size + extra, rows * size + extra, cells


def cell_color(name, frame):
    if name.startswith("bit"):
        bit = int(name[3:])
        return "white" if (frame % 8) >> bit & 1 else "black"
    return name


def make_frames(kind, alpha, frames, path):
    width, height, cells = layout(kind)
    with open(path, "wb") as f:
        for frame in range(frames):
            buf = bytearray(width * height * 4)
            for cell in cells:
                color = cell_color(cell["name"], frame)
                r, g, b = COLORS[color]
                a = ALPHA.get(cell["name"], 255) if alpha else 255
                row = bytes((r, g, b, a)) * cell["w"]
                for y in range(cell["y"], cell["y"] + cell["h"]):
                    start = (y * width + cell["x"]) * 4
                    buf[start:start + len(row)] = row
            f.write(buf)
    return width, height


# ---------------------------------------------------------------------------------------------
# Colour math.

MATRIX_K = {
    "bt601": (0.299, 0.114),
    "bt709": (0.2126, 0.0722),
    "bt2020": (0.2627, 0.0593),
    "smpte240m": (0.212, 0.087),
    "fcc": (0.30, 0.11),
}

D65 = (0.3127, 0.3290)
PRIMARIES = {
    "bt709": ((0.640, 0.330), (0.300, 0.600), (0.150, 0.060), D65),
    "smpte170m": ((0.630, 0.340), (0.310, 0.595), (0.155, 0.070), D65),
    "smpte240m": ((0.630, 0.340), (0.310, 0.595), (0.155, 0.070), D65),
    "bt470bg": ((0.640, 0.330), (0.290, 0.600), (0.150, 0.060), D65),
    "bt2020": ((0.708, 0.292), (0.170, 0.797), (0.131, 0.046), D65),
    "smpte432": ((0.680, 0.320), (0.265, 0.690), (0.150, 0.060), D65),
}


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(a, v):
    return [sum(a[i][k] * v[k] for k in range(3)) for i in range(3)]


def mat_inv(m):
    a, b, c = m[0]
    d, e, f = m[1]
    g, h, i = m[2]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    return [
        [(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det],
        [(f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det],
        [(d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det],
    ]


def rgb_to_xyz(primaries):
    (rx, ry), (gx, gy), (bx, by), (wx, wy) = PRIMARIES[primaries]
    cols = [[x / y, 1.0, (1 - x - y) / y] for x, y in ((rx, ry), (gx, gy), (bx, by))]
    m = [[cols[j][i] for j in range(3)] for i in range(3)]
    w = [wx / wy, 1.0, (1 - wx - wy) / wy]
    s = mat_vec(mat_inv(m), w)
    return [[m[i][j] * s[j] for j in range(3)] for i in range(3)]


def srgb_decode(v):
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def srgb_encode(v):
    return 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


def bt709_inverse_oetf(v):
    return v / 4.5 if v < 0.081 else ((v + 0.099) / 1.099) ** (1 / 0.45)


# How the platform interprets the SDR video transfer functions (bt709, smpte170m, bt2020-10,
# bt2020-12, smpte240m) when it colour-manages. Measured in Canvas 2D on macOS 27 (M4 Max) with
# the 8-bit BT.709/601/P3/2020 videos: CoreGraphics/ColorSync decode them with a pure 1.961 power
# (Apple's ITU-R 709 colour space), e.g. gray128 -> 139, gray16 -> 14, gray235 -> 237 in an sRGB
# canvas; this model fits every patch within 2. Not modelled: the BT.709 inverse OETF
# ("bt709-inverse-oetf", gray16 -> 31) and BT.1886 ("bt1886", gamma 2.4, gray128 -> 121), kept
# for reference. Change this and regenerate if the platform changes.
SDR_VIDEO_TRANSFER_MODEL = "gamma-1.961"

SDR_VIDEO_TRANSFERS = {"bt709", "smpte170m", "bt2020-10", "bt2020-12", "smpte240m"}
HDR_TRANSFERS = {"smpte2084", "arib-std-b67"}


def transfer_decode(trc, v):
    if trc == "iec61966-2-1":
        return srgb_decode(v)
    if trc in SDR_VIDEO_TRANSFERS:
        if SDR_VIDEO_TRANSFER_MODEL == "gamma-1.961":
            return v ** 1.961
        if SDR_VIDEO_TRANSFER_MODEL == "srgb":
            return srgb_decode(v)
        if SDR_VIDEO_TRANSFER_MODEL == "bt709-inverse-oetf":
            return bt709_inverse_oetf(v)
        if SDR_VIDEO_TRANSFER_MODEL == "bt1886":
            return v ** 2.4
    if trc == "gamma22":
        return v ** 2.2
    if trc == "gamma28":
        return v ** 2.8
    if trc == "linear":
        return v
    raise ValueError("no managed model for transfer " + trc)


TARGETS = {"srgb": "bt709", "displayP3": "smpte432"}


def manage(rgb8, primaries, trc, target):
    """rgb8: native 8-bit floats. Returns 8-bit floats in the target space (clipped)."""
    lin = [transfer_decode(trc, max(0.0, min(1.0, c / 255.0))) for c in rgb8]
    m = mat_mul(mat_inv(rgb_to_xyz(TARGETS[target])), rgb_to_xyz(primaries))
    out = mat_vec(m, lin)
    return [srgb_encode(max(0.0, min(1.0, c))) * 255.0 for c in out]


def round_trip(rgb8, matrix, rng, bits):
    """RGB -> Y'CbCr (quantised to `bits`) -> RGB, as 8-bit floats (clipped)."""
    kr, kb = MATRIX_K[matrix]
    kg = 1 - kr - kb
    r, g, b = (c / 255.0 for c in rgb8)
    y = kr * r + kg * g + kb * b
    cb = (b - y) / (2 * (1 - kb))
    cr = (r - y) / (2 * (1 - kr))
    scale = 1 << (bits - 8)
    maxv = (1 << bits) - 1
    if rng == "pc":
        yq = round(y * maxv)
        cbq = round((1 << (bits - 1)) + cb * maxv)
        crq = round((1 << (bits - 1)) + cr * maxv)
        cbq, crq = (max(0, min(maxv, v)) for v in (cbq, crq))
        y2 = yq / maxv
        cb2 = (cbq - (1 << (bits - 1))) / maxv
        cr2 = (crq - (1 << (bits - 1))) / maxv
    else:
        yq = round(16 * scale + 219 * scale * y)
        cbq = round(128 * scale + 224 * scale * cb)
        crq = round(128 * scale + 224 * scale * cr)
        y2 = (yq - 16 * scale) / (219 * scale)
        cb2 = (cbq - 128 * scale) / (224 * scale)
        cr2 = (crq - 128 * scale) / (224 * scale)
    r2 = y2 + 2 * (1 - kr) * cr2
    b2 = y2 + 2 * (1 - kb) * cb2
    g2 = (y2 - kr * r2 - kb * b2) / kg
    return [max(0.0, min(1.0, c)) * 255.0 for c in (r2, g2, b2)]


def r1(values):
    return [round(v, 1) for v in values]


# ---------------------------------------------------------------------------------------------
# The video set: one video per distinct decoded pixel format / colour description. Fields:
#   name, ext, row, pixfmt, matrix (swscale out_color_matrix, or "rgb"), range ("tv"/"pc"/"-"),
#   primaries, trc, colorspace ("-" = untagged), alpha, bits, chroma, encoder args,
#   layout, frames, notes. "-" tags mean really untagged.

X264 = ["-c:v", "libx264", "-preset", "veryfast", "-crf", "12", "-threads", "1"]
X265 = ["-c:v", "libx265", "-preset", "fast", "-crf", "12", "-tag:v", "hvc1",
        "-x265-params", "log-level=error:pools=1:frame-threads=1"]
X265_HDR10 = ["-c:v", "libx265", "-preset", "fast", "-crf", "12", "-tag:v", "hvc1",
              "-x265-params", "log-level=error:pools=1:frame-threads=1:hdr10=1:repeat-headers=1:"
              "master-display=G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,50):max-cll=1000,400"]
VTHEVC = ["-c:v", "hevc_videotoolbox", "-q:v", "80", "-tag:v", "hvc1"]
VP9 = ["-c:v", "libvpx-vp9", "-crf", "10", "-b:v", "0", "-deadline", "good", "-cpu-used", "4", "-threads", "1", "-row-mt", "0"]
VP8 = ["-c:v", "libvpx", "-crf", "4", "-b:v", "1M", "-threads", "1"]
AV1 = ["-c:v", "libsvtav1", "-crf", "20", "-preset", "8", "-svtav1-params", "enable-hdr=0:lp=1"]


def V(name, ext, pixfmt, matrix, rng, prim, trc, cspace, bits, chroma, args, alpha=False,
      layout="v3", frames=FRAMES, notes="", codec=None):
    return dict(name=name, ext=ext, pixfmt=pixfmt, matrix=matrix, range=rng, primaries=prim,
                transfer=trc, colorspace=cspace, bits=bits, chroma=chroma, args=args, alpha=alpha,
                layout=layout, frames=frames, notes=notes, codec=codec)


VIDEOS = [
    V("h264-420-bt601-videorange", "mp4", "yuv420p", "bt601", "tv", "smpte170m", "smpte170m", "smpte170m", 8, "420", X264),
    V("h264-420-bt709-fullrange", "mp4", "yuv420p", "bt709", "pc", "bt709", "bt709", "bt709", 8, "420", X264),
    V("h264-420-bt709-videorange", "mp4", "yuv420p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "420", X264,
      notes="the default 8-bit video"),
    V("h264-420-bt2020-videorange", "mp4", "yuv420p", "bt2020", "tv", "bt2020", "bt709", "bt2020nc", 8, "420", X264),
    V("h264-420-smpte240m-videorange", "mp4", "yuv420p", "smpte240m", "tv", "smpte240m", "smpte240m", "smpte240m", 8, "420", X264),
    V("h264-420-untagged", "mp4", "yuv420p", "bt601", "-", "-", "-", "-", 8, "420", X264,
      notes="no colour tags; decoders assume 601 video range, SMPTE-C primaries and the 709 transfer"),
    V("h264-420-p3d65-videorange", "mp4", "yuv420p", "bt709", "tv", "smpte432", "bt709", "bt709", 8, "420", X264),
    V("hevc-420-10bit-bt709-videorange", "mp4", "yuv420p10le", "bt709", "tv", "bt709", "bt709", "bt709", 10, "420", X265,
      notes="the default 10-bit video"),
    V("hevc-420-10bit-pq-videorange", "mp4", "yuv420p10le", "bt2020", "tv", "bt2020", "smpte2084", "bt2020nc", 10, "420", X265_HDR10,
      notes="HDR10 SEI (mastering display + content light level)"),
    V("hevc-420-10bit-hlg-videorange", "mp4", "yuv420p10le", "bt2020", "tv", "bt2020", "arib-std-b67", "bt2020nc", 10, "420", X265),
    V("hevc-420-10bit-bt709-fullrange", "mp4", "yuv420p10le", "bt709", "pc", "bt709", "bt709", "bt709", 10, "420", X265),
    V("h264-422-bt709-videorange", "mp4", "yuv422p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "422", X264),
    V("hevc-422-10bit-bt709-videorange", "mp4", "yuv422p10le", "bt709", "tv", "bt709", "bt709", "bt709", 10, "422", X265),
    V("hevc-444-8bit-bt709-videorange", "mp4", "yuv444p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "444", X265),
    # Odd size: the pattern with one extra pixel column and row.
    V("hevc-444-8bit-bt709-videorange-321x193", "mp4", "yuv444p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "444", X265,
      layout="v3-odd", notes="odd size: the last patch column and row are 65 pixels"),
    V("hevc-420-12bit-bt709-videorange", "mp4", "yuv420p12le", "bt709", "tv", "bt709", "bt709", "bt709", 12, "420", X265),
    V("h264-rgb", "mp4", "bgr0", "rgb", "-", "bt709", "iec61966-2-1", "rgb", 8, "444",
      ["-c:v", "libx264rgb", "-preset", "veryfast", "-crf", "12", "-threads", "1"],
      notes="RGB-coded H.264 (GBR matrix)"),
    # hevc_videotoolbox alpha needs BGRA input and no range in setparams; VT converts with the
    # tagged 709 matrix to video range.
    V("hevcvt-alpha-bt709-videorange", "mov", "bgra", "rgb", "-", "bt709", "bt709", "bt709", 8, "420",
      VTHEVC + ["-alpha_quality", "0.9"], alpha=True, codec="vt-alpha",
      notes="HEVC with alpha (VideoToolbox encoder); VT converts BGRA with the 709 matrix to video range"),
    V("prores-4444-alpha-bt709-videorange", "mov", "yuva444p10le", "bt709", "tv", "bt709", "bt709", "bt709", 10, "444",
      ["-c:v", "prores_ks", "-profile:v", "4", "-alpha_bits", "16", "-qscale:v", "8"], alpha=True),
    V("prores-422-bt709-videorange", "mov", "yuv422p10le", "bt709", "tv", "bt709", "bt709", "bt709", 10, "422",
      ["-c:v", "prores_ks", "-profile:v", "2", "-qscale:v", "8"]),
    V("mjpeg-420-bt601-fullrange", "mov", "yuvj420p", "bt601", "pc", "bt709", "iec61966-2-1", "smpte170m", 8, "420",
      ["-c:v", "mjpeg", "-q:v", "2"]),
    # Raw videos: 2 frames at 160x96 (32x32 patches) to keep the size down.
    V("raw-bgra", "mov", "bgra", "rgb", "-", "bt709", "iec61966-2-1", "-", 8, "444",
      ["-c:v", "rawvideo", "-tag:v", "BGRA"], alpha=True, layout="v3-small", frames=RAW_FRAMES,
      notes="uncompressed BGRA with alpha, 2 frames, 160x96"),
    V("raw-2vuy-bt709-videorange", "mov", "uyvy422", "bt709", "tv", "bt709", "bt709", "bt709", 8, "422",
      ["-c:v", "rawvideo", "-tag:v", "2vuy"], layout="v3-small", frames=RAW_FRAMES,
      notes="uncompressed 2vuy, 2 frames, 160x96"),
    V("av1-420-bt709-videorange", "mp4", "yuv420p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "420", AV1,
      notes="needs AV1 hardware decode on Cocoa"),
    V("vp9-p0-420-bt709-videorange", "webm", "yuv420p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "420", VP9),
    V("vp9-p2-420-10bit-pq-videorange", "webm", "yuv420p10le", "bt2020", "tv", "bt2020", "smpte2084", "bt2020nc", 10, "420",
      VP9 + ["-profile:v", "2"]),
    V("vp8-420-bt601-videorange", "webm", "yuv420p", "bt601", "tv", "smpte170m", "smpte170m", "smpte170m", 8, "420", VP8),
    V("vp9-alpha-420-bt709-videorange", "webm", "yuva420p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "420",
      VP9 + ["-auto-alt-ref", "0"], alpha=True),
    V("h264-420-bt709-videorange-192x192", "mp4", "yuv420p", "bt709", "tv", "bt709", "bt709", "bt709", 8, "420", X264,
      layout="square", notes="square: rows 0 and 2 keep the corner colours, row 1 holds the counter bits"),
]

# Derived videos: remuxed copies of an encoded video (stream copy, same bitstream).
#   rotation: ffmpeg -display_rotation (degrees counter-clockwise), hflip: -display_hflip.
DERIVED = [
    dict(name="h264-420-bt709-videorange-rot90", ext="mp4", source="h264-420-bt709-videorange", rotation=90, hflip=False),
    dict(name="h264-420-bt709-videorange-rot180", ext="mp4", source="h264-420-bt709-videorange", rotation=180, hflip=False),
    dict(name="h264-420-bt709-videorange-rot270", ext="mp4", source="h264-420-bt709-videorange", rotation=270, hflip=False),
    dict(name="h264-420-bt709-videorange-hflip", ext="mp4", source="h264-420-bt709-videorange", rotation=0, hflip=True),
    dict(name="h264-420-bt709-videorange-frag", ext="mp4", source="h264-420-bt709-videorange", fragmented=True),
    dict(name="hevc-420-10bit-bt709-videorange-frag", ext="mp4", source="hevc-420-10bit-bt709-videorange", fragmented=True),
    dict(name="hevcvt-alpha-bt709-videorange-frag", ext="mp4", source="hevcvt-alpha-bt709-videorange", fragmented=True),
]

SKIPPED = [
    # Nothing is skipped with ffmpeg 9.0.2 on macOS 27; record entries here as
    # dict(name=..., reason=...) if a tool is missing.
]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)


def encode(spec, workdir):
    out = os.path.join(HERE, spec["name"] + "." + spec["ext"])
    raw = os.path.join(workdir, "%s-%s-%d.rgba" % (spec["layout"], "a" if spec["alpha"] else "o", spec["frames"]))
    if not os.path.exists(raw):
        make_frames(spec["layout"], spec["alpha"], spec["frames"], raw)
    width, height, _ = layout(spec["layout"])
    m, rng = spec["matrix"], spec["range"]
    if m == "rgb":
        vf = "format=" + spec["pixfmt"]
    else:
        vf = "scale=out_color_matrix=%s:out_range=%s:flags=accurate_rnd+full_chroma_int+bitexact,format=%s" % (
            m, "tv" if rng == "-" else rng, spec["pixfmt"])
    params = []
    if spec["primaries"] != "-":
        params.append("color_primaries=" + spec["primaries"])
    if spec["transfer"] != "-":
        params.append("color_trc=" + spec["transfer"])
    if spec["colorspace"] != "-":
        params.append("colorspace=" + ("gbr" if spec["colorspace"] == "rgb" else spec["colorspace"]))
    if rng != "-":
        params.append("range=" + rng)
    if m != "rgb":
        if spec["colorspace"] == "-":
            params.append("colorspace=unknown")
        if rng == "-":
            params.append("range=unknown")
    if params:
        vf += ",setparams=" + ":".join(params)
    mux = ["-movflags", "+write_colr"] if spec["ext"] in ("mp4", "mov") else []
    cmd = [FFMPEG, "-hide_banner", "-loglevel", "error", "-y",
           "-f", "rawvideo", "-pix_fmt", "rgba", "-s", "%dx%d" % (width, height), "-framerate", str(FPS),
           "-i", raw, "-frames:v", str(spec["frames"]), "-vf", vf] + spec["args"] + mux + \
          ["-fflags", "+bitexact", "-flags:v", "+bitexact", "-map_metadata", "-1", out]
    run(cmd)
    if "libsvtav1" in spec["args"] and spec["ext"] in ("mp4", "mov"):
        clear_sdtp_disposable(out)
    return out


# libsvtav1 (through ffmpeg) writes sdtp entries that mark frames as not depended on (disposable) even
# though other frames reference them. AVFoundation trusts sdtp and skips those frames, so seeking to a
# later frame fails to decode it. Clear the "not depended on" marking (sample_is_depended_on = 2) in
# every sdtp box so that it reads as unknown.
SDTP_CONTAINERS = {b"moov", b"trak", b"mdia", b"minf", b"stbl", b"moof", b"traf", b"mvex", b"edts", b"dinf"}


def clear_sdtp_disposable(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())

    def walk(start, end):
        changed = 0
        offset = start
        while offset + 8 <= end:
            size, kind = struct.unpack(">I4s", data[offset:offset + 8])
            header = 8
            if size == 1:
                size = struct.unpack(">Q", data[offset + 8:offset + 16])[0]
                header = 16
            elif size == 0:
                size = end - offset
            if size < header or offset + size > end:
                break
            if kind == b"sdtp":
                # FullBox: version (1 byte) and flags (3 bytes), then one byte per sample.
                for i in range(offset + header + 4, offset + size):
                    if data[i] & 0x0c == 0x08:
                        data[i] &= ~0x0c & 0xff
                        changed += 1
            elif kind in SDTP_CONTAINERS:
                changed += walk(offset + header, offset + size)
            offset += size
        return changed

    if walk(0, len(data)):
        with open(path, "wb") as f:
            f.write(data)


def derive(spec):
    src = os.path.join(HERE, spec["source"] + "." + ("mov" if spec["source"].startswith("hevcvt") else "mp4"))
    out = os.path.join(HERE, spec["name"] + "." + spec["ext"])
    cmd = [FFMPEG, "-hide_banner", "-loglevel", "error", "-y"]
    if "rotation" in spec:
        cmd += ["-display_rotation", str(spec["rotation"])]
        if spec["hflip"]:
            cmd += ["-display_hflip"]
    cmd += ["-i", src, "-map", "0:v", "-c", "copy", "-fflags", "+bitexact", "-map_metadata", "-1"]
    if spec.get("fragmented"):
        cmd += ["-movflags", "frag_keyframe+empty_moov+default_base_moof"]
    run(cmd + [out])
    if spec["source"].startswith("av1") and spec["ext"] in ("mp4", "mov"):
        clear_sdtp_disposable(out)
    return out


# ---------------------------------------------------------------------------------------------
# Probing.

def ffprobe_json(path, *args):
    return json.loads(run([FFPROBE, "-v", "error", "-of", "json"] + list(args) + [path]).stdout)


def find_box(data, box):
    i = data.find(box.encode())
    if i < 4:
        return None
    size = struct.unpack(">I", data[i - 4:i])[0]
    return i + 4, size - 8


def hevc_codec_string(rec):
    profile_space = rec[1] >> 6
    tier = (rec[1] >> 5) & 1
    profile_idc = rec[1] & 0x1f
    compat = struct.unpack(">I", rec[2:6])[0]
    rev = int("{:032b}".format(compat)[::-1], 2)
    constraints = list(rec[6:12])
    while constraints and constraints[-1] == 0:
        constraints.pop()
    level = rec[12]
    s = "hvc1.%s%d.%X.%s%d" % ("" if not profile_space else "ABC"[profile_space - 1], profile_idc, rev, "H" if tier else "L", level)
    if constraints:
        s += "." + ".".join("%X" % c for c in constraints)
    return s


def codec_info(path, stream):
    """Returns (codec string, description offset, description size) for MSE / WebCodecs."""
    with open(path, "rb") as f:
        data = f.read()
    name = stream["codec_name"]
    if name == "h264":
        box = find_box(data, "avcC")
        off, size = box
        return "avc1.%02X%02X%02X" % (data[off + 1], data[off + 2], data[off + 3]), off, size
    if name == "hevc":
        box = find_box(data, "hvcC")
        off, size = box
        return hevc_codec_string(data[off:off + size]), off, size
    if name == "av1":
        box = find_box(data, "av1C")
        if box:
            off, size = box
            b1, b2 = data[off + 1], data[off + 2]
            profile, level = b1 >> 5, b1 & 0x1f
            tier = "H" if b2 >> 7 else "M"
            depth = 12 if (b2 >> 5) & 1 and (b2 >> 6) & 1 else 10 if (b2 >> 6) & 1 else 8
            return "av01.%d.%02d%s.%02d" % (profile, level, tier, depth), off, size
        return "av01.0.04M.08", None, None
    if name == "vp9":
        profile = int(stream.get("profile", "Profile 0").split()[-1]) if "profile" in stream else 0
        depth = 10 if "10" in stream.get("pix_fmt", "") else 8
        return "vp09.%02d.10.%02d" % (profile, depth), None, None
    if name == "vp8":
        return "vp8", None, None
    return None, None, None


def packets(path):
    """Encoded chunks [pos, size, key, timestamp us] in decode order, verified against the file."""
    j = ffprobe_json(path, "-select_streams", "v:0", "-show_packets", "-show_data_hash", "MD5")
    with open(path, "rb") as f:
        data = f.read()
    result = []
    for p in j.get("packets", []):
        pos, size = int(p["pos"]), int(p["size"])
        if hashlib.md5(data[pos:pos + size]).hexdigest() != p["data_hash"].split(":")[1]:
            return None
        result.append([pos, size, 1 if "K" in p["flags"] else 0, round(float(p["pts_time"]) * 1e6)])
    return result


def decode_frame(path, frame, autorotate):
    """Decodes one frame with ffmpeg to RGBA. Returns (w, h, bytes)."""
    cmd = [FFMPEG, "-hide_banner", "-loglevel", "error"]
    if not autorotate:
        cmd += ["-noautorotate"]
    cmd += ["-i", path, "-vf", "select=eq(n\\,%d)" % frame, "-frames:v", "1", "-pix_fmt", "rgba",
            "-c:v", "pam", "-f", "image2pipe", "-"]
    data = run(cmd).stdout
    end = data.index(b"ENDHDR\n") + 7
    header = data[:end].decode()
    w = int(re.search(r"WIDTH (\d+)", header).group(1))
    h = int(re.search(r"HEIGHT (\d+)", header).group(1))
    pixels = data[end:]
    assert len(pixels) == w * h * 4, (path, len(pixels), w, h)
    return w, h, pixels


def sample(w, data, x, y, size):
    acc = [0, 0, 0, 0]
    for yy in range(y, y + size):
        for xx in range(x, x + size):
            i = (yy * w + xx) * 4
            for k in range(4):
                acc[k] += data[i + k]
    return [v / (size * size) for v in acc]


# Transforms: mirror (horizontal) first, then rotate clockwise. Matrices act on centred
# coordinates with y down. Same table as video-formats-test-utils.js.
TRANSFORMS = {
    "none": (1, 0, 0, 1), "rotate90": (0, -1, 1, 0), "rotate180": (-1, 0, 0, -1), "rotate270": (0, 1, -1, 0),
    "mirror": (-1, 0, 0, 1), "mirror+rotate90": (0, -1, -1, 0), "mirror+rotate180": (1, 0, 0, -1),
    "mirror+rotate270": (0, 1, 1, 0),
}


def map_point(t, w, h, x, y):
    a, b, c, d = TRANSFORMS[t]
    cx, cy = x - w / 2, y - h / 2
    nx, ny = a * cx + b * cy, c * cx + d * cy
    ow, oh = (w, h) if b == 0 else (h, w)
    return nx + ow / 2, ny + oh / 2, ow, oh


def detect_transform(spec_layout, w, h, data):
    lw, lh, cells = layout(spec_layout)
    corners = [c for c in cells if c["name"] in ("red", "green", "blue", "white")]
    best = None
    for t in TRANSFORMS:
        err = 0
        for c in corners:
            x, y, ow, oh = map_point(t, lw, lh, c["x"] + c["w"] / 2, c["y"] + c["h"] / 2)
            if (ow, oh) != (w, h):
                err = 1e9
                break
            s = sample(w, data, int(x) - 4, int(y) - 4, 8)
            err += sum(abs(s[k] - COLORS[c["name"]][k]) for k in range(3))
        if best is None or err < best[0]:
            best = (err, t)
    return best[1]


def ffmpeg_version():
    out = run([FFMPEG, "-version"]).stdout.decode().splitlines()[0]
    return out.split(" Copyright")[0]


def expected_for(spec):
    """Per colour: native / srgb / displayP3 / displayP3ReadAsSrgb / alpha."""
    trc, prim = spec["transfer"], spec["primaries"]
    managed_trc, managed_prim = trc, prim
    assumed = None
    if prim == "-":
        managed_prim, managed_trc = "smpte170m", "bt709"
        assumed = {"primaries": managed_prim, "transfer": managed_trc}
    measured = managed_trc in HDR_TRANSFERS
    colors = {}
    for color, rgb in COLORS.items():
        if spec["codec"] == "vt-alpha":
            native = round_trip(rgb, "bt709", "tv", 8)
        elif spec["matrix"] == "rgb":
            native = [float(c) for c in rgb]
        else:
            native = round_trip(rgb, spec["matrix"], "tv" if spec["range"] == "-" else spec["range"], spec["bits"])
        entry = {"native": r1(native)}
        if measured:
            entry["srgb"] = entry["displayP3"] = entry["displayP3ReadAsSrgb"] = None
        else:
            srgb = manage(native, managed_prim, managed_trc, "srgb")
            p3 = manage(native, managed_prim, managed_trc, "displayP3")
            entry["srgb"] = r1(srgb)
            entry["displayP3"] = r1(p3)
            entry["displayP3ReadAsSrgb"] = r1(manage(p3, "smpte432", "iec61966-2-1", "srgb"))
        # The native values taken as sRGB and drawn into a Display P3 canvas (e.g. an ImageBitmap
        # made with colorSpaceConversion "none").
        entry["nativeInDisplayP3"] = r1(manage(native, "bt709", "iec61966-2-1", "displayP3"))
        colors[color] = entry
    return colors, ("measured" if measured else "computed"), assumed


def self_check(spec, colors):
    """Native vs colour-managed sRGB for SDR BT.601/709 videos: where an un-managed sink (WebGL) and a
    colour-managed one (Canvas 2D) are expected to differ by design."""
    if spec["matrix"] not in ("bt601", "bt709") and spec["codec"] != "vt-alpha":
        return None
    if spec["transfer"] in HDR_TRANSFERS:
        return None
    diffs = {}
    for color, e in colors.items():
        if e["srgb"] is None:
            continue
        d = max(abs(a - b) for a, b in zip(e["native"], e["srgb"]))
        if d >= 0.5:
            diffs[color] = round(d, 1)
    return {"maxDifference": max(diffs.values()) if diffs else 0, "differingColors": diffs}


def describe(spec, path, workdir, derived=None):
    j = ffprobe_json(path, "-select_streams", "v:0", "-show_streams", "-show_format")
    stream = j["streams"][0]
    base = spec if derived is None else derived
    codec, desc_off, desc_size = codec_info(path, stream)
    width, height, cells = layout(spec["layout"])
    colors, managed, assumed = expected_for(spec)
    entry = {
        "file": os.path.basename(path),
        "container": {"mp4": "mp4", "mov": "quicktime", "webm": "webm"}[base["ext"]],
        "codec": stream["codec_name"],
        "codecString": codec,
        "mimeType": None,
        "encoder": spec["args"][1],
        "profile": stream.get("profile"),
        "pixelFormat": stream.get("pix_fmt"),
        "width": width,
        "height": height,
        "frames": spec["frames"],
        "fps": FPS,
        "matrix": spec["matrix"] if spec["codec"] != "vt-alpha" else "bt709",
        "range": spec["range"] if spec["codec"] != "vt-alpha" else "tv",
        "primaries": spec["primaries"],
        "transfer": spec["transfer"],
        "bitDepth": spec["bits"],
        "chroma": spec["chroma"],
        "alpha": spec["alpha"],
        "rotation": 0,
        "hflip": False,
        "transform": "none",
        "fragmented": bool(derived and derived.get("fragmented")),
        # Presentation time of frame 0. Non-zero for the fragmented copies of B-frame streams: the
        # fragments keep the encoder's 2-frame composition delay and have no edit list.
        "startTime": round(float((stream.get("start_time") or "0").rstrip(",")), 6),
        "layout": spec["layout"],
        "managed": managed,
        "notes": spec["notes"],
        "bytes": os.path.getsize(path),
        "streamTags": {k: stream.get(k) for k in ("color_range", "color_space", "color_transfer", "color_primaries")},
    }
    if assumed:
        entry["managedAssumes"] = assumed
    if codec:
        entry["mimeType"] = "video/%s; codecs=\"%s\"" % ("webm" if base["ext"] == "webm" else "mp4", codec)
    if derived and "rotation" in derived:
        entry["rotation"] = derived["rotation"]
        entry["hflip"] = derived["hflip"]
        entry["notes"] = "ffmpeg -display_rotation %d%s on a stream copy of %s" % (
            derived["rotation"], " -display_hflip" if derived["hflip"] else "", spec["name"])
    if derived and derived.get("fragmented"):
        entry["notes"] = "fragmented MP4 (frag_keyframe+empty_moov+default_base_moof) stream copy of " + spec["name"]
    if desc_off is not None:
        entry["decoderDescription"] = [desc_off, desc_size]
    chunks = packets(path) if stream["codec_name"] in ("h264", "hevc", "av1", "vp9", "vp8") else None
    if chunks:
        entry["decoderChunks"] = chunks
    entry["colors"] = colors
    check = self_check(spec, colors)
    if check is not None:
        entry["nativeVsManagedSrgb"] = check

    # Independent decode with ffmpeg: the displayed transform (autorotate) and the native values.
    w, h, data = decode_frame(path, 0, True)
    entry["displayWidth"], entry["displayHeight"] = w, h
    entry["transform"] = detect_transform(spec["layout"], w, h, data)
    w, h, data = decode_frame(path, min(5, spec["frames"] - 1), False)
    frame = min(5, spec["frames"] - 1)
    maxdiff = 0
    for cell in cells:
        color = cell_color(cell["name"], frame)
        s = min(16, cell["w"] // 2)
        x = cell["x"] + (cell["w"] - s) // 2
        y = cell["y"] + (cell["h"] - s) // 2
        got = sample(w, data, x, y, s)
        exp = colors[color]["native"]
        maxdiff = max(maxdiff, max(abs(got[k] - exp[k]) for k in range(3)))
    entry["ffmpegDecodeMaxDifference"] = round(maxdiff, 1)
    return entry


def main():
    only = re.compile(sys.argv[1]) if len(sys.argv) > 1 else None
    workdir = tempfile.mkdtemp(prefix="video-formats-")
    manifest_videos = {}
    by_name = {}
    try:
        for spec in VIDEOS:
            by_name[spec["name"]] = spec
            path = os.path.join(HERE, spec["name"] + "." + spec["ext"])
            if only is None or only.search(spec["name"]):
                try:
                    encode(spec, workdir)
                    print("ok   " + os.path.basename(path))
                except subprocess.CalledProcessError as e:
                    print("FAIL " + os.path.basename(path) + ": " + e.stderr.decode(errors="replace"))
                    continue
            if os.path.exists(path):
                manifest_videos[os.path.basename(path)] = describe(spec, path, workdir)
        for d in DERIVED:
            src = by_name[d["source"]]
            path = os.path.join(HERE, d["name"] + "." + d["ext"])
            if only is None or only.search(d["name"]) or only.search(d["source"]):
                derive(d)
                print("ok   " + os.path.basename(path))
            if os.path.exists(path):
                manifest_videos[os.path.basename(path)] = describe(src, path, workdir, derived=d)
    finally:
        shutil.rmtree(workdir)

    pattern = {}
    for kind in ("v3", "v3-odd", "v3-small", "square"):
        w, h, cells = layout(kind)
        pattern[kind] = {"width": w, "height": h, "cells": cells}
    manifest = {
        "generator": "generate-video-formats.py",
        "ffmpeg": ffmpeg_version(),
        "patternVersion": 3,
        "fps": FPS,
        "sdrVideoTransferModel": SDR_VIDEO_TRANSFER_MODEL,
        "colors": {k: list(v) for k, v in COLORS.items()},
        "alpha": ALPHA,
        "layouts": pattern,
        "skipped": SKIPPED,
        "videos": manifest_videos,
    }
    text = json.dumps(manifest, indent=1, sort_keys=False)
    # Keep the long numeric arrays on one line.
    text = re.sub(r"\[\s+([-0-9.,\s]+?)\s+\]", lambda m: "[" + re.sub(r"\s+", " ", m.group(1)) + "]", text)
    with open(os.path.join(HERE, "video-formats-manifest.js"), "w") as f:
        f.write("// Generated by generate-video-formats.py. Do not edit.\n")
        f.write("var videoFormatsManifest = " + text + ";\n")
    print("wrote video-formats-manifest.js (%d videos)" % len(manifest_videos))


if __name__ == "__main__":
    main()
