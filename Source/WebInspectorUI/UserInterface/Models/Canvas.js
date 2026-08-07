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
    constructor(target, identifier, contextType, sizes, {cssCanvasNames, contextAttributes, features, memoryCost, stackTrace, displayName} = {})
    {
        super();

        console.assert(target instanceof WI.Target, target);
        console.assert(identifier);
        console.assert(contextType);
        console.assert(Array.isArray(sizes) && sizes.every((size) => size instanceof WI.Size), sizes);
        console.assert(!stackTrace || stackTrace instanceof WI.StackTrace, stackTrace);
        console.assert(!displayName || typeof displayName === "string", displayName);

        this._target = target;
        this._identifier = identifier;
        this._contextType = contextType;
        this._sizes = sizes;
        this._domNodes = [];
        this._cssCanvasNames = cssCanvasNames || [];
        this._contextAttributes = contextAttributes || {};
        this._features = features || [];
        this._extensions = new Set;
        this._memoryCost = memoryCost || NaN;
        this._stackTrace = stackTrace || null;
        this._displayName = displayName || "";

        this._shaderProgramCollection = new WI.ShaderProgramCollection;
        this._recordingCollection = new WI.RecordingCollection;

        this._nextShaderProgramDisplayNumber = null;

        this._requestNodesPromise = null;

        this._cssCanvasClientNodes = [];
        this._requestCSSCanvasClientNodesPromise = null;

        this._recordingState = WI.Canvas.RecordingState.Inactive;
        this._recordingFrames = [];
        this._recordingBufferUsed = 0;

        // COMPATIBILITY (macOS 14.0, iOS 17.0): `Canvas.canvasSizeChanged` did not exist yet.
        if (!InspectorBackend.hasEvent("Canvas.canvasSizeChanged")) {
            console.assert(!sizes.length);

            this.requestNodes().then((nodes) => {
                for (let node of nodes) {
                    node.addEventListener(WI.DOMNode.Event.AttributeModified, this._calculateSize, this);
                    node.addEventListener(WI.DOMNode.Event.AttributeRemoved, this._calculateSize, this);
                }
            });
            this._calculateSize();
        }
    }

    // Static

    static fromPayload(target, payload)
    {
        let contextType = null;
        switch (payload.contextType) {
        case InspectorBackend.Enum.Canvas.ContextType.Canvas2D:
            contextType = WI.Canvas.ContextType.Canvas2D;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.OffscreenCanvas2D:
            contextType = WI.Canvas.ContextType.OffscreenCanvas2D;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.BitmapRenderer:
            contextType = WI.Canvas.ContextType.BitmapRenderer;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.OffscreenBitmapRenderer:
            contextType = WI.Canvas.ContextType.OffscreenBitmapRenderer;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGL:
            contextType = WI.Canvas.ContextType.WebGL;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.OffscreenWebGL:
            contextType = WI.Canvas.ContextType.OffscreenWebGL;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGL2:
            contextType = WI.Canvas.ContextType.WebGL2;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.OffscreenWebGL2:
            contextType = WI.Canvas.ContextType.OffscreenWebGL2;
            break;
        case InspectorBackend.Enum.Canvas.ContextType.WebGPU:
            contextType = WI.Canvas.ContextType.WebGPU;
            break;
        default:
            console.error("Invalid canvas context type", payload.contextType);
        }

        let sizes = payload.sizes?.map(WI.Size.fromJSON) || [];

        // COMPATIBILITY (macOS X.Y, iOS X.Y): `width` and `height` were replaced by `sizes`.
        if (!payload.sizes && "width" in payload && "height" in payload)
            sizes.push(new WI.Size(payload.width, payload.height));

        // COMPATIBILITY (macOS X.Y, iOS X.Y): `cssCanvasName` was renamed to `cssCanvasNames`.
        let cssCanvasNames = payload.cssCanvasNames || (payload.cssCanvasName ? [payload.cssCanvasName] : []);

        // COMPATIBILITY (macOS 13.0, iOS 16.0): `backtrace` was renamed to `stackTrace`.
        if (payload.backtrace)
            payload.stackTrace = {callFrames: payload.backtrace};

        return new WI.Canvas(target, payload.canvasId, contextType, sizes, {
            cssCanvasNames,
            contextAttributes: payload.contextAttributes,
            features: payload.features,
            memoryCost: payload.memoryCost,
            stackTrace: WI.StackTrace.fromPayload(target, payload.stackTrace),
            displayName: payload.name,
        });
    }

    static _domNodeForId(target, nodeId)
    {
        if (target instanceof WI.FrameTarget)
            return WI.domManager.nodeForIdInFrameTarget(nodeId, target);
        return WI.domManager.nodeForId(nodeId);
    }

    static displayNameForContextType(contextType)
    {
        switch (contextType) {
        case WI.Canvas.ContextType.Canvas2D:
            return WI.UIString("2D", "2D @ Canvas Context Type", "2D is a type of rendering context associated with a <canvas> element.");
        case WI.Canvas.ContextType.OffscreenCanvas2D:
            return WI.UIString("Offscreen2D", "2D @ Offscreen Canvas Context Type", "2D is a type of rendering context associated with a OffscreenCanvas.");
        case WI.Canvas.ContextType.BitmapRenderer:
            return WI.UIString("Bitmap Renderer", "Bitmap Renderer @ Canvas Context Type", "Bitmap Renderer is a type of rendering context associated with a <canvas> element.");
        case WI.Canvas.ContextType.OffscreenBitmapRenderer:
            return WI.UIString("Bitmap Renderer (Offscreen)", "Bitmap Renderer @ Offscreen Canvas Context Type", "Bitmap Renderer is a type of rendering context associated with a OffscreenCanvas.");
        case WI.Canvas.ContextType.WebGL:
            return WI.UIString("WebGL", "WebGL @ Canvas Context Type", "WebGL is a type of rendering context associated with a <canvas> element.");
        case WI.Canvas.ContextType.OffscreenWebGL:
            return WI.UIString("WebGL (Offscreen)", "WebGL @ Offscreen Canvas Context Type", "WebGL is a type of rendering context associated with a OffscreenCanvas.");
        case WI.Canvas.ContextType.WebGL2:
            return WI.UIString("WebGL2", "WebGL2 @ Canvas Context Type", "WebGL2 is a type of rendering context associated with a <canvas> element.");
        case WI.Canvas.ContextType.OffscreenWebGL2:
            return WI.UIString("WebGL2 (Offscreen)", "WebGL2 @ Offscreen Canvas Context Type", "WebGL2 is a type of rendering context associated with a OffscreenCanvas.");
        case WI.Canvas.ContextType.WebGPU:
            return WI.UIString("WebGPU", "WebGPU @ Canvas Context Type", "WebGPU is a type of rendering context associated with a <canvas> element.");
        }

        console.assert(false, "Unknown canvas context type", contextType);
        return null;
    }

    static displayNameForColorSpace(colorSpace)
    {
        switch(colorSpace) {
        case WI.Canvas.ColorSpace.SRGB:
            return WI.UIString("sRGB", "sRGB @ Color Space", "Label for a canvas that uses the sRGB color space.");
        case WI.Canvas.ColorSpace.SRGBLinear:
            return WI.UIString("Linear sRGB", "Linear sRGB @ Color Space", "Label for a canvas that uses the linear sRGB color space.");
        case WI.Canvas.ColorSpace.DisplayP3:
            return WI.UIString("Display P3", "Display P3 @ Color Space", "Label for a canvas that uses the Display P3 color space.");
        case WI.Canvas.ColorSpace.DisplayP3Linear:
            return WI.UIString("Linear Display P3", "Linear Display P3 @ Color Space", "Label for a canvas that uses the linear Display P3 color space.");
        }

        console.assert(false, "Unknown canvas color space", colorSpace);
        return null;
    }

    static resetUniqueDisplayNameNumbers()
    {
        Canvas._nextContextUniqueDisplayNameNumber = 1;
        Canvas._nextDeviceUniqueDisplayNameNumber = 1;
    }

    // Public

    get target() { return this._target; }
    get identifier() { return this._identifier; }
    get contextType() { return this._contextType; }
    get sizes() { return this._sizes; }
    get memoryCost() { return this._memoryCost; }
    get cssCanvasNames() { return this._cssCanvasNames; }
    get contextAttributes() { return this._contextAttributes; }
    get features() { return this._features; }
    get extensions() { return this._extensions; }
    get stackTrace() { return this._stackTrace; }
    get shaderProgramCollection() { return this._shaderProgramCollection; }
    get recordingCollection() { return this._recordingCollection; }
    get recordingFrameCount() { return this._recordingFrames.length; }
    get recordingBufferUsed() { return this._recordingBufferUsed; }

    get recordingActive()
    {
        return this._recordingState !== WI.Canvas.RecordingState.Inactive;
    }

    get displayName()
    {
        if (this._displayName)
            return this._displayName;

        if (this._contextType === Canvas.ContextType.WebGPU) {
            if (!this._uniqueDisplayNameNumber)
                this._uniqueDisplayNameNumber = Canvas._nextDeviceUniqueDisplayNameNumber++;
            return WI.UIString("Device %d").format(this._uniqueDisplayNameNumber);
        }

        if (this._cssCanvasNames.length === 1)
            return WI.UIString("CSS canvas \u201C%s\u201D").format(this._cssCanvasNames[0]);

        if (this._domNodes.length === 1) {
            let idSelector = this._domNodes[0].escapedIdSelector;
            if (idSelector)
                return WI.UIString("Canvas %s").format(idSelector);
        }

        if (!this._uniqueDisplayNameNumber)
            this._uniqueDisplayNameNumber = Canvas._nextContextUniqueDisplayNameNumber++;
        return WI.UIString("Canvas %d").format(this._uniqueDisplayNameNumber);
    }

    get is2D()
    {
        return this._contextType === Canvas.ContextType.Canvas2D || this._contextType === Canvas.ContextType.OffscreenCanvas2D;
    }

    get isBitmapRender()
    {
        return this._contextType === Canvas.ContextType.BitmapRenderer || this._contextType === Canvas.ContextType.OffscreenBitmapRenderer;
    }

    get isWebGL()
    {
        return this._contextType === Canvas.ContextType.WebGL || this._contextType === Canvas.ContextType.OffscreenWebGL;
    }

    get isWebGL2()
    {
        return this._contextType === Canvas.ContextType.WebGL2 || this._contextType === Canvas.ContextType.OffscreenWebGL2;
    }

    requestNodes()
    {
        if (!this._requestNodesPromise) {
            let requestPromise;
            // COMPATIBILITY (macOS X.Y, iOS X.Y): `Canvas.requestNode` was renamed to `Canvas.requestNodes`.
            if (this._target.hasCommand("Canvas.requestNode")) {
                WI.domManager.ensureDocument();
                requestPromise = this._target.CanvasAgent.requestNode(this._identifier).then(({nodeId}) => ({nodeIds: nodeId ? [nodeId] : []}));
            } else if (this._target.hasCommand("Canvas.requestNodes")) {
                WI.domManager.ensureDocument();
                requestPromise = this._target.CanvasAgent.requestNodes(this._identifier);
            } else
                requestPromise = Promise.resolve({nodeIds: []});

            let promise = requestPromise.then(({nodeIds}) => {
                if (promise !== this._requestNodesPromise)
                    return this._domNodes;

                this._domNodes = nodeIds.map((nodeId) => WI.Canvas._domNodeForId(this._target, nodeId));
                return this._domNodes;
            }, () => promise === this._requestNodesPromise ? [] : this._domNodes);
            this._requestNodesPromise = promise;
        }
        return this._requestNodesPromise;
    }

    requestContent()
    {
        return this._target.CanvasAgent.requestContent(this._identifier).then((result) => result.content).catch((error) => console.error(error));
    }

    requestCSSCanvasClientNodes()
    {
        if (!this._requestCSSCanvasClientNodesPromise) {
            let requestPromise;
            // COMPATIBILITY (macOS X.Y, iOS X.Y): `Canvas.requestCSSCanvasClientNodes` was temporarily renamed to `Canvas.requestClientNodes`.
            if (this._target.hasCommand("Canvas.requestClientNodes")) {
                WI.domManager.ensureDocument();
                requestPromise = this._target.CanvasAgent.requestClientNodes(this._identifier);
            } else if (this._target.hasCommand("Canvas.requestCSSCanvasClientNodes")) {
                WI.domManager.ensureDocument();
                requestPromise = this._target.CanvasAgent.requestCSSCanvasClientNodes(this._identifier);
            } else
                requestPromise = Promise.resolve({clientNodeIds: []});

            let promise = requestPromise.then(({clientNodeIds}) => {
                if (promise !== this._requestCSSCanvasClientNodesPromise)
                    return this._cssCanvasClientNodes;

                this._cssCanvasClientNodes = clientNodeIds.map((clientNodeId) => WI.Canvas._domNodeForId(this._target, clientNodeId));
                return this._cssCanvasClientNodes;
            }, () => promise === this._requestCSSCanvasClientNodesPromise ? [] : this._cssCanvasClientNodes);
            this._requestCSSCanvasClientNodesPromise = promise;
        }
        return this._requestCSSCanvasClientNodesPromise;
    }

    highlight()
    {
        Promise.all([
            (!this._cssCanvasNames.length || this._contextType === Canvas.ContextType.WebGPU) ? this.requestNodes() : [],
            (this._cssCanvasNames.length || this._contextType === Canvas.ContextType.WebGPU) ? this.requestCSSCanvasClientNodes() : [],
        ]).then((nodesLists) => {
            let nodes = nodesLists.flat();
            if (!nodes.length)
                return;
            WI.domManager.highlightDOMNodeList(nodes);
        });
    }

    startRecording(singleFrame)
    {
        let handleStartRecording = (error) => {
            if (error) {
                console.error(error);
                return;
            }

            this._recordingState = WI.Canvas.RecordingState.ActiveFrontend;
        };

        if (singleFrame) {
            const frameCount = 1;
            this._target.CanvasAgent.startRecording(this._identifier, frameCount, handleStartRecording);
        } else
            this._target.CanvasAgent.startRecording(this._identifier, handleStartRecording);
    }

    stopRecording()
    {
        this._target.CanvasAgent.stopRecording(this._identifier);
    }

    saveIdentityToCookie(cookie)
    {
        if (this._cssCanvasNames.length)
            cookie[WI.Canvas.CSSCanvasNameCookieKey] = JSON.stringify(this._cssCanvasNames);
        else if (this._domNodes.length)
            cookie[WI.Canvas.NodePathCookieKey] = JSON.stringify(this._domNodes.map((domNode) => domNode.path));

    }

    sizeChanged(sizes)
    {
        // Called from WI.CanvasManager.

        console.assert(Array.isArray(sizes), sizes);
        console.assert(sizes.every((size) => size instanceof WI.Size), sizes);

        if (Array.shallowEqual(this._sizes, sizes))
            return;

        this._sizes = sizes;

        this.dispatchEventToListeners(WI.Canvas.Event.SizeChanged);
    }

    memoryChanged(memoryCost)
    {
        // Called from WI.CanvasManager.

        if (memoryCost === this._memoryCost)
            return;

        this._memoryCost = memoryCost;

        this.dispatchEventToListeners(WI.Canvas.Event.MemoryChanged);
    }

    enableExtension(extension)
    {
        // Called from WI.CanvasManager.

        this._extensions.add(extension);

        this.dispatchEventToListeners(WI.Canvas.Event.ExtensionEnabled, {extension});
    }

    nodesChanged()
    {
        // Called from WI.CanvasManager.

        this._requestNodesPromise = null;

        this.dispatchEventToListeners(Canvas.Event.NodesChanged);
    }

    cssCanvasClientNodesChanged()
    {
        // Called from WI.CanvasManager.

        this._cssCanvasClientNodes = null;
        this._requestCSSCanvasClientNodesPromise = null;

        this.dispatchEventToListeners(Canvas.Event.CSSCanvasClientNodesChanged);
    }

    cssCanvasNamesChanged(cssCanvasNames)
    {
        // Called from WI.CanvasManager.

        console.assert(Array.isArray(cssCanvasNames), cssCanvasNames);

        if (Array.shallowEqual(this._cssCanvasNames, cssCanvasNames))
            return;

        this._cssCanvasNames = cssCanvasNames;

        this.dispatchEventToListeners(Canvas.Event.CSSCanvasNamesChanged);
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

        let version = InspectorBackend.getVersion("Recording");
        this._recordingFrames.pushAll(framesPayload.map((frame) => WI.RecordingFrame.fromPayload(frame, version)));

        this._recordingBufferUsed = bufferUsed;

        this.dispatchEventToListeners(WI.Canvas.Event.RecordingProgress);
    }

    recordingFinished(recordingPayload)
    {
        // Called from WI.CanvasManager.

        let initiatedByUser = this._recordingState === WI.Canvas.RecordingState.ActiveFrontend;

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

    // Private

    async _calculateSize()
    {
        let remoteObject = await WI.RemoteObject.resolveCanvasContext(this);
        if (!remoteObject)
            return;

        function inspectedPage_context_getCanvasSize() {
            return {
                width: this.canvas.width,
                height: this.canvas.height,
            };
        }
        let size = await remoteObject.callFunctionJSON(inspectedPage_context_getCanvasSize);
        remoteObject.release();

        this.sizeChanged([WI.Size.fromJSON(size)]);
    }
};

