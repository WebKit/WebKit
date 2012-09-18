/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebRenderingStats_h
#define WebRenderingStats_h

namespace WebKit {

struct WebRenderingStats {
    int numAnimationFrames;
    int numFramesSentToScreen;
    int droppedFrameCount;
    double totalPaintTimeInSeconds;
    double totalRasterizeTimeInSeconds;
    double totalCommitTimeInSeconds;
    size_t totalCommitCount;

    WebRenderingStats()
        : numAnimationFrames(0)
        , numFramesSentToScreen(0)
        , droppedFrameCount(0)
        , totalPaintTimeInSeconds(0)
        , totalRasterizeTimeInSeconds(0)
        , totalCommitTimeInSeconds(0)
        , totalCommitCount(0)
    {
    }

    // In conjunction with enumerateFields, this allows the embedder to
    // enumerate the values in this structure without
    // having to embed references to its specific member variables. This
    // simplifies the addition of new fields to this type.
    class Enumerator {
    public:
        virtual void addInt(const char* name, int value) = 0;
        virtual void addDouble(const char* name, double value) = 0;
    protected:
        virtual ~Enumerator() { }
    };

    // Outputs the fields in this structure to the provided enumerator.
    void enumerateFields(Enumerator* enumerator) const
    {
        enumerator->addInt("numAnimationFrames", numAnimationFrames);
        enumerator->addInt("numFramesSentToScreen", numFramesSentToScreen);
        enumerator->addInt("droppedFrameCount", droppedFrameCount);
        enumerator->addDouble("totalPaintTimeInSeconds", totalPaintTimeInSeconds);
        enumerator->addDouble("totalRasterizeTimeInSeconds", totalRasterizeTimeInSeconds);
        enumerator->addDouble("totalCommitTimeInSeconds", totalCommitTimeInSeconds);
        enumerator->addInt("totalCommitCount", totalCommitCount);
    }
};

} // namespace WebKit

#endif // WebRenderingStats_h
