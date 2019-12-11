%
%  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
%
%  Use of this source code is governed by a BSD-style license
%  that can be found in the LICENSE file in the root of the source
%  tree. An additional intellectual property rights grant can be found
%  in the file PATENTS.  All contributing project authors may
%  be found in the AUTHORS file in the root of the source tree.
%

function [d, t] = readDetection(file, fs, chunkSize)
%[d, t] = readDetection(file, fs, chunkSize)
%
%Reads a detection signal from a DAT file.
%
%d: The detection signal.
%t: The respective time vector.
%
%file: The DAT file where the detection signal is stored in float format.
%fs: The signal sample rate in Hertz.
%chunkSize: The chunk size used for the detection in seconds.
fid = fopen(file);
d = fread(fid, inf, 'float');
fclose(fid);
t = 0:(1 / fs):(length(d) * chunkSize - 1 / fs);
d = d(floor(t / chunkSize) + 1);
