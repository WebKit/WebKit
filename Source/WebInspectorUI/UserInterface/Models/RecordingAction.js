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
    constructor(name, parameters, swizzleTypes, stackTrace, result, receiver, snapshot)
    {
        super();

        this._payloadName = name;
        this._payloadParameters = parameters;
        this._payloadSwizzleTypes = swizzleTypes;
        this._payloadStackTrace = stackTrace;
        this._payloadResult = result ?? -1;
        this._payloadReceiver = receiver ?? -1;
        this._payloadSnapshot = snapshot ?? -1;

        this._name = "";
        this._parameters = [];
        this._stackTrace = null;
        this._snapshot = "";
        this._receiver = "";
        this._result = "";

        this._valid = true;
        this._isFunction = false;
        this._isGetter = false;
        this._isVisual = false;

        this._states = [];
        this._stateModifiers = new Set;

        this._warning = null;
        this._swizzled = false;
        this._processed = false;
    }

    // Static

    static fromPayload(payload, version)
    {
        // Version 1: (name, parameters, swizzleTypes, [stackTrace, [snapshot]])
        // Version 2: (name, parameters, swizzleTypes, [stackTrace, [snapshot]])
        // Version 3: (name, parameters, swizzleTypes, [stackTrace, [result, [receiver, [snapshot]]]])

        if (!Array.isArray(payload))
            payload = [];

        let [name, parameters, swizzleTypes, stackTrace, result, receiver, snapshot] = payload;

        if (typeof name !== "number") {
            if (payload.length > 0)
                WI.Recording.synthesizeWarning(WI.UIString("non-number %s").format(WI.unlocalizedString("name")));

            name = -1;
        }

        if (!Array.isArray(parameters)) {
            if (payload.length > 1)
                WI.Recording.synthesizeWarning(WI.UIString("non-array %s").format(WI.unlocalizedString("parameters")));

            parameters = [];
        }

        if (!Array.isArray(swizzleTypes)) {
            if (payload.length > 2)
                WI.Recording.synthesizeWarning(WI.UIString("non-array %s").format(WI.unlocalizedString("swizzleTypes")));

            swizzleTypes = [];
        }

        if (typeof stackTrace !== "number" || isNaN(stackTrace) || (!stackTrace && stackTrace !== 0)) {
            if (payload.length > 3)
                WI.Recording.synthesizeWarning(WI.UIString("non-number %s").format(WI.unlocalizedString("stackTrace")));

            stackTrace = [];
        }

        if (version < 3) {
            snapshot = result;
            result = -1;
            receiver = -1;
        } else if (!Array.isArray(result) || result.length !== 2 || result.some((item) => typeof item !== "number" || isNaN(item))) {
            if (payload.length > 4 && result !== -1)
                WI.Recording.synthesizeWarning(WI.UIString("non-array %s").format(WI.unlocalizedString("result")));

            result = -1;
        }

        if (version >= 3 && (!Array.isArray(receiver) || receiver.length !== 2 || receiver.some((item) => typeof item !== "number" || isNaN(item)))) {
            if (payload.length > 5 && receiver !== -1)
                WI.Recording.synthesizeWarning(WI.UIString("non-array %s").format(WI.unlocalizedString("receiver")));

            receiver = -1;
        }

        if (typeof snapshot !== "number" || isNaN(snapshot)) {
            if (payload.length > (version >= 3 ? 6 : 4))
                WI.Recording.synthesizeWarning(WI.UIString("non-number %s").format(WI.unlocalizedString("snapshot")));

            snapshot = -1;
        }

        return new WI.RecordingAction(name, parameters, swizzleTypes, stackTrace, result, receiver, snapshot);
    }

    static isFunctionForType(type, name)
    {
        if (type === WI.Recording.Type.CanvasWebGPU)
            return !WI.RecordingAction._webGPUAttributeNames.has(name);

        let prototype = WI.RecordingAction._prototypeForType(type);
        if (!prototype)
            return false;
        let propertyDescriptor = Object.getOwnPropertyDescriptor(prototype, name);
        if (!propertyDescriptor)
            return false;
        return typeof propertyDescriptor.value === "function";
    }

    static bitfieldNamesForParameter(type, name, value, index, count, path = [])
    {
        if (!value)
            return null;

        let bitfield = WI.RecordingAction._bitfields[type]?.[name]?.[index];
        for (let propertyName of path)
            bitfield = bitfield?.[propertyName];
        if (!bitfield)
            return null;

        let namespaceName = typeof bitfield === "string" ? bitfield : "context";
        let namespace = namespaceName === "context" ? WI.RecordingAction._prototypeForType(type) : window[namespaceName];
        if (!namespace)
            return null;

        let constantNames = Array.isArray(bitfield) ? bitfield : Object.getOwnPropertyNames(namespace);
        for (let name of constantNames) {
            let bit = namespace[name];
            if (typeof bit === "number" && bit === value)
                return [`${namespaceName}.${name}`];
        }

        function testAndClearBit(bit, name) {
            if (!bit)
                return;

            if (value & bit)
                names.push(name);

            value = value & ~bit;
        }

        function hexString(value) {
            return "0x" + value.toString(16);
        }

        let names = [];

        for (let name of constantNames) {
            let bit = namespace[name];
            if (typeof bit === "number" && !(bit & (bit - 1)))
                testAndClearBit(bit, `${namespaceName}.${name}`);
        }
        if (value)
            names.push(hexString(value));

        return names.length ? names : null;
    }

    static constantNameForParameter(type, name, value, index, count)
    {
        let indexesForType = WI.RecordingAction._constantIndexes[type];
        if (!indexesForType)
            return null;

        let indexesForAction = indexesForType[name];
        if (!indexesForAction)
            return null;

        if (Array.isArray(indexesForAction)) {
            if (!indexesForAction.includes(index))
                return null;
        } else if (typeof indexesForAction === "object") {
            let indexesForActionVariant = indexesForAction[count];
            if (!indexesForActionVariant)
                return null;

            if (Array.isArray(indexesForActionVariant) && !indexesForActionVariant.includes(index))
                return null;
        }

        let isWebGL = type === WI.Recording.Type.CanvasWebGL || type === WI.Recording.Type.OffscreenCanvasWebGL;
        let isWebGL2 = type === WI.Recording.Type.CanvasWebGL2 || type === WI.Recording.Type.OffscreenCanvasWebGL2;

        if (value === 0 && (isWebGL || isWebGL2)) {
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
        if (prototype) {
            for (let key in prototype) {
                let descriptor = Object.getOwnPropertyDescriptor(prototype, key);
                if (descriptor && descriptor.value === value)
                    return key;
            }
        }

        return null;
    }

    static objectReferencesForParameter(type, name, value, index)
    {
        let result = new Set;
        WI.RecordingAction._objectReferenceCollectors[type]?.[name]?.[index]?.(value, function(reference, swizzleTypes) {
            if (!Array.isArray(reference) || reference.length !== 2)
                return;

            let [identifier, swizzleType] = reference;
            if (typeof identifier !== "number" || isNaN(identifier) || !swizzleTypes.includes(swizzleType))
                return;

            result.add(reference);
        });
        return result;
    }

    static _prototypeForType(type)
    {
        switch (type) {
        case WI.Recording.Type.Canvas2D:
            return CanvasRenderingContext2D.prototype;
        case WI.Recording.Type.OffscreenCanvas2D:
            if (window.OffscreenCanvasRenderingContext2D)
                return OffscreenCanvasRenderingContext2D.prototype;
            break;
        case WI.Recording.Type.CanvasBitmapRenderer:
        case WI.Recording.Type.OffscreenCanvasBitmapRenderer:
            if (window.ImageBitmapRenderingContext)
                return ImageBitmapRenderingContext.prototype;
            break;
        case WI.Recording.Type.CanvasWebGL:
        case WI.Recording.Type.OffscreenCanvasWebGL:
            if (window.WebGLRenderingContext)
                return WebGLRenderingContext.prototype;
            break;
        case WI.Recording.Type.CanvasWebGL2:
        case WI.Recording.Type.OffscreenCanvasWebGL2:
            if (window.WebGL2RenderingContext)
                return WebGL2RenderingContext.prototype;
            break;
        case WI.Recording.Type.CanvasWebGPU:
            return null;
        }

        WI.reportInternalError("Unknown recording type: " + type);
        return null;
    }

    // Public

    get name() { return this._name; }
    get parameters() { return this._parameters; }
    get swizzleTypes() { return this._payloadSwizzleTypes; }

    async recordingObjectIdentifiers(recording)
    {
        let result = [];
        if (Array.isArray(this._payloadResult))
            result.push(this._payloadResult);
        if (Array.isArray(this._payloadReceiver))
            result.push(this._payloadReceiver);

        let name = await recording.swizzle(this._payloadName, WI.Recording.Swizzle.String);
        for (let index = 0; index < this._payloadSwizzleTypes.length; ++index) {
            let swizzleType = this._payloadSwizzleTypes[index];
            let value = this._payloadParameters[index];
            if (WI.Recording.isReferenceSwizzleType(swizzleType))
                value = await recording.swizzle(value, swizzleType);
            if (WI.Recording.isObjectSwizzleType(swizzleType) && !isNaN(value))
                result.push([value, swizzleType]);
            else {
                for (let reference of WI.RecordingAction.objectReferencesForParameter(recording.type, name, value, index)) {
                    if (WI.Recording.isObjectSwizzleType(reference[1]))
                        result.push(reference);
                }
            }
        }

        return result;
    }

    get stackTrace() { return this._stackTrace; }
    get snapshot() { return this._snapshot; }
    get receiver() { return this._receiver; }
    get result() { return this._result; }
    get valid() { return this._valid; }
    get isFunction() { return this._isFunction; }
    get isGetter() { return this._isGetter; }
    get isVisual() { return this._isVisual; }
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

        if (recording.isCanvasWebGL || recording.isCanvasWebGL2 || recording.isCanvasWebGPU) {
            // We add each RecordingAction to the list of visualActionIndexes after it is processed.
            if (this._valid && this._isVisual) {
                let contentBefore = recording.visualActionIndexes.length ? recording.actions[recording.visualActionIndexes.lastValue].snapshot : recording.initialState.content;
                if (this._snapshot === contentBefore)
                    this._warning = WI.UIString("This action causes no visual change");
            }
            return;
        }

        function getContent() {
            if (context instanceof CanvasRenderingContext2D || (window.OffscreenCanvasRenderingContext2D && context instanceof OffscreenCanvasRenderingContext2D))
                return context.getImageData(0, 0, context.canvas.width, context.canvas.height).data;

            if ((window.WebGLRenderingContext && context instanceof WebGLRenderingContext) || (window.WebGL2RenderingContext && context instanceof WebGL2RenderingContext)) {
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

        if (recording.isCanvas2D) {
            let currentState = WI.RecordingState.fromCanvasContext2D(context, {source: this});
            console.assert(currentState);

            if (this.name === "save")
                states.push(currentState);
            else if (this.name === "restore")
                states.pop();

            this._states = states.slice();
            this._states.push(currentState);

            let lastState = null;
            if (lastAction) {
                lastState = lastAction.states.lastValue;
                for (let [name, value] of currentState) {
                    let previousValue = lastState.get(name);
                    if (value !== previousValue && !Object.shallowEqual(value, previousValue))
                        this._stateModifiers.add(name);
                }
            }

            let currentX = currentState.get("currentX");
            let invalidX = (currentX < 0 || currentX >= context.canvas.width) && (!lastState || currentX !== lastState.get("currentX"));

            let currentY = currentState.get("currentY");
            let invalidY = (currentY < 0 || currentY >= context.canvas.height) && (!lastState || currentY !== lastState.get("currentY"));

            if (invalidX || invalidY)
                this._warning = WI.UIString("This action moves the path outside the visible area");
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

        let [name, parameters, stackTrace, snapshot] = await Promise.all([
            recording.swizzle(this._payloadName, WI.Recording.Swizzle.String),
            Promise.all(this._payloadParameters.map(swizzleParameter)),
            recording.swizzle(this._payloadStackTrace, WI.Recording.Swizzle.CallStack),
            this._payloadSnapshot >= 0 ? recording.swizzle(this._payloadSnapshot, WI.Recording.Swizzle.String) : "",
        ]);
        this._name = name;
        this._parameters = parameters;
        this._stackTrace = stackTrace;
        if (this._payloadSnapshot >= 0)
            this._snapshot = snapshot;
        if (Array.isArray(this._payloadResult))
            this._result = recording.displayNameForReference(this._payloadResult);
        if (Array.isArray(this._payloadReceiver))
            this._receiver = recording.displayNameForReference(this._payloadReceiver);

        if (this.isCanvasReceiver) {
            this._isFunction = false;
            this._isGetter = !this._parameters.length;
            this._isVisual = !this._isGetter;
        } else {
            let isWebGLObjectAction = this._receiver && (recording.isCanvasWebGL || recording.isCanvasWebGL2);
            let isDefinitelyFunction = this._parameters.length > 1;
            this._isFunction = isWebGLObjectAction || isDefinitelyFunction || WI.RecordingAction.isFunctionForType(recording.type, this._name);
            this._isGetter = !this._isFunction && !this._parameters.length;

            if (this._snapshot)
                this._isVisual = true;
            else {
                let visualNames = WI.RecordingAction._visualNames[recording.type];
                this._isVisual = visualNames ? visualNames.has(this._name) : false;
            }

            if (this._valid && !isWebGLObjectAction && !isDefinitelyFunction) {
                let prototype = WI.RecordingAction._prototypeForType(recording.type);
                if (prototype && !(name in prototype)) {
                    this.markInvalid();

                    WI.Recording.synthesizeWarning(WI.UIString("\u0022%s\u0022 is not valid for %s").format(name, prototype.constructor.name));
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

    get isCanvasReceiver()
    {
        return this._receiver === "canvas" || (!this._receiver && (this._name === "width" || this._name === "height"));
    }

    apply(context, options = {})
    {
        console.assert(this._swizzled, "You must swizzle() before you can apply().");
        console.assert(this._processed, "You must process() before you can apply().");

        if (!this.valid)
            return;

        try {
            let name = options.nameOverride || this._name;

            if (this.isCanvasReceiver)
                context = context.canvas;

            if (this.isFunction)
                context[name](...this._parameters);
            else {
                if (this.isGetter)
                    void context[name];
                else
                    context[name] = this._parameters[0];
            }
        } catch {
            this.markInvalid();

            WI.Recording.synthesizeWarning(WI.UIString("\u0022%s\u0022 threw an error").format(this._name));
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

    toJSON(version)
    {
        let json = [this._payloadName, this._payloadParameters, this._payloadSwizzleTypes, this._payloadStackTrace];
        if (version >= 3) {
            if (this._payloadSnapshot >= 0) {
                json.push(this._payloadResult);
                json.push(this._payloadReceiver);
                json.push(this._payloadSnapshot);
            } else if (Array.isArray(this._payloadReceiver)) {
                json.push(this._payloadResult);
                json.push(this._payloadReceiver);
            } else if (Array.isArray(this._payloadResult))
                json.push(this._payloadResult);
            return json;
        }

        if (this._payloadSnapshot >= 0)
            json.push(this._payloadSnapshot);
        return json;
    }
};

WI.RecordingAction.Event = {
    ValidityChanged: "recording-action-marked-invalid",
};

WI.RecordingAction._bitfields = {
    [WI.Recording.Type.CanvasWebGL]: {
        "clear": {
            0: ["COLOR_BUFFER_BIT", "DEPTH_BUFFER_BIT", "STENCIL_BUFFER_BIT"],
        },
    },
    [WI.Recording.Type.CanvasWebGL2]: {
        "blitFramebuffer": {
            8: ["COLOR_BUFFER_BIT", "DEPTH_BUFFER_BIT", "STENCIL_BUFFER_BIT"],
        },
        "clear": {
            0: ["COLOR_BUFFER_BIT", "DEPTH_BUFFER_BIT", "STENCIL_BUFFER_BIT"],
        },
        "clientWaitSync": {
            1: ["SYNC_FLUSH_COMMANDS_BIT"],
        },
    },
    [WI.Recording.Type.CanvasWebGPU]: {
        "createBindGroupLayout": {
            0: {
                "entries": {
                    "visibility": "GPUShaderStage",
                },
            },
        },
        "createBuffer": {
            0: {
                "usage": "GPUBufferUsage",
            },
        },
        "createRenderPipeline": {
            0: {
                "fragment": {
                    "targets": {
                        "writeMask": "GPUColorWrite",
                    },
                },
            },
        },
        "createRenderPipelineAsync": {
            0: {
                "fragment": {
                    "targets": {
                        "writeMask": "GPUColorWrite",
                    },
                },
            },
        },
        "createTexture": {
            0: {
                "usage": "GPUTextureUsage",
            },
        },
        "createView": {
            0: {
                "usage": "GPUTextureUsage",
            },
        },
        "mapAsync": {
            0: "GPUMapMode",
        },
    },
};
WI.RecordingAction._bitfields[WI.Recording.Type.OffscreenCanvasWebGL] = WI.RecordingAction._bitfields[WI.Recording.Type.CanvasWebGL];
WI.RecordingAction._bitfields[WI.Recording.Type.OffscreenCanvasWebGL2] = WI.RecordingAction._bitfields[WI.Recording.Type.CanvasWebGL2];

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
    [WI.Recording.Type.CanvasWebGL2]: {
        "activeTexture": true,
        "beginQuery": [0],
        "beginTransformFeedback": true,
        "bindBuffer": true,
        "bindBufferBase": [0],
        "bindBufferRange": [0],
        "bindFramebuffer": true,
        "bindRenderbuffer": true,
        "bindTexture": true,
        "bindTransformFeedback": [0],
        "blendEquation": true,
        "blendEquationSeparate": true,
        "blendFunc": true,
        "blendFuncSeparate": true,
        "blitFramebuffer": [10],
        "bufferData": [0, 2],
        "bufferSubData": [0],
        "checkFramebufferStatus": true,
        "clearBufferfi": [0],
        "clearBufferfv": [0],
        "clearBufferiv": [0],
        "clearBufferuiv": [0],
        "compressedTexImage2D": [0, 2],
        "compressedTexSubImage2D": [0],
        "compressedTexSubImage3D": [0],
        "copyBufferSubData": [0, 1],
        "copyTexImage2D": [0, 2],
        "copyTexSubImage2D": [0],
        "copyTexSubImage3D": [0],
        "createShader": true,
        "cullFace": true,
        "depthFunc": true,
        "disable": true,
        "drawArrays": [0],
        "drawArraysInstanced": [0],
        "drawBuffers": true,
        "drawElements": [0, 2],
        "drawElementsInstanced": [0, 2],
        "drawRangeElements": [0, 4],
        "enable": true,
        "endQuery": true,
        "fenceSync": [0],
        "framebufferRenderbuffer": true,
        "framebufferTexture2D": [0, 1, 2],
        "framebufferTextureLayer": [0, 1],
        "frontFace": true,
        "generateMipmap": true,
        "getActiveUniformBlockParameter": [2],
        "getActiveUniforms": [2],
        "getBufferParameter": true,
        "getBufferSubData": [0],
        "getFramebufferAttachmentParameter": true,
        "getIndexedParameter": [0],
        "getInternalformatParameter": true,
        "getParameter": true,
        "getProgramParameter": true,
        "getQuery": true,
        "getQueryParameter": [1],
        "getRenderbufferParameter": true,
        "getSamplerParameter": [1],
        "getShaderParameter": true,
        "getShaderPrecisionFormat": true,
        "getSyncParameter": [1],
        "getTexParameter": true,
        "getVertexAttrib": [1],
        "getVertexAttribOffset": [1],
        "hint": true,
        "invalidateFramebuffer": [0, 1],
        "invalidateSubFramebuffer": [0, 1],
        "isEnabled": true,
        "pixelStorei": [0],
        "readBuffer": true,
        "readPixels": [4, 5],
        "renderbufferStorage": [0, 1],
        "renderbufferStorageMultisample": [0, 2],
        "samplerParameterf": [1],
        "samplerParameteri": [1],
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
            10: [0, 2, 6, 7],
            11: [0, 2, 7, 8],
        },
        "texParameterf": [0, 1],
        "texParameteri": [0, 1],
        "texStorage2D":[0, 2],
        "texSubImage2D": {
            6: [0, 4, 5],
            7: [0, 4, 5],
            8: [0, 6, 7],
            9: [0, 6, 7],
            10: [0, 6, 7],
            11: [0, 8, 9],
            12: [0, 8, 9],
        },
        "transformFeedbackVaryings": [2],
        "vertexAttribIPointer": [2],
        "vertexAttribPointer": [2],
    },
};
WI.RecordingAction._constantIndexes[WI.Recording.Type.OffscreenCanvasWebGL] = WI.RecordingAction._constantIndexes[WI.Recording.Type.CanvasWebGL];
WI.RecordingAction._constantIndexes[WI.Recording.Type.OffscreenCanvasWebGL2] = WI.RecordingAction._constantIndexes[WI.Recording.Type.CanvasWebGL2];

WI.RecordingAction._objectReferenceCollectors = {
    [WI.Recording.Type.CanvasWebGPU]: {
        "beginComputePass": [
            function(value, collect) {
                collect(value?.timestampWrites?.querySet, [WI.Recording.Swizzle.GPUQuerySet]);
            },
        ],
        "beginRenderPass": [
            function(value, collect) {
                let textureSwizzleTypes = [WI.Recording.Swizzle.GPUTexture, WI.Recording.Swizzle.GPUTextureView];
                for (let attachment of (Array.isArray(value?.colorAttachments) ? value.colorAttachments : [])) {
                    if (!attachment)
                        continue;
                    collect(attachment.view, textureSwizzleTypes);
                    collect(attachment.resolveTarget, textureSwizzleTypes);
                }
                collect(value?.depthStencilAttachment?.view, textureSwizzleTypes);
                collect(value?.occlusionQuerySet, [WI.Recording.Swizzle.GPUQuerySet]);
                collect(value?.timestampWrites?.querySet, [WI.Recording.Swizzle.GPUQuerySet]);
            },
        ],
        "copyBufferToTexture": [
            function(value, collect) {
                collect(value?.buffer, [WI.Recording.Swizzle.GPUBuffer]);
            },
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
        ],
        "copyElementImageToTexture": [
            function(value, collect) {
                collect(value?.source, [WI.Recording.Swizzle.None]);
            },
            function(value, collect) {
                collect(value?.destination?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
        ],
        "copyExternalImageToTexture": [
            function(value, collect) {
                collect(value?.source, [
                    WI.Recording.Swizzle.Image,
                    WI.Recording.Swizzle.ImageBitmap,
                    WI.Recording.Swizzle.ImageData,
                ]);
            },
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
        ],
        "copyTextureToBuffer": [
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
            function(value, collect) {
                collect(value?.buffer, [WI.Recording.Swizzle.GPUBuffer]);
            },
        ],
        "copyTextureToTexture": [
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
        ],
        "createBindGroup": [
            function(value, collect) {
                let descriptor = value;
                collect(descriptor?.layout, [WI.Recording.Swizzle.GPUBindGroupLayout]);
                for (let entry of (Array.isArray(descriptor?.entries) ? descriptor.entries : [])) {
                    if (Array.isArray(entry?.resource)) {
                        collect(entry.resource, [
                            WI.Recording.Swizzle.GPUBuffer,
                            WI.Recording.Swizzle.GPUExternalTexture,
                            WI.Recording.Swizzle.GPUSampler,
                            WI.Recording.Swizzle.GPUTexture,
                            WI.Recording.Swizzle.GPUTextureView,
                        ]);
                    } else
                        collect(entry?.resource?.buffer, [WI.Recording.Swizzle.GPUBuffer]);
                }
            },
        ],
        "createComputePipeline": [
            function(value, collect) {
                let descriptor = value;
                collect(descriptor?.layout, [WI.Recording.Swizzle.GPUPipelineLayout]);
                collect(descriptor?.compute?.module, [WI.Recording.Swizzle.GPUShaderModule]);
            },
        ],
        "createComputePipelineAsync": [
            function(value, collect) {
                let descriptor = value;
                collect(descriptor?.layout, [WI.Recording.Swizzle.GPUPipelineLayout]);
                collect(descriptor?.compute?.module, [WI.Recording.Swizzle.GPUShaderModule]);
            },
        ],
        "createPipelineLayout": [
            function(value, collect) {
                for (let bindGroupLayout of (Array.isArray(value?.bindGroupLayouts) ? value.bindGroupLayouts : []))
                    collect(bindGroupLayout, [WI.Recording.Swizzle.GPUBindGroupLayout]);
            },
        ],
        "createRenderPipeline": [
            function(value, collect) {
                let descriptor = value;
                collect(descriptor?.layout, [WI.Recording.Swizzle.GPUPipelineLayout]);
                collect(descriptor?.vertex?.module, [WI.Recording.Swizzle.GPUShaderModule]);
                collect(descriptor?.fragment?.module, [WI.Recording.Swizzle.GPUShaderModule]);
            },
        ],
        "createRenderPipelineAsync": [
            function(value, collect) {
                let descriptor = value;
                collect(descriptor?.layout, [WI.Recording.Swizzle.GPUPipelineLayout]);
                collect(descriptor?.vertex?.module, [WI.Recording.Swizzle.GPUShaderModule]);
                collect(descriptor?.fragment?.module, [WI.Recording.Swizzle.GPUShaderModule]);
            },
        ],
        "createShaderModule": [
            function(value, collect) {
                for (let hint of Object.values(value?.hints || {}))
                    collect(hint?.layout, [WI.Recording.Swizzle.GPUPipelineLayout]);
            },
        ],
        "executeBundles": [
            function(value, collect) {
                for (let bundle of (Array.isArray(value) ? value : []))
                    collect(bundle, [WI.Recording.Swizzle.GPURenderBundle]);
            },
        ],
        "importExternalTexture": [
            function(value, collect) {
                collect(value?.source, [WI.Recording.Swizzle.Image]);
            },
        ],
        "submit": [
            function(value, collect) {
                for (let commandBuffer of (Array.isArray(value) ? value : []))
                    collect(commandBuffer, [WI.Recording.Swizzle.GPUCommandBuffer]);
            },
        ],
        "writeTexture": [
            function(value, collect) {
                collect(value?.texture, [WI.Recording.Swizzle.GPUTexture]);
            },
        ],
    },
};

WI.RecordingAction._webGPUAttributeNames = new Set([
    "label",
]);

WI.RecordingAction._visualNames = {
    [WI.Recording.Type.Canvas2D]: new Set([
        "clearRect",
        "drawFocusIfNeeded",
        "drawImage",
        "fill",
        "fillRect",
        "fillText",
        "putImageData",
        "stroke",
        "strokeRect",
        "strokeText",
    ]),
    [WI.Recording.Type.OffscreenCanvas2D]: new Set([
        "clearRect",
        "drawImage",
        "fill",
        "fillRect",
        "fillText",
        "putImageData",
        "strokeRect",
        "strokeText",   
    ]),
    [WI.Recording.Type.CanvasBitmapRenderer]: new Set([
        "transferFromImageBitmap",
    ]),
    [WI.Recording.Type.CanvasWebGL]: new Set([
        "clear",
        "drawArrays",
        "drawArraysInstancedANGLE",
        "drawElements",
        "drawElementsInstancedANGLE",
        "multiDrawArraysWEBGL",
        "multiDrawArraysInstancedWEBGL",
        "multiDrawElementsWEBGL",
        "multiDrawElementsInstancedWEBGL",
    ]),
    [WI.Recording.Type.CanvasWebGL2]: new Set([
        "clear",
        "drawArrays",
        "drawArraysInstanced",
        "drawArraysInstancedBaseInstanceWEBGL",
        "drawElements",
        "drawElementsInstanced",
        "drawElementsInstancedBaseVertexBaseInstanceWEBGL",
        "multiDrawArraysWEBGL",
        "multiDrawArraysInstancedBaseInstanceWEBGL",
        "multiDrawArraysInstancedWEBGL",
        "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
        "multiDrawElementsInstancedWEBGL",
        "multiDrawElementsWEBGL",
    ]),
    [WI.Recording.Type.CanvasWebGPU]: new Set([
        "copyElementImageToTexture",
        "copyExternalImageToTexture",
        "submit",
        "writeTexture",
    ]),
};
WI.RecordingAction._visualNames[WI.Recording.Type.OffscreenCanvasBitmapRenderer] = WI.RecordingAction._visualNames[WI.Recording.Type.CanvasBitmapRenderer];
WI.RecordingAction._visualNames[WI.Recording.Type.OffscreenCanvasWebGL] = WI.RecordingAction._visualNames[WI.Recording.Type.CanvasWebGL];
WI.RecordingAction._visualNames[WI.Recording.Type.OffscreenCanvasWebGL2] = WI.RecordingAction._visualNames[WI.Recording.Type.CanvasWebGL2];

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
    [WI.Recording.Type.OffscreenCanvas2D]: {
        arc: ["currentX", "currentY"],
        arcTo: ["currentX", "currentY"],
        beginPath: ["currentX", "currentY"],
        bezierCurveTo: ["currentX", "currentY"],
        closePath: ["currentX", "currentY"],
        ellipse: ["currentX", "currentY"],
        lineTo: ["currentX", "currentY"],
        moveTo: ["currentX", "currentY"],
        quadraticCurveTo: ["currentX", "currentY"],
        rect: ["currentX", "currentY"],
        resetTransform: ["transform"],
        rotate: ["transform"],
        scale: ["transform"],
        setTransform: ["transform"],
        translate: ["transform"],
    },
};
