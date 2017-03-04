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

WebInspector.WebSocketResource = class WebSocketResource extends WebInspector.Resource
{
    constructor(url, loaderIdentifier, targetId, requestIdentifier, requestHeaders, requestData, requestSentTimestamp, initiatorSourceCodeLocation)
    {
        const type = WebInspector.Resource.Type.WebSocket;
        const mimeType = null;
        const requestMethod = "GET";
        super(url, mimeType, type, loaderIdentifier, targetId, requestIdentifier, requestMethod, requestHeaders, requestData, requestSentTimestamp, initiatorSourceCodeLocation);

        this._readyState = WebInspector.WebSocketResource.ReadyState.Connecting;
        this._frames = [];
    }

    // Public

    get frames() { return this._frames; }

    get readyState()
    {
        return this._readyState;
    }

    set readyState(state)
    {
        if (state === this._readyState)
            return;

        let previousState = this._readyState;
        this._readyState = state;

        this.dispatchEventToListeners(WebInspector.WebSocketResource.Event.ReadyStateChanged, {previousState, state});
    }

    addFrame(data, isIncoming, opcode, timestamp, elapsedTime)
    {
        let frame = {data, isIncoming, opcode, timestamp, elapsedTime};
        this._frames.push(frame);

        this.dispatchEventToListeners(WebInspector.WebSocketResource.Event.FrameAdded, frame);
    }
};

WebInspector.WebSocketResource.Event = {
    FrameAdded: Symbol("web-socket-frame-added"),
    ReadyStateChanged: Symbol("web-socket-resource-ready-state-changed"),
};

WebInspector.WebSocketResource.ReadyState = {
    Closed: Symbol("web-socket-ready-state-closed"),
    Connecting: Symbol("web-socket-ready-state-connecting"),
    Open: Symbol("web-socket-ready-state-open"),
};
