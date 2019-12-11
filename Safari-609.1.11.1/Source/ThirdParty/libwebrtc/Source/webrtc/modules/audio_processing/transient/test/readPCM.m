%
%  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
%
%  Use of this source code is governed by a BSD-style license
%  that can be found in the LICENSE file in the root of the source
%  tree. An additional intellectual property rights grant can be found
%  in the file PATENTS.  All contributing project authors may
%  be found in the AUTHORS file in the root of the source tree.
%

function [x, t] = readPCM(file, fs)
%[x, t] = readPCM(file, fs)
%
%Reads a signal from a PCM file.
%
%x: The read signal after normalization.
%t: The respective time vector.
%
%file: The PCM file where the signal is stored in int16 format.
%fs: The signal sample rate in Hertz.
fid = fopen(file);
x = fread(fid, inf, 'int16');
fclose(fid);
x = x - mean(x);
x = x / max(abs(x));
t = 0:(1 / fs):((length(x) - 1) / fs);
