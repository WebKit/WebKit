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

WebInspector.ShaderProgram = class ShaderProgram extends WebInspector.Object
{
    constructor(id, canvas)
    {
        super();

        this._id = id;
        this._canvas = canvas || null;
    }

    // Public

    get id()
    {
        return this._id;
    }

    get canvas()
    {
        return this._canvas;
    }

    get displayName()
    {
        return WebInspector.UIString("Program %d").format(this._id.objectId);
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.ShaderProgram.FrameURLCookieKey] = this.canvas.parentFrame.url.hash;
        cookie[WebInspector.ShaderProgram.CanvasNameKey] = this.canvas.name.hash;
        cookie[WebInspector.ShaderProgram.IDCookieKey] = this._id;
    }

    updateCanvas(canvas)
    {
        // Called by canvas.

        console.assert(!this._canvas);
        this._canvas = canvas;
    }
};

WebInspector.ShaderProgram.FrameURLCookieKey = "shader-program-url";
WebInspector.ShaderProgram.CanvasNameKey = "shader-program-canvas-name";
WebInspector.ShaderProgram.IDCookieKey = "shader-program-id";
