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

WI.RecordingAction = class RecordingAction extends WI.Object
{
    constructor(name, parameters, swizzleTypes, trace, snapshot)
    {
        super();

        this._payloadName = name;
        this._payloadParameters = parameters;
        this._payloadSwizzleTypes = swizzleTypes;
        this._payloadTrace = trace;
        this._payloadSnapshot = snapshot || -1;

        this._name = "";
        this._parameters = [];
        this._trace = [];
        this._snapshot = "";

        this._valid = true;
        this._swizzledPromise = null;

        this._isFunction = false;
        this._isGetter = false;
        this._isVisual = false;
        this._hasVisibleEffect = undefined;
        this._stateModifiers = new Set;
    }

    // Static

    // Payload format: [name, parameters, swizzleTypes, trace, [snapshot]]
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

        if (!Array.isArray(payload[3]))
            payload[3] = [];

        if (payload.length >= 5 && isNaN(payload[4]))
            payload[4] = -1;

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
    get swizzleTypes() { return this._payloadSwizzleTypes; }
    get trace() { return this._trace; }
    get snapshot() { return this._snapshot; }
    get valid() { return this._valid; }
    get isFunction() { return this._isFunction; }
    get isGetter() { return this._isGetter; }
    get isVisual() { return this._isVisual; }
    get hasVisibleEffect() { return this._hasVisibleEffect; }
    get stateModifiers() { return this._stateModifiers; }

    markInvalid()
    {
        let wasValid = this._valid;
        this._valid = false;

        if (wasValid)
            this.dispatchEventToListeners(WI.RecordingAction.Event.ValidityChanged);
    }

    swizzle(recording)
    {
        if (!this._swizzledPromise)
            this._swizzledPromise = this._swizzle(recording);
        return this._swizzledPromise;
    }

    apply(context, options = {})
    {
        if (!this.valid)
            return;

        function getContent() {
            if (context instanceof CanvasRenderingContext2D) {
                let imageData = context.getImageData(0, 0, context.canvas.width, context.canvas.height);
                return [imageData.width, imageData.height, ...imageData.data];
            }

            if (context instanceof WebGLRenderingContext || context instanceof WebGL2RenderingContext) {
                let pixels = new Uint8Array(context.drawingBufferWidth * context.drawingBufferHeight * 4);
                context.readPixels(0, 0, context.canvas.width, context.canvas.height, context.RGBA, context.UNSIGNED_BYTE, pixels);
                return [...pixels];
            }

            if (context.canvas instanceof HTMLCanvasElement)
                return [context.canvas.toDataURL()];

            console.assert("Unknown context type", context);
            return [];
        }

        let contentBefore = null;
        let shouldCheckForChange = this._isVisual && this._hasVisibleEffect === undefined;
        if (shouldCheckForChange)
            contentBefore = getContent();

        try {
            let name = options.nameOverride || this._name;
            if (this.isFunction)
                context[name](...this._parameters);
            else {
                if (this.isGetter)
                    context[name];
                else
                    context[name] = this._parameters[0];
            }

            if (shouldCheckForChange) {
                this._hasVisibleEffect = !Array.shallowEqual(contentBefore, getContent());
                if (!this._hasVisibleEffect)
                    this.dispatchEventToListeners(WI.RecordingAction.Event.HasVisibleEffectChanged);
            }
        } catch {
            this.markInvalid();

            WI.Recording.synthesizeError(WI.UIString("“%s” threw an error.").format(this._name));
        }
    }

    toJSON()
    {
        let json = [this._payloadName, this._payloadParameters, this._payloadSwizzleTypes, this._payloadTrace];
        if (this._payloadSnapshot >= 0)
            json.push(this._payloadSnapshot);
        return json;
    }

    // Private

    async _swizzle(recording)
    {
        let swizzleParameter = (item, index) => {
            return recording.swizzle(item, this._payloadSwizzleTypes[index]);
        };

        let swizzleCallFrame = async (item, index) => {
            let array = await recording.swizzle(item, WI.Recording.Swizzle.None);
            let [functionName, url] = await Promise.all([
                recording.swizzle(array[0], WI.Recording.Swizzle.String),
                recording.swizzle(array[1], WI.Recording.Swizzle.String),
            ]);
            return WI.CallFrame.fromPayload(WI.mainTarget, {
                functionName,
                url,
                lineNumber: array[2],
                columnNumber: array[3],
            });
        };

        let swizzlePromises = [
            recording.swizzle(this._payloadName, WI.Recording.Swizzle.String),
            Promise.all(this._payloadParameters.map(swizzleParameter)),
            Promise.all(this._payloadTrace.map(swizzleCallFrame)),
        ];
        if (this._payloadSnapshot >= 0)
            swizzlePromises.push(recording.swizzle(this._payloadSnapshot, WI.Recording.Swizzle.String));

        let [name, parameters, callFrames, snapshot] = await Promise.all(swizzlePromises);
        this._name = name;
        this._parameters = parameters;
        this._trace = callFrames;
        if (this._payloadSnapshot >= 0)
            this._snapshot = snapshot;

        if (this._valid) {
            let parametersSpecified = this._parameters.every((parameter) => parameter !== undefined);
            let parametersCanBeSwizzled = this._payloadSwizzleTypes.every((swizzleType) => swizzleType !== WI.Recording.Swizzle.None);
            if (!parametersSpecified || !parametersCanBeSwizzled)
                this.markInvalid();
        }

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

    getColorParameters()
    {
        switch (this._name) {
        // 2D
        case "fillStyle":
        case "strokeStyle":
        case "shadowColor":
        // 2D (non-standard, legacy)
        case "setFillColor":
        case "setStrokeColor":
        // WebGL
        case "blendColor":
        case "clearColor":
            return this._parameters;

        // 2D (non-standard, legacy)
        case "setShadow":
            return this._parameters.slice(3);
        }

        return [];
    }

    getImageParameters()
    {
        switch (this._name) {
        // 2D
        case "createImageData":
        case "createPattern":
        case "drawImage":
        case "fillStyle":
        case "putImageData":
        case "strokeStyle":
        // 2D (non-standard)
        case "drawImageFromRect":
            return this._parameters.slice(0, 1);
        }

        return [];
    }
};

WI.RecordingAction.Event = {
    ValidityChanged: "recording-action-marked-invalid",
    HasVisibleEffectChanged: "recording-action-has-visible-effect-changed",
};

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
    ]),
    [WI.Recording.Type.CanvasWebGL]: new Set([
        "clear",
        "drawArrays",
        "drawElements",
    ]),
};

WI.RecordingAction._stateModifiers = {
    [WI.Recording.Type.Canvas2D]: {
        arc: ["currentX", "currentY"],
        arcTo: ["currentX", "currentY"],
        beginPath: ["currentX", "currentY"],
        bezierCurveTo: ["currentX", "currentY"],
        clearShadow: ["shadowOffsetX", "shadowOffsetY", "shadowBlur", "shadowColor"],
        closePath: ["currentX", "currentY"],
        ellipse: ["currentX", "currentY"],
        lineTo: ["currentX", "currentY"],
        moveTo: ["currentX", "currentY"],
        quadraticCurveTo: ["currentX", "currentY"],
        rect: ["currentX", "currentY"],
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
