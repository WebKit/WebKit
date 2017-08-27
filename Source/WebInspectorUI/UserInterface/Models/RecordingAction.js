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

WI.RecordingAction = class RecordingAction
{
    constructor(name, parameters, trace, snapshot)
    {
        this._payloadName = name;
        this._payloadParameters = parameters;
        this._payloadTrace = trace;
        this._payloadSnapshot = snapshot || -1;

        this._name = "";
        this._parameters = [];
        this._trace = [];
        this._snapshot = "";

        this._valid = true;

        this._isFunction = false;
        this._isGetter = false;
        this._isVisual = false;
        this._stateModifiers = new Set;
    }

    // Static

    // Payload format: [name, parameters, trace, [snapshot]]
    static fromPayload(payload)
    {
        if (!Array.isArray(payload))
            payload = [];

        if (isNaN(payload[0]))
            payload[0] = -1;

        if (!Array.isArray(payload[1]))
            payload[1] = [];

        if (!Array.isArray(payload[2]))
            payload[2] = [];

        if (payload.length >= 4 && isNaN(payload[3]))
            payload[3] = -1;

        return new WI.RecordingAction(...payload);
    }

    static isFunctionForType(type, name)
    {
        let functionNames = WI.RecordingAction._functionNames[type];
        if (!functionNames)
            return false;

        return functionNames.has(name);
    }

    // Public

    get name() { return this._name; }
    get parameters() { return this._parameters; }
    get trace() { return this._trace; }
    get snapshot() { return this._snapshot; }

    get valid() { return this._valid; }
    set valid(valid) { this._valid = !!valid; }

    get isFunction() { return this._isFunction; }
    get isGetter() { return this._isGetter; }
    get isVisual() { return this._isVisual; }
    get stateModifiers() { return this._stateModifiers; }

    swizzle(recording)
    {
        this._name = recording.swizzle(this._payloadName, WI.Recording.Swizzle.String);

        this._parameters = this._payloadParameters.map((item, i) => {
            let type = this.parameterSwizzleTypeForTypeAtIndex(recording.type, i);
            if (!type)
                return item;

            let swizzled = recording.swizzle(item, type);
            if (swizzled === WI.Recording.Swizzle.Invalid)
                this._valid = false;

            return swizzled;
        });

        for (let item of this._payloadTrace) {
            try {
                let array = recording.swizzle(item, WI.Recording.Swizzle.Array);
                let callFrame = WI.CallFrame.fromPayload(WI.mainTarget, {
                    functionName: recording.swizzle(array[0], WI.Recording.Swizzle.String),
                    url: recording.swizzle(array[1], WI.Recording.Swizzle.String),
                    lineNumber: array[2],
                    columnNumber: array[3],
                });
                this._trace.push(callFrame);
            } catch { }
        }

        if (this._payloadSnapshot >= 0)
            this._snapshot = recording.swizzle(this._payloadSnapshot, WI.Recording.Swizzle.String);

        this._isFunction = WI.RecordingAction.isFunctionForType(recording.type, this._name);
        this._isGetter = !this._isFunction && !this._parameters.length;

        let visualNames = WI.RecordingAction._visualNames[recording.type];
        this._isVisual = visualNames ? visualNames.has(this._name) : false;

        this._stateModifiers = new Set([this._name]);
        let stateModifiers = WI.RecordingAction._stateModifiers[recording.type];
        if (stateModifiers) {
            let modifiedByAction = stateModifiers[this._name] || [];
            for (let item of modifiedByAction)
                this._stateModifiers.add(item);
        }
    }

    parameterSwizzleTypeForTypeAtIndex(type, index)
    {
        let functionNames = WI.RecordingAction._parameterSwizzleTypeForTypeAtIndex[type];
        if (!functionNames)
            return null;

        let parameterLengths = functionNames[this._name];
        if (!parameterLengths)
            return null;

        let argumentSwizzleTypes = parameterLengths[this._payloadParameters.length];
        if (!argumentSwizzleTypes)
            return null;

        return argumentSwizzleTypes[index] || null;
    }

    toJSON()
    {
        let json = [this._payloadName, this._payloadParameters, this._payloadTrace];
        if (this._payloadSnapshot >= 0)
            json.push(this._payloadSnapshot);
        return json;
    }
};

