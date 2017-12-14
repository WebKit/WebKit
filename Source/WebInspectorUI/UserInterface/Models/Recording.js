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

WI.Recording = class Recording
{
    constructor(version, type, initialState, frames, data)
    {
        this._version = version;
        this._type = type;
        this._initialState = initialState;
        this._frames = frames;
        this._data = data;
        this._displayName = WI.UIString("Recording");

        this._swizzle = [];
        this._visualActionIndexes = [];
        this._source = null;

        let actions = [new WI.RecordingInitialStateAction].concat(...this._frames.map((frame) => frame.actions));
        this._actions = Promise.all(actions.map((action) => action.swizzle(this))).then(() => {
            actions.forEach((action, index) => {
                if (!action.valid)
                    return;

                let prototype = null;
                if (this._type === WI.Recording.Type.Canvas2D)
                    prototype = CanvasRenderingContext2D.prototype;
                else if (this._type === WI.Recording.Type.CanvasWebGL)
                    prototype = WebGLRenderingContext.prototype;

                if (prototype) {
                    let validName = action.name in prototype;
                    let validFunction = !action.isFunction || typeof prototype[action.name] === "function";
                    if (!validName || !validFunction) {
                        action.markInvalid();

                        WI.Recording.synthesizeError(WI.UIString("“%s” is invalid.").format(this._name));
                    }
                }

                if (action.isVisual)
                    this._visualActionIndexes.push(index);
            });

            return actions;
        });
    }

    static fromPayload(payload, frames)
    {
        if (typeof payload !== "object" || payload === null)
            return null;

        if (isNaN(payload.version) || payload.version <= 0)
            return null;

        let type = null;
        switch (payload.type) {
        case RecordingAgent.Type.Canvas2D:
            type = WI.Recording.Type.Canvas2D;
            break;
        case RecordingAgent.Type.CanvasWebGL:
            type = WI.Recording.Type.CanvasWebGL;
            break;
        default:
            type = String(payload.type);
            break;
        }

        if (typeof payload.initialState !== "object" || payload.initialState === null)
            payload.initialState = {};
        if (typeof payload.initialState.attributes !== "object" || payload.initialState.attributes === null)
            payload.initialState.attributes = {};
        if (!Array.isArray(payload.initialState.parameters))
            payload.initialState.parameters = [];
        if (typeof payload.initialState.content !== "string")
            payload.initialState.content = "";

        if (!Array.isArray(payload.frames))
            payload.frames = [];

        if (!Array.isArray(payload.data))
            payload.data = [];

        if (!frames)
            frames = payload.frames.map(WI.RecordingFrame.fromPayload)

        return new WI.Recording(payload.version, type, payload.initialState, frames, payload.data);
    }

    static displayNameForSwizzleType(swizzleType)
    {
        switch (swizzleType) {
        case WI.Recording.Swizzle.None:
            return WI.unlocalizedString("None");
        case WI.Recording.Swizzle.Number:
            return WI.unlocalizedString("Number");
        case WI.Recording.Swizzle.Boolean:
            return WI.unlocalizedString("Boolean");
        case WI.Recording.Swizzle.String:
            return WI.unlocalizedString("String");
        case WI.Recording.Swizzle.Array:
            return WI.unlocalizedString("Array");
        case WI.Recording.Swizzle.TypedArray:
            return WI.unlocalizedString("TypedArray");
        case WI.Recording.Swizzle.Image:
            return WI.unlocalizedString("Image");
        case WI.Recording.Swizzle.ImageData:
            return WI.unlocalizedString("ImageData");
        case WI.Recording.Swizzle.DOMMatrix:
            return WI.unlocalizedString("DOMMatrix");
        case WI.Recording.Swizzle.Path2D:
            return WI.unlocalizedString("Path2D");
        case WI.Recording.Swizzle.CanvasGradient:
            return WI.unlocalizedString("CanvasGradient");
        case WI.Recording.Swizzle.CanvasPattern:
            return WI.unlocalizedString("CanvasPattern");
        case WI.Recording.Swizzle.WebGLBuffer:
            return WI.unlocalizedString("WebGLBuffer");
        case WI.Recording.Swizzle.WebGLFramebuffer:
            return WI.unlocalizedString("WebGLFramebuffer");
        case WI.Recording.Swizzle.WebGLRenderbuffer:
            return WI.unlocalizedString("WebGLRenderbuffer");
        case WI.Recording.Swizzle.WebGLTexture:
            return WI.unlocalizedString("WebGLTexture");
        case WI.Recording.Swizzle.WebGLShader:
            return WI.unlocalizedString("WebGLShader");
        case WI.Recording.Swizzle.WebGLProgram:
            return WI.unlocalizedString("WebGLProgram");
        case WI.Recording.Swizzle.WebGLUniformLocation:
            return WI.unlocalizedString("WebGLUniformLocation");
        case WI.Recording.Swizzle.ImageBitmap:
            return WI.unlocalizedString("ImageBitmap");
        default:
            console.error("Unknown swizzle type", swizzleType);
            return null;
        }
    }

    static synthesizeError(message)
    {
        const target = WI.mainTarget;
        const source = WI.ConsoleMessage.MessageSource.Other;
        const level = WI.ConsoleMessage.MessageLevel.Error;
        let consoleMessage = new WI.ConsoleMessage(target, source, level, WI.UIString("Recording error: %s").format(message));
        consoleMessage.shouldRevealConsole = true;

        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
    }

    // Public

    get displayName() { return this._displayName; }
    get type() { return this._type; }
    get initialState() { return this._initialState; }
    get frames() { return this._frames; }
    get data() { return this._data; }
    get visualActionIndexes() { return this._visualActionIndexes; }

    get actions() { return this._actions; }

    get source() { return this._source; }
    set source(source) { this._source = source; }

    createDisplayName(suggestedName)
    {
        let recordingNameSet;
        if (this._source) {
            recordingNameSet = this._source[WI.Recording.CanvasRecordingNamesSymbol];
            if (!recordingNameSet)
                this._source[WI.Recording.CanvasRecordingNamesSymbol] = recordingNameSet = new Set;
        } else
            recordingNameSet = WI.Recording._importedRecordingNameSet;

        let name;
        if (suggestedName) {
            name = suggestedName;
            let duplicateNumber = 2;
            while (recordingNameSet.has(name))
                name = `${suggestedName} (${duplicateNumber++})`;
        } else {
            let recordingNumber = 1;
            do {
                name = WI.UIString("Recording %d").format(recordingNumber++);
            } while (recordingNameSet.has(name));
        }

        recordingNameSet.add(name);
        this._displayName = name;
    }

    async swizzle(index, type)
    {
        if (typeof this._swizzle[index] !== "object")
            this._swizzle[index] = {};

        if (type === WI.Recording.Swizzle.Number)
            return parseFloat(index);

        if (type === WI.Recording.Swizzle.Boolean)
            return !!index;

        if (type === WI.Recording.Swizzle.Array)
            return Array.isArray(index) ? index : [];

        if (type === WI.Recording.Swizzle.DOMMatrix)
            return new DOMMatrix(index);

        // FIXME: <https://webkit.org/b/176009> Web Inspector: send data for WebGL objects during a recording instead of a placeholder string
        if (type === WI.Recording.Swizzle.TypedArray
            || type === WI.Recording.Swizzle.WebGLBuffer
            || type === WI.Recording.Swizzle.WebGLFramebuffer
            || type === WI.Recording.Swizzle.WebGLRenderbuffer
            || type === WI.Recording.Swizzle.WebGLTexture
            || type === WI.Recording.Swizzle.WebGLShader
            || type === WI.Recording.Swizzle.WebGLProgram
            || type === WI.Recording.Swizzle.WebGLUniformLocation) {
            return index;
        }

        if (!(type in this._swizzle[index])) {
            try {
                let data = this._data[index];
                switch (type) {
                case WI.Recording.Swizzle.None:
                    this._swizzle[index][type] = data;
                    break;

                case WI.Recording.Swizzle.String:
                    this._swizzle[index][type] = String(data);
                    break;

                case WI.Recording.Swizzle.Image:
                    this._swizzle[index][type] = await WI.ImageUtilities.promisifyLoad(data);
                    break;

                case WI.Recording.Swizzle.ImageData:
                    this._swizzle[index][type] = new ImageData(new Uint8ClampedArray(data[0]), parseInt(data[1]), parseInt(data[2]));
                    break;

                case WI.Recording.Swizzle.Path2D:
                    this._swizzle[index][type] = new Path2D(data);
                    break;

                case WI.Recording.Swizzle.CanvasGradient:
                    var gradientType = await this.swizzle(data[0], WI.Recording.Swizzle.String);

                    WI.ImageUtilities.scratchCanvasContext2D((context) => {
                        this._swizzle[index][type] = gradientType === "radial-gradient" ? context.createRadialGradient(...data[1]) : context.createLinearGradient(...data[1]);
                    });

                    for (let stop of data[2]) {
                        let color = await this.swizzle(stop[1], WI.Recording.Swizzle.String);
                        this._swizzle[index][type].addColorStop(stop[0], color);
                    }
                    break;

                case WI.Recording.Swizzle.CanvasPattern:
                    var [image, repeat] = await Promise.all([
                        this.swizzle(data[0], WI.Recording.Swizzle.Image),
                        this.swizzle(data[1], WI.Recording.Swizzle.String),
                    ]);

                    WI.ImageUtilities.scratchCanvasContext2D((context) => {
                        this._swizzle[index][type] = context.createPattern(image, repeat);
                        this._swizzle[index][type].__image = image;
                    });
                    break;

                case WI.Recording.Swizzle.ImageBitmap:
                    var image = await this.swizzle(index, WI.Recording.Swizzle.Image);
                    this._swizzle[index][type] = await createImageBitmap(image);
                    break;
                }
            } catch { }
        }

        return this._swizzle[index][type];
    }

    toJSON()
    {
        let initialState = {};
        if (!isEmptyObject(this._initialState.attributes))
            initialState.attributes = this._initialState.attributes;
        if (this._initialState.parameters.length)
            initialState.parameters = this._initialState.parameters;
        if (this._initialState.content && this._initialState.content.length)
            initialState.content = this._initialState.content;

        return {
            version: this._version,
            type: this._type,
            initialState,
            frames: this._frames.map((frame) => frame.toJSON()),
            data: this._data,
        };
    }
};

WI.Recording._importedRecordingNameSet = new Set;

WI.Recording.CanvasRecordingNamesSymbol = Symbol("canvas-recording-names");

WI.Recording.Type = {
    Canvas2D: "canvas-2d",
    CanvasWebGL: "canvas-webgl",
};

// Keep this in sync with WebCore::RecordingSwizzleTypes.
WI.Recording.Swizzle = {
    None: 0,
    Number: 1,
    Boolean: 2,
    String: 3,
    Array: 4,
    TypedArray: 5,
    Image: 6,
    ImageData: 7,
    DOMMatrix: 8,
    Path2D: 9,
    CanvasGradient: 10,
    CanvasPattern: 11,
    WebGLBuffer: 12,
    WebGLFramebuffer: 13,
    WebGLRenderbuffer: 14,
    WebGLTexture: 15,
    WebGLShader: 16,
    WebGLProgram: 17,
    WebGLUniformLocation: 18,
    ImageBitmap: 19,
};
