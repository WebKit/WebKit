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

WebInspector.Recording = class Recording
{
    constructor(version, type, initialState, frames, data)
    {
        this._version = version;
        this._type = type;
        this._initialState = initialState;
        this._frames = frames;
        this._data = data;
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
            type = WebInspector.Recording.Type.Canvas2D;
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

        let frames = payload.frames.map(WebInspector.RecordingFrame.fromPayload);
        return new WebInspector.Recording(payload.version, type, payload.initialState, frames, payload.data);
    }

    // Public

    get type() { return this._type; }
    get initialState() { return this._initialState; }
    get frames() { return this._frames; }
    get data() { return this._data; }

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

WebInspector.Recording.Type = {
    Canvas2D: "canvas-2d",
};
