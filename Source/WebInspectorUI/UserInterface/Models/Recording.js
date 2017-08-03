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

        this._actions = [new WI.RecordingInitialStateAction].concat(...this._frames.map((frame) => frame.actions));
        this._swizzle = [];
        this._source = null;

        for (let frame of this._frames) {
            for (let action of frame.actions) {
                action.swizzle(this);

                let prototype = null;
                if (this._type === WI.Recording.Type.Canvas2D)
                    prototype = CanvasRenderingContext2D.prototype;

                if (prototype) {
                    let validName = action.name in prototype;
                    let validFunction = !action.isFunction || typeof prototype[action.name] === "function";
                    if (!validName || !validFunction) {
                        action.valid = false;

                        WI.Recording.synthesizeError(WI.UIString("“%s” is invalid.").format(action.name));
                    }
                }
            }
        }
    }

    // Static

    static fromPayload(payload)
    {
        if (typeof payload !== "object" || payload === null)
            payload = {};

        if (isNaN(payload.version) || payload.version <= 0)
            return null;

        let type = null;
        switch (payload.type) {
        case RecordingAgent.Type.Canvas2D:
            type = WI.Recording.Type.Canvas2D;
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

        let frames = payload.frames.map(WI.RecordingFrame.fromPayload);
        return new WI.Recording(payload.version, type, payload.initialState, frames, payload.data);
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

    get type() { return this._type; }
    get initialState() { return this._initialState; }
    get frames() { return this._frames; }
    get data() { return this._data; }

    get actions() { return this._actions; }

    get source() { return this._source; }
    set source(source) { this._source = source; }

    swizzle(index, type)
    {
        if (typeof this._swizzle[index] !== "object")
            this._swizzle[index] = {};

        if (!(type in this._swizzle[index])) {
            try {
                let data = this._data[index];
                switch (type) {
                case WI.Recording.Swizzle.Array:
                    if (Array.isArray(data))
                        this._swizzle[index][type] = data;
                    break;
                case WI.Recording.Swizzle.CanvasStyle:
                    if (Array.isArray(data)) {
                        let context = document.createElement("canvas").getContext("2d");

                        let canvasStyle = this.swizzle(data[0], WI.Recording.Swizzle.String);
                        if (canvasStyle === "linear-gradient" || canvasStyle === "radial-gradient") {
                            this._swizzle[index][type] = canvasStyle === "radial-gradient" ? context.createRadialGradient(...data[1]) : context.createLinearGradient(...data[1]);
                            for (let stop of data[2]) {
                                let color = this.swizzle(stop[1], WI.Recording.Swizzle.String);
                                this._swizzle[index][type].addColorStop(stop[0], color);
                            }
                        } else if (canvasStyle === "pattern") {
                            let image = this.swizzle(data[1], WI.Recording.Swizzle.Image);
                            let repeat = this.swizzle(data[2], WI.Recording.Swizzle.String);
                            this._swizzle[index][type] = context.createPattern(image, repeat);
                        }
                    } else if (typeof data === "string")
                        this._swizzle[index][type] = data;
                    break;
                case WI.Recording.Swizzle.Element:
                    this._swizzle[index][type] = WI.Recording.Swizzle.Invalid;
                    break;
                case WI.Recording.Swizzle.Image:
                    this._swizzle[index][type] = new Image;
                    this._swizzle[index][type].src = data;
                    break;
                case WI.Recording.Swizzle.ImageData:
                    this._swizzle[index][type] = new ImageData(new Uint8ClampedArray(data[0]), parseInt(data[1]), parseInt(data[2]));
                    break;
                case WI.Recording.Swizzle.Path2D:
                    this._swizzle[index][type] = new Path2D(data);
                    break;
                case WI.Recording.Swizzle.String:
                    if (typeof data === "string")
                        this._swizzle[index][type] = data;
                    break;
                }
            } catch (e) {
                this._swizzle[index][type] = WI.Recording.Swizzle.Invalid;
            }

            if (!(type in this._swizzle[index]))
                this._swizzle[index][type] = WI.Recording.Swizzle.Invalid;
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

WI.Recording.Type = {
    Canvas2D: "canvas-2d",
};

WI.Recording.Swizzle = {
    Array: "Array",
    CanvasStyle: "CanvasStyle",
    Element: "Element",
    Image: "Image",
    ImageData: "ImageData",
    Path2D: "Path2D",
    String: "String",
    Invalid: Symbol("invalid"),
};
