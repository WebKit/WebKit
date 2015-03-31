/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.WebGLContextAttributes = class WebGLContextAttributes extends WebInspector.Object
{
    constructor(alpha, depth, stencil, antialias, premultipliedAlpha, preserveDrawingBuffer)
    {
        super();

        this._alpha = alpha;
        this._depth = depth;
        this._stencil = stencil;
        this._antialias = antialias;
        this._premultipliedAlpha = premultipliedAlpha;
        this._preserveDrawingBuffer = preserveDrawingBuffer;
    }

    // Static

    static fromPayload(payload)
    {
        console.assert(payload);
        return new WebInspector.WebGLContextAttributes(payload.alpha, payload.depth, payload.stencil, payload.antialias, payload.premultipliedAlpha, payload.preserveDrawingBuffer);
    }

    // Public

    get alpha()
    {
        return this._alpha;
    }

    get depth()
    {
        return this._depth;
    }

    get stencil()
    {
        return this._stencil;
    }

    get antialias()
    {
        return this._antialias;
    }

    get premultipliedAlpha()
    {
        return this._premultipliedAlpha;
    }

    get preserveDrawingBuffer()
    {
        return this._preserveDrawingBuffer;
    }
};
