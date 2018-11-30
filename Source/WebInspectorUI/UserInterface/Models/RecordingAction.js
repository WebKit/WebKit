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
        this._isFunction = false;
        this._isGetter = false;
        this._isVisual = false;

        this._contextReplacer = null;

        this._states = [];
        this._stateModifiers = new Set;

        this._warning = null;
        this._swizzled = false;
        this._processed = false;
    }

    // Static

    // Payload format: (name, parameters, swizzleTypes, [trace, [snapshot]])
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

        if (isNaN(payload[3]) || (!payload[3] && payload[3] !== 0)) {
            // COMPATIBILITY (iOS 12.1): "trace" was sent as an array of call frames instead of a single call stack
            if (!Array.isArray(payload[3]))
                payload[3] = [];
        }

        if (payload.length >= 5 && isNaN(payload[4]))
            payload[4] = -1;

        return new WI.RecordingAction(...payload);
    }

    static isFunctionForType(type, name)
    {
        let prototype = WI.RecordingAction._prototypeForType(type);
        if (!prototype)
            return false;
        let propertyDescriptor = Object.getOwnPropertyDescriptor(prototype, name);
        if (!propertyDescriptor)
            return false;
        return typeof propertyDescriptor.value === "function";
    }

    static constantNameForParameter(type, name, value, index, count)
    {
        let indexesForType = WI.RecordingAction._constantIndexes[type];
        if (!indexesForType)
            return null;

        let indexesForAction = indexesForType[name];
        if (!indexesForAction)
            return null;

        if (Array.isArray(indexesForAction) && !indexesForAction.includes(index))
            return null;

        if (typeof indexesForAction === "object") {
            let indexesForActionVariant = indexesForAction[count];
            if (!indexesForActionVariant)
                return null;

            if (Array.isArray(indexesForActionVariant) && !indexesForActionVariant.includes(index))
                return null;
        }

        if (value === 0 && type === WI.Recording.Type.CanvasWebGL) {
            if (name === "blendFunc" || name === "blendFuncSeparate")
                return "ZERO";
            if (index === 0) {
                if (name === "drawArrays" || name === "drawElements")
                    return "POINTS";
                if (name === "pixelStorei")
                    return "NONE";
            }
        }

        let prototype = WI.RecordingAction._prototypeForType(type);
        for (let key in prototype) {
            let descriptor = Object.getOwnPropertyDescriptor(prototype, key);
            if (descriptor && descriptor.value === value)
                return key;
        }

        return null;
    }

    static _prototypeForType(type)
    {
        if (type === WI.Recording.Type.Canvas2D)
            return CanvasRenderingContext2D.prototype;
        if (type === WI.Recording.Type.CanvasBitmapRenderer)
            return ImageBitmapRenderingContext.prototype;
        if (type === WI.Recording.Type.CanvasWebGL)
            return WebGLRenderingContext.prototype;
        return null;
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
    get contextReplacer() { return this._contextReplacer; }
    get states() { return this._states; }
    get stateModifiers() { return this._stateModifiers; }
    get warning() { return this._warning; }

    get ready()
    {
        return this._swizzled && this._processed;
    }

    process(recording, context, states, {lastAction} = {})
    {
        console.assert(this._swizzled, "You must swizzle() before you can process().");
        console.assert(!this._processed, "You should only process() once.");

        this._processed = true;

        if (recording.type === WI.Recording.Type.CanvasWebGL) {
            // We add each RecordingAction to the list of visualActionIndexes after it is processed.
            if (this._valid && this._isVisual) {
                let contentBefore = recording.visualActionIndexes.length ? recording.actions[recording.visualActionIndexes.lastValue].snapshot : recording.initialState.content;
                if (this._snapshot === contentBefore)
                    this._warning = WI.UIString("This action causes no visual change");
            }
            return;
        }

        function getContent() {
            if (context instanceof CanvasRenderingContext2D)
                return context.getImageData(0, 0, context.canvas.width, context.canvas.height).data;

            if (context instanceof WebGLRenderingContext || context instanceof WebGL2RenderingContext) {
                let pixels = new Uint8Array(context.drawingBufferWidth * context.drawingBufferHeight * 4);
                context.readPixels(0, 0, context.canvas.width, context.canvas.height, context.RGBA, context.UNSIGNED_BYTE, pixels);
                return pixels;
            }

            if (context.canvas instanceof HTMLCanvasElement)
                return [context.canvas.toDataURL()];

            console.assert("Unknown context type", context);
            return [];
        }

        let contentBefore = null;
        let shouldCheckHasVisualEffect = this._valid && this._isVisual;
        if (shouldCheckHasVisualEffect)
            contentBefore = getContent();

        this.apply(context);

        if (shouldCheckHasVisualEffect) {
            let contentAfter = getContent();
            if (Array.shallowEqual(contentBefore, contentAfter))
                this._warning = WI.UIString("This action causes no visual change");
        }

        if (recording.type === WI.Recording.Type.Canvas2D) {
            let currentState = WI.RecordingState.fromContext(recording.type, context, {source: this});
            console.assert(currentState);

            if (this.name === "save")
                states.push(currentState);
            else if (this.name === "restore")
                states.pop();

            this._states = states.slice();
            this._states.push(currentState);

            let lastState = null;
            if (lastAction) {
                let previousState = lastAction.states.lastValue;
                for (let [name, value] of currentState) {
                    let previousValue = previousState.get(name);
                    if (value !== previousValue && !Object.shallowEqual(value, previousValue))
                        this._stateModifiers.add(name);
                }
            }

            if (WI.ImageUtilities.supportsCanvasPathDebugging()) {
                let currentX = currentState.currentX;
                let invalidX = (currentX < 0 || currentX >= context.canvas.width) && (!lastState || currentX !== lastState.currentX);

                let currentY = currentState.currentY;
                let invalidY = (currentY < 0 || currentY >= context.canvas.height) && (!lastState || currentY !== lastState.currentY);

                if (invalidX || invalidY)
                    this._warning = WI.UIString("This action moves the path outside the visible area");
            }
        }
    }

    async swizzle(recording, lastAction)
    {
        console.assert(!this._swizzled, "You should only swizzle() once.");

        if (!this._valid) {
            this._swizzled = true;
            return;
        }

        let swizzleParameter = (item, index) => {
            return recording.swizzle(item, this._payloadSwizzleTypes[index]);
        };

        let swizzlePromises = [
            recording.swizzle(this._payloadName, WI.Recording.Swizzle.String),
            Promise.all(this._payloadParameters.map(swizzleParameter)),
        ];

        if (!isNaN(this._payloadTrace))
            swizzlePromises.push(recording.swizzle(this._payloadTrace, WI.Recording.Swizzle.CallStack))
        else {
            // COMPATIBILITY (iOS 12.1): "trace" was sent as an array of call frames instead of a single call stack
            swizzlePromises.push(Promise.all(this._payloadTrace.map((item) => recording.swizzle(item, WI.Recording.Swizzle.CallFrame))));
        }

        if (this._payloadSnapshot >= 0)
            swizzlePromises.push(recording.swizzle(this._payloadSnapshot, WI.Recording.Swizzle.String));

        let [name, parameters, callFrames, snapshot] = await Promise.all(swizzlePromises);
        this._name = name;
        this._parameters = parameters;
        this._trace = callFrames;
        if (this._payloadSnapshot >= 0)
            this._snapshot = snapshot;

        if (recording.type === WI.Recording.Type.Canvas2D || recording.type === WI.Recording.Type.CanvasBitmapRenderer || recording.type === WI.Recording.Type.CanvasWebGL) {
            if (this._name === "width" || this._name === "height") {
                this._contextReplacer = "canvas";
                this._isFunction = false;
                this._isGetter = !this._parameters.length;
                this._isVisual = !this._isGetter;
            }

            // FIXME: <https://webkit.org/b/180833>
        }

        if (!this._contextReplacer) {
            this._isFunction = WI.RecordingAction.isFunctionForType(recording.type, this._name);
            this._isGetter = !this._isFunction && !this._parameters.length;

            let visualNames = WI.RecordingAction._visualNames[recording.type];
            this._isVisual = visualNames ? visualNames.has(this._name) : false;

            if (this._valid) {
                let prototype = WI.RecordingAction._prototypeForType(recording.type);
                if (prototype && !(name in prototype)) {
                    this.markInvalid();

                    WI.Recording.synthesizeError(WI.UIString("\u0022%s\u0022 is invalid.").format(name));
                }
            }
        }

        if (this._valid) {
            let parametersSpecified = this._parameters.every((parameter) => parameter !== undefined);
            let parametersCanBeSwizzled = this._payloadSwizzleTypes.every((swizzleType) => swizzleType !== WI.Recording.Swizzle.None);
            if (!parametersSpecified || !parametersCanBeSwizzled)
                this.markInvalid();
        }

        if (this._valid) {
            let stateModifiers = WI.RecordingAction._stateModifiers[recording.type];
            if (stateModifiers) {
                this._stateModifiers.add(this._name);
                let modifiedByAction = stateModifiers[this._name] || [];
                for (let item of modifiedByAction)
                    this._stateModifiers.add(item);
            }
        }

        this._swizzled = true;
    }

    apply(context, options = {})
    {
        console.assert(this._swizzled, "You must swizzle() before you can apply().");
        console.assert(this._processed, "You must process() before you can apply().");

        if (!this.valid)
            return;

        try {
            let name = options.nameOverride || this._name;

            if (this._contextReplacer)
                context = context[this._contextReplacer];

            if (this.isFunction)
                context[name](...this._parameters);
            else {
                if (this.isGetter)
                    context[name];
                else
                    context[name] = this._parameters[0];
            }
        } catch {
            this.markInvalid();

            WI.Recording.synthesizeError(WI.UIString("\u0022%s\u0022 threw an error.").format(this._name));
        }
    }

    markInvalid()
    {
        if (!this._valid)
            return;

        this._valid = false;

        this.dispatchEventToListeners(WI.RecordingAction.Event.ValidityChanged);
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
        case "colorMask":
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
        // BitmapRenderer
        case "transferFromImageBitmap":
            return this._parameters.slice(0, 1);

        // WebGL
        case "texImage2D":
        case "texSubImage2D":
        case "compressedTexImage2D":
            return [this._parameters.lastValue];
        }

        return [];
    }

    toJSON()
    {
        let json = [this._payloadName, this._payloadParameters, this._payloadSwizzleTypes, this._payloadTrace];
        if (this._payloadSnapshot >= 0)
            json.push(this._payloadSnapshot);
        return json;
    }
};

