/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)

namespace WebCore {

enum class GraphicsContextGLSimulatedEventForTesting : uint8_t {
    GPUStatusFailure,
    Timeout,
    DisplayBufferAllocationFailure,
    RenderbufferAllocationFailure
};

// The outcome of GraphicsContextGL::copyTextureFromVideoFrame(). Recorded for tests.
enum class GraphicsContextGLVideoUploadOutcome : uint8_t {
    // The copy happened.
    Direct, // The frame's pixel format was used as is.
    Converted, // The frame was converted to a pixel format the copy supports.
    Cached, // The texture already holds the frame.
    // The copy did not happen.
    NotTransferable, // The frame could not be sent to the GPU process.
    ContextLost,
    NoFrame, // The GPU process could not read the frame.
    Unsupported,
    ConversionFailed,
    NoIOSurface,
    CopyFailed, // The copy raised a GL error, e.g. the destination format is not supported.
    PbufferFailed,
};

constexpr bool isSuccessfulVideoUpload(GraphicsContextGLVideoUploadOutcome outcome)
{
    return outcome == GraphicsContextGLVideoUploadOutcome::Direct || outcome == GraphicsContextGLVideoUploadOutcome::Converted || outcome == GraphicsContextGLVideoUploadOutcome::Cached;
}

// Test-only record of the last copyTextureFromVideoFrame() call of a context. `sequence` counts the calls.
struct GraphicsContextGLVideoUploadRecord {
    uint64_t sequence { 0 };
    GraphicsContextGLVideoUploadOutcome outcome { GraphicsContextGLVideoUploadOutcome::Unsupported };
    uint32_t pixelFormat { 0 }; // The frame's FourCC, before any conversion.
};

} // namespace WebCore

#endif
