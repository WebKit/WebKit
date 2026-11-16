#!/bin/bash
#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#
# Author: jimbankoski@google.com (Jim Bankoski)

if [[ $# -ne 2 ]]; then
  echo "Encodes a file using best known settings (slow!)"
  echo "  Usage:    be [FILE] [BITRATE]"
  echo "  Example:  be akiyo_cif.y4m 200"
  exit
fi

f=$1  # file is first parameter
b=$2  # bitrate is second parameter

if [[ -e $f.fpf ]]; then
  # First-pass file found, do second pass only
  aomenc \
    $f \
    -o $f-$b.av1.webm \
    -p 2 \
    --pass=2 \
    --fpf=$f.fpf \
    --good \
    --cpu-used=0 \
    --target-bitrate=$b \
    --auto-alt-ref=1 \
    -v \
    --minsection-pct=0 \
    --maxsection-pct=800 \
    --lag-in-frames=25 \
    --kf-min-dist=0 \
    --kf-max-dist=99999 \
    --static-thresh=0 \
    --min-q=0 \
    --max-q=63 \
    --drop-frame=0 \
    --bias-pct=50 \
    --minsection-pct=0 \
    --maxsection-pct=800 \
    --psnr \
    --arnr-maxframes=7 \
    --arnr-strength=3
else
  # No first-pass file found, do 2-pass encode
  aomenc \
    $f \
    -o $f-$b.av1.webm \
    -p 2 \
    --pass=1 \
    --fpf=$f.fpf \
    --good \
    --cpu-used=0 \
    --target-bitrate=$b \
    --auto-alt-ref=1 \
    -v \
    --minsection-pct=0 \
    --maxsection-pct=800 \
    --lag-in-frames=25 \
    --kf-min-dist=0 \
    --kf-max-dist=99999 \
    --static-thresh=0 \
    --min-q=0 \
    --max-q=63 \
    --drop-frame=0

  aomenc \
    $f \
    -o $f-$b.av1.webm \
    -p 2 \
    --pass=2 \
    --fpf=$f.fpf \
    --good \
    --cpu-used=0 \
    --target-bitrate=$b \
    --auto-alt-ref=1 \
    -v \
    --minsection-pct=0 \
    --maxsection-pct=800 \
    --lag-in-frames=25 \
    --kf-min-dist=0 \
    --kf-max-dist=99999 \
    --static-thresh=0 \
    --min-q=0 \
    --max-q=63 \
    --drop-frame=0 \
    --bias-pct=50 \
    --minsection-pct=0 \
    --maxsection-pct=800 \
    --psnr \
    --arnr-maxframes=7 \
    --arnr-strength=3
fi
