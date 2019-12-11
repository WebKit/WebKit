function sequence = maxUnwrap(sequence, max)
%
% sequence = maxUnwrap(sequence, max)
% Unwraps when a wrap around is detected.
%
% Arguments
%
% sequence: The vector to unwrap.
% max: The maximum value that the sequence can take,
%      and after which it will wrap over to 0.
%
% Return value
%
% sequence: The unwrapped vector.
%

% Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
%
% Use of this source code is governed by a BSD-style license
% that can be found in the LICENSE file in the root of the source
% tree. An additional intellectual property rights grant can be found
% in the file PATENTS.  All contributing project authors may
% be found in the AUTHORS file in the root of the source tree.

sequence = round((unwrap(2 * pi * sequence / max) * max) / (2 * pi));