WI.RecordingAction.Event = {
    ValidityChanged: "recording-action-marked-invalid",
};

WI.RecordingAction._constantIndexes = {
    [WI.Recording.Type.CanvasWebGL]: {
        "activeTexture": true,
        "bindBuffer": true,
        "bindFramebuffer": true,
        "bindRenderbuffer": true,
        "bindTexture": true,
        "blendEquation": true,
        "blendEquationSeparate": true,
        "blendFunc": true,
        "blendFuncSeparate": true,
        "bufferData": [0, 2],
        "bufferSubData": [0],
        "checkFramebufferStatus": true,
        "compressedTexImage2D": [0, 2],
        "compressedTexSubImage2D": [0],
        "copyTexImage2D": [0, 2],
        "copyTexSubImage2D": [0],
        "createShader": true,
        "cullFace": true,
        "depthFunc": true,
        "disable": true,
        "drawArrays": [0],
        "drawElements": [0, 2],
        "enable": true,
        "framebufferRenderbuffer": true,
        "framebufferTexture2D": [0, 1, 2],
        "frontFace": true,
        "generateMipmap": true,
        "getBufferParameter": true,
        "getFramebufferAttachmentParameter": true,
        "getParameter": true,
        "getProgramParameter": true,
        "getRenderbufferParameter": true,
        "getShaderParameter": true,
        "getShaderPrecisionFormat": true,
        "getTexParameter": true,
        "getVertexAttrib": [1],
        "getVertexAttribOffset": [1],
        "hint": true,
        "isEnabled": true,
        "pixelStorei": [0],
        "readPixels": [4, 5],
        "renderbufferStorage": [0, 1],
        "stencilFunc": [0],
        "stencilFuncSeparate": [0, 1],
        "stencilMaskSeparate": [0],
        "stencilOp": true,
        "stencilOpSeparate": true,
        "texImage2D": {
            5: [0, 2, 3, 4],
            6: [0, 2, 3, 4],
            8: [0, 2, 6, 7],
            9: [0, 2, 6, 7],
        },
        "texParameterf": [0, 1],
        "texParameteri": [0, 1],
        "texSubImage2D": {
            6: [0, 4, 5],
            7: [0, 4, 5],
            8: [0, 6, 7],
            9: [0, 6, 7],
        },
        "vertexAttribPointer": [2],
    },
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
    [WI.Recording.Type.CanvasBitmapRenderer]: new Set([
        "transferFromImageBitmap",
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