WI.Canvas._nextContextUniqueDisplayNameNumber = 1;
WI.Canvas._nextDeviceUniqueDisplayNameNumber = 1;

WI.Canvas.FrameURLCookieKey = "canvas-frame-url";
WI.Canvas.CSSCanvasNameCookieKey = "canvas-css-canvas-name";

WI.Canvas.ContextType = {
    Canvas2D: "canvas-2d",
    OffscreenCanvas2D: "offscreen-canvas-2d",
    BitmapRenderer: "bitmaprenderer",
    OffscreenBitmapRenderer: "offscreen-bitmaprenderer",
    WebGL: "webgl",
    OffscreenWebGL: "offscreen-webgl",
    WebGL2: "webgl2",
    OffscreenWebGL2: "offscreen-webgl2",
    WebGPU: "webgpu",
};

WI.Canvas.ColorSpace = {
    SRGB: "srgb",
    DisplayP3: "display-p3",
};

WI.Canvas.RecordingState = {
    Inactive: "canvas-recording-state-inactive",
    ActiveFrontend: "canvas-recording-state-active-frontend",
    ActiveConsole: "canvas-recording-state-active-console",
    ActiveAutoCapture: "canvas-recording-state-active-auto-capture",
};

WI.Canvas.Event = {
    SizeChanged: "canvas-size-changed",
    MemoryChanged: "canvas-memory-changed",
    ExtensionEnabled: "canvas-extension-enabled",
    NodesChanged: "canvas-nodes-changed",
    CSSCanvasClientNodesChanged: "canvas-css-canvas-client-nodes-changed",
    CSSCanvasNamesChanged: "canvas-css-canvas-names-changed",
    RecordingStarted: "canvas-recording-started",
    RecordingProgress: "canvas-recording-progress",
    RecordingStopped: "canvas-recording-stopped",
};
