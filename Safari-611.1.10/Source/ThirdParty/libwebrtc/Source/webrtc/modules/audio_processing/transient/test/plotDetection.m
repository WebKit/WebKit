%
%  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
%
%  Use of this source code is governed by a BSD-style license
%  that can be found in the LICENSE file in the root of the source
%  tree. An additional intellectual property rights grant can be found
%  in the file PATENTS.  All contributing project authors may
%  be found in the AUTHORS file in the root of the source tree.
%

function [] = plotDetection(PCMfile, DATfile, fs, chunkSize)
%[] = plotDetection(PCMfile, DATfile, fs, chunkSize)
%
%Plots the signal alongside the detection values.
%
%PCMfile: The file of the input signal in PCM format.
%DATfile: The file containing the detection values in binary float format.
%fs: The sample rate of the signal in Hertz.
%chunkSize: The chunk size used to compute the detection values in seconds.
[x, tx] = readPCM(PCMfile, fs);
[d, td] = readDetection(DATfile, fs, chunkSize);
plot(tx, x, td, d);
