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

WebInspector.Canvas = class Canvas extends WebInspector.Object
{
    constructor(identifier, contextType, frame, cssCanvasName)
    {
        super();

        console.assert(identifier);
        console.assert(contextType);
        console.assert(frame instanceof WebInspector.Frame);

        this._identifier = identifier;
        this._contextType = contextType;
        this._frame = frame;
        this._cssCanvasName = cssCanvasName || "";
    }

    // Static

    static fromPayload(payload)
    {
        let contextType = null;
        switch (payload.contextType) {
        case CanvasAgent.ContextType.Canvas2D:
            contextType = WebInspector.Canvas.ContextType.Canvas2D;
            break;
        case CanvasAgent.ContextType.WebGL:
            contextType = WebInspector.Canvas.ContextType.WebGL;
            break;
        default:
            console.error("Invalid canvas context type", payload.contextType);
        }

        let frame = WebInspector.frameResourceManager.frameForIdentifier(payload.frameId);
        return new WebInspector.Canvas(payload.canvasId, contextType, frame, payload.cssCanvasName);
    }

    static displayNameForContextType(contextType)
    {
        switch (contextType) {
        case WebInspector.Canvas.ContextType.Canvas2D:
            return WebInspector.UIString("2D");
        case WebInspector.Canvas.ContextType.WebGL:
            return WebInspector.UIString("WebGL");
        default:
            console.error("Invalid canvas context type", contextType);
        }
    }

    static resetUniqueDisplayNameNumbers()
    {
        WebInspector.Canvas._nextUniqueDisplayNameNumber = 1;
    }

    // Public

    get identifier() { return this._identifier; }
    get contextType() { return this._contextType; }
    get frame() { return this._frame; }
    get cssCanvasName() { return this._cssCanvasName; }

    get displayName()
    {
        if (this.cssCanvasName) {
            console.assert(!this._node, "Unexpected DOM node for CSS canvas.");
            return WebInspector.UIString("CSS canvas “%s”").format(this._cssCanvasName);
        }

        // TODO:if the DOM node for the canvas is known and an id attribute value
        // exists, return the following: WebInspector.UIString("Canvas #%s").format(id);

        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = this.constructor._nextUniqueDisplayNameNumber++;
        return WebInspector.UIString("Canvas %d").format(this._uniqueDisplayNameNumber);
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.Canvas.FrameURLCookieKey] = this._frame.url.hash;
        if (this._cssCanvasName)
            cookie[WebInspector.Canvas.CSSCanvasNameCookieKey] = this._cssCanvasName;

        // TODO: if the canvas has an associated DOM node, and the node path to the cookie.
    }
};

WebInspector.Canvas._nextUniqueDisplayNameNumber = 1;

WebInspector.Canvas.FrameURLCookieKey = "canvas-frame-url";
WebInspector.Canvas.CSSCanvasNameCookieKey = "canvas-css-canvas-name";

WebInspector.Canvas.ContextType = {
    Canvas2D: Symbol("canvas-2d"),
    WebGL: Symbol("webgl"),
};
