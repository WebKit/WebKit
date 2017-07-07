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

WebInspector.CanvasManager = class CanvasManager extends WebInspector.Object
{
    constructor()
    {
        super();

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._canvasIdentifierMap = new Map;

        if (window.CanvasAgent)
            CanvasAgent.enable();
    }

    // Public

    get canvases()
    {
        return [...this._canvasIdentifierMap.values()];
    }

    canvasAdded(canvasPayload)
    {
        // Called from WebInspector.CanvasObserver.

        console.assert(!this._canvasIdentifierMap.has(canvasPayload.canvasId), `Canvas already exists with id ${canvasPayload.canvasId}.`);

        let canvas = WebInspector.Canvas.fromPayload(canvasPayload);
        this._canvasIdentifierMap.set(canvas.identifier, canvas);

        canvas.frame.canvasCollection.add(canvas);

        this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasWasAdded, {canvas});
    }

    canvasRemoved(canvasIdentifier)
    {
        // Called from WebInspector.CanvasObserver.

        let canvas = this._canvasIdentifierMap.take(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.frame.canvasCollection.remove(canvas);

        this.dispatchEventToListeners(WebInspector.CanvasManager.Event.CanvasWasRemoved, {canvas});
    }

    canvasMemoryChanged(canvasIdentifier, memoryCost)
    {
        // Called from WebInspector.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.memoryCost = memoryCost;
    }

    cssCanvasClientNodesChanged(canvasIdentifier)
    {
        // Called from WebInspector.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.cssCanvasClientNodesChanged();
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WebInspector.Frame);
        if (!event.target.isMainFrame())
            return;

        WebInspector.Canvas.resetUniqueDisplayNameNumbers();

        if (this._canvasIdentifierMap.size) {
            this._canvasIdentifierMap.clear();
            this.dispatchEventToListeners(WebInspector.CanvasManager.Event.Cleared);
        }
    }
};

WebInspector.CanvasManager.Event = {
    Cleared: "canvas-manager-cleared",
    CanvasWasAdded: "canvas-manager-canvas-was-added",
    CanvasWasRemoved: "canvas-manager-canvas-was-removed",
};
