/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGL)

#include "WebGLRenderingContextBase.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class WebGLExtension : public RefCounted<WebGLExtension> {
    WTF_MAKE_ISO_ALLOCATED(WebGLExtension);
public:
    // Extension names are needed to properly wrap instances in JavaScript objects.
    enum ExtensionName {
        WebGLLoseContextName,
        EXTBlendMinMaxName,
        EXTFragDepthName,
        EXTShaderTextureLODName,
        EXTTextureFilterAnisotropicName,
        EXTsRGBName,
        OESTextureFloatName,
        OESTextureFloatLinearName,
        OESTextureHalfFloatName,
        OESTextureHalfFloatLinearName,
        OESStandardDerivativesName,
        OESVertexArrayObjectName,
        WebGLDebugRendererInfoName,
        WebGLDebugShadersName,
        WebGLCompressedTextureS3TCName,
        WebGLCompressedTextureS3TCsRGBName,
        WebGLDepthTextureName,
        WebGLDrawBuffersName,
        OESElementIndexUintName,
        WebGLCompressedTextureATCName,
        WebGLCompressedTextureETCName,
        WebGLCompressedTextureETC1Name,
        WebGLCompressedTexturePVRTCName,
        WebGLCompressedTextureASTCName,
        ANGLEInstancedArraysName,
        EXTColorBufferHalfFloatName,
        WebGLColorBufferFloatName,
        EXTColorBufferFloatName,
    };

    WebGLRenderingContextBase* context() { return m_context; }

    virtual ~WebGLExtension();
    virtual ExtensionName getName() const = 0;

    // Lose the parent WebGL context. The context loss mode changes
    // the behavior specifically of WEBGL_lose_context, which does not
    // lose its connection to its parent context when it forces a
    // context loss. However, all extensions must be lost when
    // destroying their WebGLRenderingContextBase.
    virtual void loseParentContext(WebGLRenderingContextBase::LostContextMode);
    bool isLost() { return !m_context; }

protected:
    WebGLExtension(WebGLRenderingContextBase&);
    WebGLRenderingContextBase* m_context;
};

} // namespace WebCore

#endif
