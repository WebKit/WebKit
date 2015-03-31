/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.CanvasManager = class CanvasManager extends WebInspector.Object
{
    constructor()
    {
        super();

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._waitingForCanvasesPayload = false;
        this._canvasIdMap = new Map;

        this._initialize();
    }

    canvasesForFrame(frame)
    {
        if (this._waitingForCanvasesPayload)
            return [];

        var canvases = [];
        for (var canvas of this._canvasIdMap.values()) {
            if (canvas.parentFrame.id === frame.id)
                canvases.push(canvas);
        }
        return canvases;
    }

    canvasAdded(canvasPayload)
    {
        // Called from WebInspector.CanvasObserver.
        console.assert(!this._canvasIdMap.has(canvasPayload.canvasId));
        var canvas = WebInspector.Canvas.fromPayload(canvasPayload);

        this._canvasIdMap.set(canvas.id, canvas);
        this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasWasAdded, {canvas: canvas});
    }

    canvasRemoved(canvasIdentifier)
    {
        // Called from WebInspector.CanvasObserver.

        console.assert(this._canvasIdMap.has(canvasIdentifier));
        if (!this._canvasIdMap.has(canvasIdentifier))
            return;

        var canvas = this._canvasIdMap.take(canvasIdentifier);
        this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasWasRemoved, {canvas: canvas});
    }

    programCreated(programIdentifier)
    {
        // Called from WebInspector.CanvasObserver.

        var canvas = this._canvasIdMap.get(programIdentifier.canvasId);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.programWasCreated(new WebInspector.ShaderProgram(programIdentifier, canvas));
    }

    programDeleted(programIdentifier)
    {
        // Called from WebInspector.CanvasObserver.

        var canvas = this._canvasIdMap.get(programIdentifier.canvasId);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.programWasDeleted(programIdentifier);
    }

    // Private

    _getCanvasesForFrameId(frameIdentifier)
    {
        var canvases = [];
        for (var canvas of this._canvasIdMap.values()) {
            if (canvas.parentFrame.id === frameIdentifier)
                canvases.push(canvas);
        }
        return canvases;
    }

    _initialize()
    {
        if (!window.CanvasAgent)
            return;

        for (var canvas of this._canvasIdMap.values())
            this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasWasRemoved, {canvas: canvas});

        WebInspector.Canvas.resetUniqueDisplayNameNumbers();

        this._canvasIdMap.clear();
        this._waitingForCanvasesPayload = true;

        CanvasAgent.getCanvases(this._processCanvasesPayload.bind(this));
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WebInspector.Frame);

        if (!event.target.isMainFrame())
            return;

        this._initialize();
    }

    _processCanvasesPayload(error, canvasArrayPayload)
    {
        console.assert(this._waitingForCanvasesPayload);
        this._waitingForCanvasesPayload = false;

        if (error) {
            console.error(JSON.stringify(error));
            return;
        }

        for (var canvasPayload of canvasArrayPayload) {
            if (!this._canvasIdMap.has(canvasPayload.canvasId)) {
                var canvas = WebInspector.Canvas.fromPayload(canvasPayload);
                this._canvasIdMap.set(canvas.id, canvas);
            }
        }

        this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasesAvailable);
    }
};

WebInspector.CanvasManager.Event = {
    CanvasesAvailable: "canvas-manager-canvases-available",
    CanvasWasAdded: "canvas-manager-canvas-was-added",
    CanvasWasRemoved: "canvas-manager-canvas-was-removed"
};
