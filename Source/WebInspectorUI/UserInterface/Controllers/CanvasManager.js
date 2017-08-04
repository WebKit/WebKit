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

WI.CanvasManager = class CanvasManager extends WI.Object
{
    constructor()
    {
        super();

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._canvasIdentifierMap = new Map;
        this._shaderProgramIdentifierMap = new Map;

        if (window.CanvasAgent)
            CanvasAgent.enable();
    }

    // Public

    get canvases()
    {
        return [...this._canvasIdentifierMap.values()];
    }

    get shaderPrograms()
    {
        return [...this._shaderProgramIdentifierMap.values()];
    }

    canvasAdded(canvasPayload)
    {
        // Called from WI.CanvasObserver.

        console.assert(!this._canvasIdentifierMap.has(canvasPayload.canvasId), `Canvas already exists with id ${canvasPayload.canvasId}.`);

        let canvas = WI.Canvas.fromPayload(canvasPayload);
        this._canvasIdentifierMap.set(canvas.identifier, canvas);

        canvas.frame.canvasCollection.add(canvas);

        this.dispatchEventToListeners(WI.CanvasManager.Event.CanvasWasAdded, {canvas});
    }

    canvasRemoved(canvasIdentifier)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.take(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        for (let program of canvas.shaderProgramCollection.items) {
            this._shaderProgramIdentifierMap.delete(program.identifier);
            this._dispatchShaderProgramRemoved(program);
        }

        canvas.frame.canvasCollection.remove(canvas);

        this.dispatchEventToListeners(WI.CanvasManager.Event.CanvasWasRemoved, {canvas});
    }

    canvasMemoryChanged(canvasIdentifier, memoryCost)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.memoryCost = memoryCost;
    }

    cssCanvasClientNodesChanged(canvasIdentifier)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.cssCanvasClientNodesChanged();
    }

    recordingFinished(canvasIdentifier, recordingPayload)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        let recording = WI.Recording.fromPayload(recordingPayload);
        recording.source = canvas;

        this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingFinished, {canvas, recording});
    }

    programCreated(canvasIdentifier, programIdentifier)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        console.assert(!this._shaderProgramIdentifierMap.has(programIdentifier), `ShaderProgram already exists with id ${programIdentifier}.`);

        let program = new WI.ShaderProgram(programIdentifier, canvas);
        this._shaderProgramIdentifierMap.set(program.identifier, program);

        canvas.shaderProgramCollection.add(program);

        this.dispatchEventToListeners(WI.CanvasManager.Event.ShaderProgramAdded, {program});
    }

    programDeleted(programIdentifier)
    {
        // Called from WI.CanvasObserver.

        let program = this._shaderProgramIdentifierMap.take(programIdentifier);
        console.assert(program);
        if (!program)
            return;

        program.canvas.shaderProgramCollection.remove(program);

        this._dispatchShaderProgramRemoved(program);
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);
        if (!event.target.isMainFrame())
            return;

        WI.Canvas.resetUniqueDisplayNameNumbers();

        this._shaderProgramIdentifierMap.clear();

        if (this._canvasIdentifierMap.size) {
            this._canvasIdentifierMap.clear();
            this.dispatchEventToListeners(WI.CanvasManager.Event.Cleared);
        }
    }

    _dispatchShaderProgramRemoved(program)
    {
        this.dispatchEventToListeners(WI.CanvasManager.Event.ShaderProgramRemoved, {program});
    }
};

WI.CanvasManager.Event = {
    Cleared: "canvas-manager-cleared",
    CanvasWasAdded: "canvas-manager-canvas-was-added",
    CanvasWasRemoved: "canvas-manager-canvas-was-removed",
    RecordingFinished: "canvas-managger-recording-finished",
    ShaderProgramAdded: "canvas-manager-shader-program-added",
    ShaderProgramRemoved: "canvas-manager-shader-program-removed",
};
