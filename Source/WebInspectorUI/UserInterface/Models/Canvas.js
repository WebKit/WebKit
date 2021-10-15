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

WI.Canvas = class Canvas extends WI.Object
{
    constructor(identifier, contextType, {domNode, cssCanvasName, contextAttributes, memoryCost, backtrace} = {})
    {
        super();

        console.assert(identifier);
        console.assert(contextType);

        this._identifier = identifier;
        this._contextType = contextType;
        this._domNode = domNode || null;
        this._cssCanvasName = cssCanvasName || "";
        this._contextAttributes = contextAttributes || {};
        this._extensions = new Set;
        this._memoryCost = memoryCost || NaN;
        this._backtrace = backtrace || [];

        this._clientNodes = null;
        this._shaderProgramCollection = new WI.ShaderProgramCollection;
        this._recordingCollection = new WI.RecordingCollection;

        this._nextShaderProgramDisplayNumber = null;

        this._requestNodePromise = null;

        this._recordingState = WI.Canvas.RecordingState.Inactive;
        this._recordingFrames = [];
        this._recordingBufferUsed = 0;
    }

    // Static

    static fromPayload(payload)
    {
        let contextType = null;
        switch (payload.contextType) {
        case InspectorBackend.Enum.Canvas.ContextType.Canvas2D:
            contextType = WI.Canvas.ContextType.Canvas2D;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.BitmapRenderer:
            contextType = WI.Canvas.ContextType.BitmapRenderer;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGL:
            contextType = WI.Canvas.ContextType.WebGL;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGL2:
            contextType = WI.Canvas.ContextType.WebGL2;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGPU:
            contextType = WI.Canvas.ContextType.WebGPU;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebMetal:
            contextType = WI.Canvas.ContextType.WebMetal;
            break;
        default:
            console.error("Invalid canvas context type", payload.contextType);
        }

        return new WI.Canvas(payload.canvasId, contextType, {
            domNode: payload.nodeId ? WI.domManager.nodeForId(payload.nodeId) : null,
            cssCanvasName: payload.cssCanvasName,
            contextAttributes: payload.contextAttributes,
            memoryCost: payload.memoryCost,
            backtrace: Array.isArray(payload.backtrace) ? payload.backtrace.map((item) => WI.CallFrame.fromPayload(WI.mainTarget, item)) : [],
        });
    }

    static displayNameForContextType(contextType)
    {
        switch (contextType) {
        case WI.Canvas.ContextType.Canvas2D:
            return WI.UIString("2D");
        case WI.Canvas.ContextType.BitmapRenderer:
            return WI.UIString("Bitmap Renderer", "Canvas Context Type Bitmap Renderer", "Bitmap Renderer is a type of rendering context associated with a <canvas> element");
        case WI.Canvas.ContextType.WebGL:
            return WI.unlocalizedString("WebGL");
        case WI.Canvas.ContextType.WebGL2:
            return WI.unlocalizedString("WebGL2");
        case WI.Canvas.ContextType.WebGPU:
            return WI.unlocalizedString("Web GPU");
        case WI.Canvas.ContextType.WebMetal:
            return WI.unlocalizedString("WebMetal");
        }

        console.assert(false, "Unknown canvas context type", contextType);
        return null;
    }

    static resetUniqueDisplayNameNumbers()
    {
        Canvas._nextContextUniqueDisplayNameNumber = 1;
        Canvas._nextDeviceUniqueDisplayNameNumber = 1;
    }

    static supportsRequestContentForContextType(contextType)
    {
        switch (contextType) {
        case Canvas.ContextType.WebGPU:
        case Canvas.ContextType.WebMetal:
            return false;
        }
        return true;
    }

    // Public

    get identifier() { return this._identifier; }
    get contextType() { return this._contextType; }
    get cssCanvasName() { return this._cssCanvasName; }
    get contextAttributes() { return this._contextAttributes; }
    get extensions() { return this._extensions; }
    get backtrace() { return this._backtrace; }
    get shaderProgramCollection() { return this._shaderProgramCollection; }
    get recordingCollection() { return this._recordingCollection; }
    get recordingFrameCount() { return this._recordingFrames.length; }
    get recordingBufferUsed() { return this._recordingBufferUsed; }

    get recordingActive()
    {
        return this._recordingState !== WI.Canvas.RecordingState.Inactive;
    }

    get memoryCost()
    {
        return this._memoryCost;
    }

    set memoryCost(memoryCost)
    {
        if (memoryCost === this._memoryCost)
            return;

        this._memoryCost = memoryCost;

        this.dispatchEventToListeners(WI.Canvas.Event.MemoryChanged);
    }

    get displayName()
    {
        if (this._cssCanvasName)
            return WI.UIString("CSS canvas \u201C%s\u201D").format(this._cssCanvasName);

        if (this._domNode) {
            let idSelector = this._domNode.escapedIdSelector;
            if (idSelector)
                return WI.UIString("Canvas %s").format(idSelector);
        }

        if (this._contextType === Canvas.ContextType.WebGPU) {
            if (!this._uniqueDisplayNameNumber)
                this._uniqueDisplayNameNumber = Canvas._nextDeviceUniqueDisplayNameNumber++;
            return WI.UIString("Device %d").format(this._uniqueDisplayNameNumber);
        }

        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = Canvas._nextContextUniqueDisplayNameNumber++;
        return WI.UIString("Canvas %d").format(this._uniqueDisplayNameNumber);
    }

    requestNode()
    {
        if (!this._requestNodePromise) {
            this._requestNodePromise = new Promise((resolve, reject) => {
                WI.domManager.ensureDocument();

                let target = WI.assumingMainTarget();
                target.CanvasAgent.requestNode(this._identifier, (error, nodeId) => {
                    if (error) {
                        resolve(null);
                        return;
                    }

                    this._domNode = WI.domManager.nodeForId(nodeId);
                    if (!this._domNode) {
                        resolve(null);
                        return;
                    }

                    resolve(this._domNode);
                });
            });
        }
        return this._requestNodePromise;
    }

    requestContent()
    {
        if (!Canvas.supportsRequestContentForContextType(this._contextType))
            return Promise.resolve(null);

        let target = WI.assumingMainTarget();
        return target.CanvasAgent.requestContent(this._identifier).then((result) => result.content).catch((error) => console.error(error));
    }

    requestClientNodes(callback)
    {
        if (this._clientNodes) {
            callback(this._clientNodes);
            return;
        }

        WI.domManager.ensureDocument();

        let wrappedCallback = (error, clientNodeIds) => {
            if (error) {
                callback([]);
                return;
            }

            clientNodeIds = Array.isArray(clientNodeIds) ? clientNodeIds : [];
            this._clientNodes = clientNodeIds.map((clientNodeId) => WI.domManager.nodeForId(clientNodeId));
            callback(this._clientNodes);
        };

        let target = WI.assumingMainTarget();

        // COMPATIBILITY (iOS 13): Canvas.requestCSSCanvasClientNodes was renamed to Canvas.requestClientNodes.
        if (!target.hasCommand("Canvas.requestClientNodes")) {
            target.CanvasAgent.requestCSSCanvasClientNodes(this._identifier, wrappedCallback);
            return;
        }

        target.CanvasAgent.requestClientNodes(this._identifier, wrappedCallback);
    }

    requestSize()
    {
        function calculateSize(domNode) {
            function getAttributeValue(name) {
                let value = Number(domNode.getAttribute(name));
                if (!Number.isInteger(value) || value < 0)
                    return NaN;
                return value;
            }

            return {
                width: getAttributeValue("width"),
                height: getAttributeValue("height")
            };
        }

        function getPropertyValue(remoteObject, name) {
            return new Promise((resolve, reject) => {
                remoteObject.getProperty(name, (error, result) => {
                    if (error) {
                        reject(error);
                        return;
                    }
                    resolve(result);
                });
            });
        }

        return this.requestNode().then((domNode) => {
            if (!domNode)
                return null;

            let size = calculateSize(domNode);
            if (!isNaN(size.width) && !isNaN(size.height))
                return size;

            // Since the "width" and "height" properties of canvas elements are more than just
            // attributes, we need to invoke the getter for each to get the actual value.
            //  - https://html.spec.whatwg.org/multipage/canvas.html#attr-canvas-width
            //  - https://html.spec.whatwg.org/multipage/canvas.html#attr-canvas-height
            let remoteObject = null;
            return WI.RemoteObject.resolveNode(domNode).then((object) => {
                remoteObject = object;
                return Promise.all([getPropertyValue(object, "width"), getPropertyValue(object, "height")]);
            }).then((values) => {
                let width = values[0].value;
                let height = values[1].value;
                values[0].release();
                values[1].release();
                remoteObject.release();
                return {width, height};
            });
        });
    }

    startRecording(singleFrame)
    {
        let target = WI.assumingMainTarget();

        let handleStartRecording = (error) => {
            if (error) {
                console.error(error);
                return;
            }

            this._recordingState = WI.Canvas.RecordingState.ActiveFrontend;

            // COMPATIBILITY (iOS 12.1): Canvas.recordingStarted did not exist yet
            if (target.hasEvent("Canvas.recordingStarted"))
                return;

            this._recordingFrames = [];
            this._recordingBufferUsed = 0;

            this.dispatchEventToListeners(WI.Canvas.Event.RecordingStarted);
        };

        // COMPATIBILITY (iOS 12.1): `frameCount` did not exist yet.
        if (target.hasCommand("Canvas.startRecording", "singleFrame")) {
            target.CanvasAgent.startRecording(this._identifier, singleFrame, handleStartRecording);
            return;
        }

        if (singleFrame) {
            const frameCount = 1;
            target.CanvasAgent.startRecording(this._identifier, frameCount, handleStartRecording);
        } else
            target.CanvasAgent.startRecording(this._identifier, handleStartRecording);
    }

    stopRecording()
    {
        let target = WI.assumingMainTarget();
        target.CanvasAgent.stopRecording(this._identifier);
    }

    saveIdentityToCookie(cookie)
    {
        if (this._cssCanvasName)
            cookie[WI.Canvas.CSSCanvasNameCookieKey] = this._cssCanvasName;
        else if (this._domNode)
            cookie[WI.Canvas.NodePathCookieKey] = this._domNode.path;

    }

    enableExtension(extension)
    {
        // Called from WI.CanvasManager.

        this._extensions.add(extension);

        this.dispatchEventToListeners(WI.Canvas.Event.ExtensionEnabled, {extension});
    }

    clientNodesChanged()
    {
        // Called from WI.CanvasManager.

        this._clientNodes = null;

        this.dispatchEventToListeners(Canvas.Event.ClientNodesChanged);
    }

    recordingStarted(initiator)
    {
        // Called from WI.CanvasManager.

        if (initiator === InspectorBackend.Enum.Recording.Initiator.Console)
            this._recordingState = WI.Canvas.RecordingState.ActiveConsole;
        else if (initiator === InspectorBackend.Enum.Recording.Initiator.AutoCapture)
            this._recordingState = WI.Canvas.RecordingState.ActiveAutoCapture;
        else {
            console.assert(initiator === InspectorBackend.Enum.Recording.Initiator.Frontend);
            this._recordingState = WI.Canvas.RecordingState.ActiveFrontend;
        }

        this._recordingFrames = [];
        this._recordingBufferUsed = 0;

        this.dispatchEventToListeners(WI.Canvas.Event.RecordingStarted);
    }

    recordingProgress(framesPayload, bufferUsed)
    {
        // Called from WI.CanvasManager.

        this._recordingFrames.pushAll(framesPayload.map(WI.RecordingFrame.fromPayload));

        this._recordingBufferUsed = bufferUsed;

        this.dispatchEventToListeners(WI.Canvas.Event.RecordingProgress);
    }

    recordingFinished(recordingPayload)
    {
        // Called from WI.CanvasManager.

        let initiatedByUser = this._recordingState === WI.Canvas.RecordingState.ActiveFrontend;

        // COMPATIBILITY (iOS 12.1): Canvas.recordingStarted did not exist yet
        if (!initiatedByUser && !InspectorBackend.hasEvent("Canvas.recordingStarted"))
            initiatedByUser = !!this.recordingActive;

        let recording = recordingPayload ? WI.Recording.fromPayload(recordingPayload, this._recordingFrames) : null;
        if (recording) {
            recording.source = this;
            recording.createDisplayName(recordingPayload.name);

            this._recordingCollection.add(recording);
        }

        this._recordingState = WI.Canvas.RecordingState.Inactive;
        this._recordingFrames = [];
        this._recordingBufferUsed = 0;

        this.dispatchEventToListeners(WI.Canvas.Event.RecordingStopped, {recording, initiatedByUser});
    }

    nextShaderProgramDisplayNumberForProgramType(programType)
    {
        // Called from WI.ShaderProgram.

        if (!this._nextShaderProgramDisplayNumber)
            this._nextShaderProgramDisplayNumber = {};

        this._nextShaderProgramDisplayNumber[programType] = (this._nextShaderProgramDisplayNumber[programType] || 0) + 1;
        return this._nextShaderProgramDisplayNumber[programType];
    }
};

WI.Canvas._nextContextUniqueDisplayNameNumber = 1;
WI.Canvas._nextDeviceUniqueDisplayNameNumber = 1;

WI.Canvas.FrameURLCookieKey = "canvas-frame-url";
WI.Canvas.CSSCanvasNameCookieKey = "canvas-css-canvas-name";

WI.Canvas.ContextType = {
    Canvas2D: "canvas-2d",
    BitmapRenderer: "bitmaprenderer",
    WebGL: "webgl",
    WebGL2: "webgl2",
    WebGPU: "webgpu",
    WebMetal: "webmetal",
};

WI.Canvas.RecordingState = {
    Inactive: "canvas-recording-state-inactive",
    ActiveFrontend: "canvas-recording-state-active-frontend",
    ActiveConsole: "canvas-recording-state-active-console",
    ActiveAutoCapture: "canvas-recording-state-active-auto-capture",
};

WI.Canvas.Event = {
    MemoryChanged: "canvas-memory-changed",
    ExtensionEnabled: "canvas-extension-enabled",
    ClientNodesChanged: "canvas-client-nodes-changed",
    RecordingStarted: "canvas-recording-started",
    RecordingProgress: "canvas-recording-progress",
    RecordingStopped: "canvas-recording-stopped",
};
