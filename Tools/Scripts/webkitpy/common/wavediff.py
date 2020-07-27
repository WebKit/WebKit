# Copyright (C) 2015 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import struct
import wave

from webkitcorepy import BytesIO, StringIO


class WaveDiff(object):
    _paramNames = ('Number of channels', 'Sample width', 'Sample rate', 'Number of frames', 'Compression type', 'Compression name')

    # Audio effect processing is intrinsically imprecise, so we need to always allow tolerance.
    _tolerance = 1

    def __init__(self, in1, in2):
        if isinstance(in1, str):
            waveFile1 = wave.open(StringIO(in1), 'r')
        else:
            waveFile1 = wave.open(BytesIO(in1), 'rb')
        if isinstance(in2, str):
            waveFile2 = wave.open(StringIO(in2), 'r')
        else:
            waveFile2 = wave.open(BytesIO(in2), 'rb')

        params1 = waveFile1.getparams()
        params2 = waveFile2.getparams()

        self._diff = ''

        self._filesAreIdentical = not sum(map(self._diffParam, params1, params2, self._paramNames))
        self._filesAreIdenticalWithinTolerance = self._filesAreIdentical

        if not self._filesAreIdentical:
            return

        # Metadata is identical, compare the content now.

        channelCount1 = waveFile1.getnchannels()
        frameCount1 = waveFile1.getnframes()
        sampleWidth1 = waveFile1.getsampwidth()
        channelCount2 = waveFile2.getnchannels()
        frameCount2 = waveFile2.getnframes()
        sampleWidth2 = waveFile2.getsampwidth()

        allData1 = self._readSamples(waveFile1, sampleWidth1, frameCount1 * channelCount1)
        allData2 = self._readSamples(waveFile2, sampleWidth2, frameCount2 * channelCount2)
        results = list(map(self._diffSample, allData1, allData2, range(max(frameCount1 * channelCount1, frameCount2 * channelCount2))))

        cumulativeSampleDiff = sum(results)
        differingSampleCount = len(list(filter(bool, results)))
        self._filesAreIdentical = not differingSampleCount
        self._filesAreIdenticalWithinTolerance = not len(list(filter(lambda x: x > self._tolerance, results)))

        if differingSampleCount:
            self._diff += '\n'
            self._diff += 'Total differing samples: %d\n' % differingSampleCount
            self._diff += 'Percentage of differing samples: %0.3f%%\n' % (100 * float(differingSampleCount) / max(frameCount1, frameCount2))
            self._diff += 'Cumulative sample difference: %d\n' % cumulativeSampleDiff
            self._diff += 'Average sample difference: %f\n' % (float(cumulativeSampleDiff) / differingSampleCount)

    def _diffParam(self, param1, param2, paramName):
        if param1 == param2:
            return False
        self._diff += paramName + '\n'
        self._diff += '< %s\n' % str(param1)
        self._diff += '---\n'
        self._diff += '> %s\n' % str(param2)
        return True

    @staticmethod
    def _readSamples(file, sampleWidth, nSamples):
        allFrames = file.readframes(nSamples)
        unpackFormat = 'b' if sampleWidth == 1 else 'h'
        return struct.unpack('<%d%s' % (nSamples, unpackFormat), allFrames)

    def _diffSample(self, data1, data2, i):
        if (data1 != data2):
            self._diff += 'Sample #%d\n' % i
            self._diff += '< %d\n' % data1
            self._diff += '---\n'
            self._diff += '> %d\n' % data2
        return abs(data1 - data2)

    def filesAreIdentical(self):
        return self._filesAreIdentical

    def filesAreIdenticalWithinTolerance(self):
        return self._filesAreIdenticalWithinTolerance

    def diffText(self):
        return self._diff
