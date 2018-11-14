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

WI.Recording = class Recording extends WI.Object
{
    constructor(version, type, initialState, frames, data)
    {
        super();

        this._version = version;
        this._type = type;
        this._initialState = initialState;
        this._frames = frames;
        this._data = data;
        this._displayName = WI.UIString("Recording");

        this._swizzle = [];
        this._actions = [new WI.RecordingInitialStateAction].concat(...this._frames.map((frame) => frame.actions));
        this._visualActionIndexes = [];
        this._source = null;

        this._processContext = null;
        this._processStates = [];
        this._processing = false;
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
        case RecordingAgent.Type.CanvasBitmapRenderer:
            type = WI.Recording.Type.CanvasBitmapRenderer;
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
        if (!Array.isArray(payload.initialState.states) || payload.initialState.states.some((item) => typeof item !== "object" || item === null)) {
            payload.initialState.states = [];

            // COMPATIBILITY (iOS 12.0): Recording.InitialState.states did not exist yet
            if (!isEmptyObject(payload.initialState.attributes)) {
                let {width, height, ...state} = payload.initialState.attributes;
                if (!isEmptyObject(state))
                    payload.initialState.states.push(state);
            }
        }
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
    get actions() { return this._actions; }
    get visualActionIndexes() { return this._visualActionIndexes; }

    get source() { return this._source; }
    set source(source) { this._source = source; }

    get processing() { return this._processing; }

    get ready()
    {
        return this._actions.lastValue.ready;
    }

    startProcessing()
    {
        console.assert(!this._processing, "Cannot start an already started process().");
        console.assert(!this.ready, "Cannot start a completed process().");
        if (this._processing || this.ready)
            return;

        this._processing = true;

        this._process();
    }

    stopProcessing()
    {
        console.assert(this._processing, "Cannot stop an already stopped process().");
        console.assert(!this.ready, "Cannot stop a completed process().");
        if (!this._processing || this.ready)
            return;

        this._processing = false;
    }

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

                case WI.Recording.Swizzle.CallStack: {
                    let array = await this.swizzle(data, WI.Recording.Swizzle.Array);
                    this._swizzle[index][type] = await Promise.all(array.map((item) => this.swizzle(item, WI.Recording.Swizzle.CallFrame)));
                    break;
                }

                case WI.Recording.Swizzle.CallFrame: {
                    let array = await this.swizzle(data, WI.Recording.Swizzle.Array);
                    let [functionName, url] = await Promise.all([
                        this.swizzle(array[0], WI.Recording.Swizzle.String),
                        this.swizzle(array[1], WI.Recording.Swizzle.String),
                    ]);
                    this._swizzle[index][type] = WI.CallFrame.fromPayload(WI.assumingMainTarget(), {
                        functionName,
                        url,
                        lineNumber: array[2],
                        columnNumber: array[3],
                    });
                    break;
                }
                }
            } catch { }
        }

        return this._swizzle[index][type];
    }

    createContext()
    {
        let createCanvasContext = (type) => {
            let canvas = document.createElement("canvas");
            if ("width" in this._initialState.attributes)
                canvas.width = this._initialState.attributes.width;
            if ("height" in this._initialState.attributes)
                canvas.height = this._initialState.attributes.height;
            return canvas.getContext(type, ...this._initialState.parameters);
        };

        if (this._type === WI.Recording.Type.Canvas2D)
            return createCanvasContext("2d");

        if (this._type === WI.Recording.Type.BitmapRenderer)
            return createCanvasContext("bitmaprenderer");

        if (this._type === WI.Recording.Type.CanvasWebGL)
            return createCanvasContext("webgl");

        console.error("Unknown recording type", this._type);
        return null;
    }

    toJSON()
    {
        let initialState = {};
        if (!isEmptyObject(this._initialState.attributes))
            initialState.attributes = this._initialState.attributes;
        if (this._initialState.states.length)
            initialState.states = this._initialState.states;
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

    // Private

    async _process()
    {
        if (!this._processContext) {
            this._processContext = this.createContext();

            if (this._type === WI.Recording.Type.Canvas2D) {
                let initialContent = await WI.ImageUtilities.promisifyLoad(this._initialState.content);
                this._processContext.drawImage(initialContent, 0, 0);

                for (let initialState of this._initialState.states) {
                    let state = await WI.RecordingState.swizzleInitialState(this, initialState);
                    state.apply(this._type, this._processContext);

                    // The last state represents the current state, which should not be saved.
                    if (initialState !== this._initialState.states.lastValue) {
                        this._processContext.save();
                        this._processStates.push(WI.RecordingState.fromContext(this._type, this._processContext));
                    }
                }
            }
        }

        // The first action is always a WI.RecordingInitialStateAction, which doesn't need to swizzle().
        // Since it is not associated with a WI.RecordingFrame, it has to manually process().
        if (!this._actions[0].ready) {
            this._actions[0].process(this, this._processContext, this._processStates);
            this.dispatchEventToListeners(WI.Recording.Event.ProcessedAction, {action: this._actions[0], index: 0});
        }

        const workInterval = 10;
        let startTime = Date.now();

        let cumulativeActionIndex = 0;
        let lastAction = this._actions[cumulativeActionIndex];
        for (let frameIndex = 0; frameIndex < this._frames.length; ++frameIndex) {
            let frame = this._frames[frameIndex];

            if (frame.actions.lastValue.ready) {
                cumulativeActionIndex += frame.actions.length;
                lastAction = frame.actions.lastValue;
                continue;
            }

            for (let actionIndex = 0; actionIndex < frame.actions.length; ++actionIndex) {
                ++cumulativeActionIndex;

                let action = frame.actions[actionIndex];
                if (action.ready) {
                    lastAction = action;
                    continue;
                }

                await action.swizzle(this);

                action.process(this, this._processContext, this._processStates, {lastAction});

                if (action.isVisual)
                    this._visualActionIndexes.push(cumulativeActionIndex);

                if (!actionIndex)
                    this.dispatchEventToListeners(WI.Recording.Event.StartProcessingFrame, {frame, index: frameIndex});

                this.dispatchEventToListeners(WI.Recording.Event.ProcessedAction, {action, index: cumulativeActionIndex});

                if (Date.now() - startTime > workInterval) {
                    await Promise.delay(); // yield

                    startTime = Date.now();
                }

                lastAction = action;

                if (!this._processing)
                    return;
            }

            if (!this._processing)
                return;
        }

        this._processContext = null;
        this._processing = false;
    }
};

WI.Recording.Event = {
    ProcessedAction: "recording-processed-action",
    StartProcessingFrame: "recording-start-processing-frame",
};

WI.Recording._importedRecordingNameSet = new Set;

WI.Recording.CanvasRecordingNamesSymbol = Symbol("canvas-recording-names");

WI.Recording.Type = {
    Canvas2D: "canvas-2d",
    CanvasBitmapRenderer: "canvas-bitmaprenderer",
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

    // Special frontend-only swizzle types.
    CallStack: Symbol("CallStack"),
    CallFrame: Symbol("CallFrame"),
};