// This object instructs the frontend as to how to reconstruct deduplicated objects found in the
// "data" section of a recording payload. It will only swizzle values if they are of the expected
// type in the right index of the version of the action (determined by the number of parameters) for
// the recording type. Since a recording can be created by importing a JSON file, this is used to
// make sure that inputs are only considered valid if they conform to the structure defined below.
// 
// For Example:
//
// IDL:
//
//     void foo(optional DOMString s = "bar");
//     void foo(DOMPath path, optional DOMString s = "bar");
//     void foo(float a, float b, float c, float d);
//
// Swizzle Entries:
//
//     - For the 1 parameter version, the parameter at index 0 needs to be swizzled as a string
//     - For the 2 parameter version, the parameters need to be swizzled as a Path and String
//     - For the 4 parameter version, numbers don't need to be swizzled, so it is not included
//
//     "foo": {
//         1: {0: String}
//         2: {0: Path2D, 1: String}
//     }

{
    let {
        ArrayBufferView,
        BufferDataSource,
        CanvasStyle,
        Element,
        Float32List,
        Image,
        ImageData,
        Int32List,
        Path2D,
        String,
        TexImageSource,
        WebGLBuffer,
        WebGLFramebuffer,
        WebGLProgram,
        WebGLRenderbuffer,
        WebGLShader,
        WebGLTexture,
        WebGLUniformLocation,
    } = WI.Recording.Swizzle;

    WI.RecordingAction._parameterSwizzleTypeForTypeAtIndex = {
        [WI.Recording.Type.Canvas2D]: {
            "clip": {
                1: {0: String},
                2: {0: Path2D, 1: String},
            },
            "createImageData": {
                1: {0: ImageData},
            },
            "createPattern": {
                2: {0: Image, 1: String},
            },
            "direction": {
                1: {0: String},
            },
            "drawImage": {
                3: {0: Image},
                5: {0: Image},
                9: {0: Image},
            },
            "drawImageFromRect": {
                10: {0: Image, 9: String},
            },
            "drawFocusIfNeeded": {
                1: {0: Element},
                2: {0: Path2D, 1: Element},
            },
            "fill": {
                1: {0: String},
                2: {0: Path2D, 1: String},
            },
            "fillStyle": {
                1: {0: CanvasStyle},
            },
            "fillText": {
                3: {0: String},
                4: {0: String},
            },
            "font": {
                1: {0: String},
            },
            "globalCompositeOperation": {
                1: {0: String},
            },
            "imageSmoothingQuality": {
                1: {0: String},
            },
            "isPointInPath": {
                4: {0: Path2D, 3: String},
            },
            "isPointInStroke": {
                3: {0: Path2D, 3: String},
            },
            "lineCap": {
                1: {0: String},
            },
            "lineJoin": {
                1: {0: String},
            },
            "measureText": {
                1: {0: String},
            },
            "putImageData": {
                3: {0: ImageData},
                7: {0: ImageData},
            },
            "setCompositeOperation": {
                1: {0: String},
            },
            "setFillColor": {
                1: {0: String},
                2: {0: String},
            },
            "setLineCap": {
                1: {0: String},
            },
            "setLineJoin": {
                1: {0: String},
            },
            "setShadow": {
                4: {3: String},
                5: {3: String},
            },
            "setStrokeColor": {
                1: {0: String},
                2: {0: String},
            },
            "shadowColor": {
                1: {0: String},
            },
            "stroke": {
                1: {0: Path2D},
            },
            "strokeStyle": {
                1: {0: CanvasStyle},
            },
            "strokeText": {
                3: {0: String},
                4: {0: String},
            },
            "textAlign": {
                1: {0: String},
            },
            "textBaseline": {
                1: {0: String},
            },
            "webkitPutImageData": {
                3: {0: ImageData},
                7: {0: ImageData},
            },
        },
        [WI.Recording.Type.CanvasWebGL]: {
            "attachShader": {
                1: {0: WebGLProgram},
                2: {0: WebGLProgram, 1: WebGLShader},
            },
            "bindAttribLocation": {
                1: {0: WebGLProgram},
                2: {0: WebGLProgram},
                3: {0: WebGLProgram, 2: String},
            },
            "bindBuffer": {
                2: {1: WebGLBuffer},
            },
            "bindFramebuffer": {
                2: {1: WebGLFramebuffer},
            },
            "bindRenderbuffer": {
                2: {1: WebGLRenderbuffer},
            },
            "bindTexture": {
                2: {1: WebGLTexture},
            },
            "bufferData": {
                3: {1: BufferDataSource},
            },
            "bufferSubData": {
                3: {2: BufferDataSource},
            },
            "compileShader": {
                1: {0: WebGLShader},
            },
            "compressedTexImage2D": {
                7: {6: ArrayBufferView},
            },
            "compressedTexSubImage2D": {
                8: {7: ArrayBufferView},
            },
            "deleteBuffer": {
                1: {0: WebGLBuffer},
            },
            "deleteFramebuffer": {
                1: {0: WebGLFramebuffer},
            },
            "deleteProgram": {
                1: {0: WebGLProgram},
            },
            "deleteRenderbuffer": {
                1: {0: WebGLRenderbuffer},
            },
            "deleteShader": {
                1: {0: WebGLShader},
            },
            "deleteTexture": {
                1: {0: WebGLTexture},
            },
            "detachShader": {
                1: {0: WebGLProgram},
                2: {0: WebGLProgram, 1: WebGLShader},
            },
            "framebufferRenderbuffer": {
                4: {3: WebGLRenderbuffer},
            },
            "framebufferTexture2D": {
                5: {4: WebGLTexture},
            },
            "getActiveAttrib": {
                2: {0: WebGLProgram},
            },
            "getActiveUniform": {
                2: {0: WebGLProgram},
            },
            "getAttachedShaders": {
                1: {0: WebGLProgram},
            },
            "getAttribLocation": {
                2: {0: WebGLProgram, 1: String},
            },
            "getExtension": {
                1: {0: String},
            },
            "getProgramInfoLog": {
                1: {0: WebGLProgram},
            },
            "getProgramParameter": {
                2: {0: WebGLProgram},
            },
            "getShaderInfoLog": {
                1: {0: WebGLShader},
            },
            "getShaderParameter": {
                2: {0: WebGLShader},
            },
            "getShaderSource": {
                1: {0: WebGLShader},
            },
            "getUniform": {
                1: {0: WebGLProgram},
                2: {0: WebGLProgram, 1: WebGLUniformLocation},
            },
            "getUniformLocation": {
                2: {0: WebGLProgram, 1: String},
            },
            "isBuffer": {
                1: {0: WebGLBuffer},
            },
            "isFramebuffer": {
                1: {0: WebGLFramebuffer},
            },
            "isProgram": {
                1: {0: WebGLProgram},
            },
            "isRenderbuffer": {
                1: {0: WebGLRenderbuffer},
            },
            "isShader": {
                1: {0: WebGLShader},
            },
            "isTexture": {
                1: {0: WebGLTexture},
            },
            "linkProgram": {
                1: {0: WebGLProgram},
            },
            "readPixels": {
                7: {6: ArrayBufferView},
            },
            "shaderSource": {
                2: {0: WebGLShader, 1: String},
            },
            "texImage2D": {
                6: {5: TexImageSource},
                9: {8: ArrayBufferView},
            },
            "texSubImage2D": {
                7: {6: TexImageSource},
                9: {8: ArrayBufferView},
            },
            "uniform1f": {
                2: {0: WebGLUniformLocation},
            },
            "uniform1fv": {
                2: {0: WebGLUniformLocation, 1: Float32List},
            },
            "uniform1i": {
                2: {0: WebGLUniformLocation},
            },
            "uniform1iv": {
                2: {0: WebGLUniformLocation, 1: Int32List},
            },
            "uniform2f": {
                3: {0: WebGLUniformLocation},
            },
            "uniform2fv": {
                2: {0: WebGLUniformLocation, 1: Float32List},
            },
            "uniform2i": {
                3: {0: WebGLUniformLocation},
            },
            "uniform2iv": {
                2: {0: WebGLUniformLocation, 1: Int32List},
            },
            "uniform3f": {
                4: {0: WebGLUniformLocation},
            },
            "uniform3fv": {
                2: {0: WebGLUniformLocation, 1: Float32List},
            },
            "uniform3i": {
                4: {0: WebGLUniformLocation},
            },
            "uniform3iv": {
                2: {0: WebGLUniformLocation, 1: Int32List},
            },
            "uniform4f": {
                5: {0: WebGLUniformLocation},
            },
            "uniform4fv": {
                2: {0: WebGLUniformLocation, 1: Float32List},
            },
            "uniform4i": {
                5: {0: WebGLUniformLocation},
            },
            "uniform4iv": {
                2: {0: WebGLUniformLocation, 1: Int32List},
            },
            "uniformMatrix2fv": {
                3: {0: WebGLUniformLocation, 2: Float32List},
            },
            "uniformMatrix3fv": {
                3: {0: WebGLUniformLocation, 2: Float32List},
            },
            "uniformMatrix4fv": {
                3: {0: WebGLUniformLocation, 2: Float32List},
            },
            "useProgram": {
                1: {0: WebGLProgram},
            },
            "validateProgram": {
                1: {0: WebGLProgram},
            },
        },
    };
}

