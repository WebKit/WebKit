/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.ShaderProgram = class ShaderProgram
{
    constructor(identifier, canvas)
    {
        console.assert(identifier);
        console.assert(canvas instanceof WI.Canvas);

        this._identifier = identifier;
        this._canvas = canvas;
        this._uniqueDisplayNumber = canvas.nextShaderProgramDisplayNumber();
        this._disabled = false;
    }

    // Public

    get identifier() { return this._identifier; }
    get canvas() { return this._canvas; }
    get disabled() { return this._disabled; }

    get displayName()
    {
        return WI.UIString("Program %d").format(this._uniqueDisplayNumber);
    }

    requestVertexShaderSource(callback)
    {
        this._requestShaderSource(CanvasAgent.ShaderType.Vertex, callback);
    }

    requestFragmentShaderSource(callback)
    {
        this._requestShaderSource(CanvasAgent.ShaderType.Fragment, callback);
    }

    updateVertexShader(source)
    {
        this._updateShader(CanvasAgent.ShaderType.Vertex, source);
    }

    updateFragmentShader(source)
    {
        this._updateShader(CanvasAgent.ShaderType.Fragment, source);
    }

    toggleDisabled(callback)
    {
        CanvasAgent.setShaderProgramDisabled(this._identifier, !this._disabled, (error) => {
            console.assert(!error, error);
            if (error)
                return;

            this._disabled = !this._disabled;
            callback();
        });
    }

    showHighlight()
    {
        const highlighted = true;
        CanvasAgent.setShaderProgramHighlighted(this._identifier, highlighted, (error) => {
            console.assert(!error, error);
        });
    }

    hideHighlight()
    {
        const highlighted = false;
        CanvasAgent.setShaderProgramHighlighted(this._identifier, highlighted, (error) => {
            console.assert(!error, error);
        });
    }

    // Private

    _requestShaderSource(shaderType, callback)
    {
        CanvasAgent.requestShaderSource(this._identifier, shaderType, (error, content) => {
            if (error) {
                callback(null);
                return;
            }

            callback(content);
        });
    }

    _updateShader(shaderType, source)
    {
        CanvasAgent.updateShader(this._identifier, shaderType, source, (error) => {
            console.assert(!error, error);
        });
    }
};

WI.ShaderProgram.ShaderType = {
    Fragment: "shader-type-fragment",
    Vertex: "shader-type-vertex",
};
