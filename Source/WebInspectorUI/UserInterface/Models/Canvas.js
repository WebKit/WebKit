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
    constructor(identifier, contextType, frame, {domNode, cssCanvasName, contextAttributes, memoryCost} = {})
    {
        super();

        console.assert(identifier);
        console.assert(contextType);
        console.assert(frame instanceof WebInspector.Frame);

        this._identifier = identifier;
        this._contextType = contextType;
        this._frame = frame;
        this._domNode = domNode || null;
        this._cssCanvasName = cssCanvasName || "";
        this._contextAttributes = contextAttributes || {};
        this._memoryCost = memoryCost || NaN;

        this._cssCanvasClientNodes = null;
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
        case CanvasAgent.ContextType.WebGL2:
            contextType = WebInspector.Canvas.ContextType.WebGL2;
            break;
        case CanvasAgent.ContextType.WebGPU:
            contextType = WebInspector.Canvas.ContextType.WebGPU;
            break;
        default:
            console.error("Invalid canvas context type", payload.contextType);
        }

        let frame = WebInspector.frameResourceManager.frameForIdentifier(payload.frameId);
        return new WebInspector.Canvas(payload.canvasId, contextType, frame, {
            domNode: payload.nodeId ? WebInspector.domTreeManager.nodeForId(payload.nodeId) : null,
            cssCanvasName: payload.cssCanvasName,
            contextAttributes: payload.contextAttributes,
            memoryCost: payload.memoryCost,
        });
    }

    static displayNameForContextType(contextType)
    {
        switch (contextType) {
        case WebInspector.Canvas.ContextType.Canvas2D:
            return WebInspector.UIString("2D");
        case WebInspector.Canvas.ContextType.WebGL:
            return WebInspector.unlocalizedString("WebGL");
        case WebInspector.Canvas.ContextType.WebGL2:
            return WebInspector.unlocalizedString("WebGL2");
        case WebInspector.Canvas.ContextType.WebGPU:
            return WebInspector.unlocalizedString("WebGPU");
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
    get contextAttributes() { return this._contextAttributes; }

    get memoryCost()
    {
        return this._memoryCost;
    }

    set memoryCost(memoryCost)
    {
        if (memoryCost === this._memoryCost)
            return;

        this._memoryCost = memoryCost;

        this.dispatchEventToListeners(WebInspector.Canvas.Event.MemoryChanged);
    }

    get displayName()
    {
        if (this._cssCanvasName)
            return WebInspector.UIString("CSS canvas “%s”").format(this._cssCanvasName);

        if (this._domNode) {
            let idSelector = this._domNode.escapedIdSelector;
            if (idSelector)
                return WebInspector.UIString("Canvas %s").format(idSelector);
        }

        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = this.constructor._nextUniqueDisplayNameNumber++;
        return WebInspector.UIString("Canvas %d").format(this._uniqueDisplayNameNumber);
    }

    requestNode(callback)
    {
        if (this._domNode) {
            callback(this._domNode);
            return;
        }

        WebInspector.domTreeManager.ensureDocument();

        CanvasAgent.requestNode(this._identifier, (error, nodeId) => {
            if (error) {
                callback(null);
                return;
            }

            this._domNode = WebInspector.domTreeManager.nodeForId(nodeId);
            callback(this._domNode);
        });
    }

    requestContent(callback)
    {
        CanvasAgent.requestContent(this._identifier, (error, content) => {
            if (error) {
                callback(null);
                return;
            }

            callback(content);
        });
    }

    requestCSSCanvasClientNodes(callback)
    {
        if (!this._cssCanvasName) {
            callback([]);
            return;
        }

        if (this._cssCanvasClientNodes) {
            callback(this._cssCanvasClientNodes);
            return;
        }

        WebInspector.domTreeManager.ensureDocument();

        CanvasAgent.requestCSSCanvasClientNodes(this._identifier, (error, clientNodeIds) => {
            if (error) {
                callback([]);
                return;
            }

            clientNodeIds = Array.isArray(clientNodeIds) ? clientNodeIds : [];
            this._cssCanvasClientNodes = clientNodeIds.map((clientNodeId) => WebInspector.domTreeManager.nodeForId(clientNodeId));

            callback(this._cssCanvasClientNodes);
        });
    }

    saveIdentityToCookie(cookie)
    {
        cookie[WebInspector.Canvas.FrameURLCookieKey] = this._frame.url.hash;

        if (this._cssCanvasName)
            cookie[WebInspector.Canvas.CSSCanvasNameCookieKey] = this._cssCanvasName;
        else if (this._domNode)
            cookie[WebInspector.Canvas.NodePathCookieKey] = this._domNode.path;

    }

    cssCanvasClientNodesChanged()
    {
        // Called from WebInspector.CanvasManager.

        if (!this._cssCanvasName)
            return;

        this._cssCanvasClientNodes = null;

        this.dispatchEventToListeners(WebInspector.Canvas.Event.CSSCanvasClientNodesChanged);
    }
};

WebInspector.Canvas._nextUniqueDisplayNameNumber = 1;

WebInspector.Canvas.FrameURLCookieKey = "canvas-frame-url";
WebInspector.Canvas.CSSCanvasNameCookieKey = "canvas-css-canvas-name";

WebInspector.Canvas.ContextType = {
    Canvas2D: "canvas-2d",
    WebGL: "webgl",
    WebGL2: "webgl2",
    WebGPU: "webgpu",
};

WebInspector.Canvas.ResourceSidebarType = "resource-type-canvas";

WebInspector.Canvas.Event = {
    MemoryChanged: "canvas-memory-changed",
    CSSCanvasClientNodesChanged: "canvas-css-canvas-client-nodes-changed",
};