WI.RecordingAction._functionNames = {
    [WI.Recording.Type.Canvas2D]: new Set([
        "arc",
        "arcTo",
        "beginPath",
        "bezierCurveTo",
        "clearRect",
        "clearShadow",
        "clip",
        "clip",
        "closePath",
        "commit",
        "createImageData",
        "createLinearGradient",
        "createPattern",
        "createRadialGradient",
        "drawFocusIfNeeded",
        "drawImage",
        "drawImageFromRect",
        "ellipse",
        "fill",
        "fill",
        "fillRect",
        "fillText",
        "getImageData",
        "getLineDash",
        "isPointInPath",
        "isPointInPath",
        "isPointInStroke",
        "isPointInStroke",
        "lineTo",
        "measureText",
        "moveTo",
        "putImageData",
        "quadraticCurveTo",
        "rect",
        "resetTransform",
        "restore",
        "rotate",
        "save",
        "scale",
        "setAlpha",
        "setCompositeOperation",
        "setFillColor",
        "setLineCap",
        "setLineDash",
        "setLineJoin",
        "setLineWidth",
        "setMiterLimit",
        "setShadow",
        "setStrokeColor",
        "setTransform",
        "stroke",
        "stroke",
        "strokeRect",
        "strokeText",
        "transform",
        "translate",
        "webkitGetImageDataHD",
        "webkitPutImageDataHD",
    ]),
    [WI.Recording.Type.CanvasWebGL]: new Set([
        "activeTexture",
        "attachShader",
        "bindAttribLocation",
        "bindBuffer",
        "bindFramebuffer",
        "bindRenderbuffer",
        "bindTexture",
        "blendColor",
        "blendEquation",
        "blendEquationSeparate",
        "blendFunc",
        "blendFuncSeparate",
        "bufferData",
        "bufferData",
        "bufferSubData",
        "checkFramebufferStatus",
        "clear",
        "clearColor",
        "clearDepth",
        "clearStencil",
        "colorMask",
        "compileShader",
        "compressedTexImage2D",
        "compressedTexSubImage2D",
        "copyTexImage2D",
        "copyTexSubImage2D",
        "createBuffer",
        "createFramebuffer",
        "createProgram",
        "createRenderbuffer",
        "createShader",
        "createTexture",
        "cullFace",
        "deleteBuffer",
        "deleteFramebuffer",
        "deleteProgram",
        "deleteRenderbuffer",
        "deleteShader",
        "deleteTexture",
        "depthFunc",
        "depthMask",
        "depthRange",
        "detachShader",
        "disable",
        "disableVertexAttribArray",
        "drawArrays",
        "drawElements",
        "enable",
        "enableVertexAttribArray",
        "finish",
        "flush",
        "framebufferRenderbuffer",
        "framebufferTexture2D",
        "frontFace",
        "generateMipmap",
        "getActiveAttrib",
        "getActiveUniform",
        "getAttachedShaders",
        "getAttribLocation",
        "getBufferParameter",
        "getContextAttributes",
        "getError",
        "getExtension",
        "getFramebufferAttachmentParameter",
        "getParameter",
        "getProgramInfoLog",
        "getProgramParameter",
        "getRenderbufferParameter",
        "getShaderInfoLog",
        "getShaderParameter",
        "getShaderPrecisionFormat",
        "getShaderSource",
        "getSupportedExtensions",
        "getTexParameter",
        "getUniform",
        "getUniformLocation",
        "getVertexAttrib",
        "getVertexAttribOffset",
        "hint",
        "isBuffer",
        "isContextLost",
        "isEnabled",
        "isFramebuffer",
        "isProgram",
        "isRenderbuffer",
        "isShader",
        "isTexture",
        "lineWidth",
        "linkProgram",
        "pixelStorei",
        "polygonOffset",
        "readPixels",
        "releaseShaderCompiler",
        "renderbufferStorage",
        "sampleCoverage",
        "scissor",
        "shaderSource",
        "stencilFunc",
        "stencilFuncSeparate",
        "stencilMask",
        "stencilMaskSeparate",
        "stencilOp",
        "stencilOpSeparate",
        "texImage2D",
        "texParameterf",
        "texParameteri",
        "texSubImage2D",
        "uniform1f",
        "uniform1fv",
        "uniform1i",
        "uniform1iv",
        "uniform2f",
        "uniform2fv",
        "uniform2i",
        "uniform2iv",
        "uniform3f",
        "uniform3fv",
        "uniform3i",
        "uniform3iv",
        "uniform4f",
        "uniform4fv",
        "uniform4i",
        "uniform4iv",
        "uniformMatrix2fv",
        "uniformMatrix3fv",
        "uniformMatrix4fv",
        "useProgram",
        "validateProgram",
        "vertexAttrib1f",
        "vertexAttrib1fv",
        "vertexAttrib2f",
        "vertexAttrib2fv",
        "vertexAttrib3f",
        "vertexAttrib3fv",
        "vertexAttrib4f",
        "vertexAttrib4fv",
        "vertexAttribPointer",
        "viewport",
    ]),
};

