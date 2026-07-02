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

if [[ $# -ne 4 ]]; then
  echo Encodes all the y4m files in the directory at the bitrates specified by
  echo the first 3 parameters and stores the results in a subdirectory named by
  echo the 4th parameter:
  echo
  echo Usage:    run_encodes.sh start-kbps end-kbps step-kbps output-directory
  echo Example:  run_encodes.sh 200 500 50 baseline
  exit
fi

s=$1
e=$2
step=$3
newdir=$4

for i in ./*y4m; do
  for (( b=$s; b<= $e; b+= $step ))
  do
    best_encode.sh $i $b
  done
  mv opsnr.stt $i.stt
done

mkdir $newdir
mv *.stt $newdir
mv *.webm $newdir
