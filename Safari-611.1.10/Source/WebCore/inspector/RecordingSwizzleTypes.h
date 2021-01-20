/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

namespace WebCore {

// Keep this in sync with WI.Recording.Swizzle.
enum class RecordingSwizzleTypes : int {
    None = 0,
    Number = 1,
    Boolean = 2,
    String = 3,
    Array = 4,
    TypedArray = 5,
    Image = 6,
    ImageData = 7,
    DOMMatrix = 8,
    Path2D = 9,
    CanvasGradient = 10,
    CanvasPattern = 11,
    WebGLBuffer = 12,
    WebGLFramebuffer = 13,
    WebGLRenderbuffer = 14,
    WebGLTexture = 15,
    WebGLShader = 16,
    WebGLProgram = 17,
    WebGLUniformLocation = 18,
    ImageBitmap = 19,
    WebGLQuery = 20,
    WebGLSampler = 21,
    WebGLSync = 22,
    WebGLTransformFeedback = 23,
    WebGLVertexArrayObject = 24,
};

} // namespace WebCore