WI.RecordingAction._visualNames = {
    [WI.Recording.Type.Canvas2D]: new Set([
        "clearRect",
        "drawFocusIfNeeded",
        "drawImage",
        "drawImageFromRect",
        "fill",
        "fillRect",
        "fillText",
        "putImageData",
        "stroke",
        "strokeRect",
        "strokeText",
        "webkitPutImageDataHD",
    ]),
    [WI.Recording.Type.CanvasWebGL]: new Set([
        "clear",
        "drawArrays",
        "drawElements",
    ]),
};

WI.RecordingAction._stateModifiers = {
    [WI.Recording.Type.Canvas2D]: {
        clearShadow: ["shadowOffsetX", "shadowOffsetY", "shadowBlur", "shadowColor"],
        resetTransform: ["transform"],
        rotate: ["transform"],
        scale: ["transform"],
        setAlpha: ["globalAlpha"],
        setCompositeOperation: ["globalCompositeOperation"],
        setFillColor: ["fillStyle"],
        setLineCap: ["lineCap"],
        setLineJoin: ["lineJoin"],
        setLineWidth: ["lineWidth"],
        setMiterLimit: ["miterLimit"],
        setShadow: ["shadowOffsetX", "shadowOffsetY", "shadowBlur", "shadowColor"],
        setStrokeColor: ["strokeStyle"],
        setTransform: ["transform"],
        translate: ["transform"],
    },
};
