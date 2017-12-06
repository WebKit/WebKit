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

        this._recordingCanvas = null;
        this._recordingFrameMap = new Map;

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

    get recordingCanvas() { return this._recordingCanvas; }

    importRecording()
    {
        WI.loadDataFromFile((data, filename) => {
            if (!data)
                return;

            let payload = null;
            try {
                payload = JSON.parse(data);
            } catch (e) {
                WI.Recording.synthesizeError(e);
                return;
            }

            let recording = WI.Recording.fromPayload(payload);
            if (!recording) {
                WI.Recording.synthesizeError(WI.UIString("unsupported version."));
                return;
            }

            let extensionStart = filename.lastIndexOf(".");
            if (extensionStart !== -1)
                filename = filename.substring(0, extensionStart);
            recording.createDisplayName(filename);

            this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingImported, {recording});
        });
    }

    startRecording(canvas, singleFrame)
    {
        console.assert(!this._recordingCanvas, "Recording already started.");
        if (this._recordingCanvas)
            return;

        this._recordingCanvas = canvas;

        CanvasAgent.startRecording(canvas.identifier, singleFrame, (error) => {
            if (error) {
                console.error(error);
                this._recordingCanvas = null;
                return;
            }

            this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingStarted, {canvas});
        });
    }

    stopRecording()
    {
        console.assert(this._recordingCanvas, "No recording started.");
        if (!this._recordingCanvas)
            return;

        let canvas = this._recordingCanvas;
        this._recordingCanvas = null;

        CanvasAgent.stopRecording(canvas.identifier, (error) => {
            if (!error)
                return;

            console.error(error);
            this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingStopped, {canvas, recording: null});
        });
    }

    canvasAdded(canvasPayload)
    {
        // Called from WI.CanvasObserver.

        console.assert(!this._canvasIdentifierMap.has(canvasPayload.canvasId), `Canvas already exists with id ${canvasPayload.canvasId}.`);

        let canvas = WI.Canvas.fromPayload(canvasPayload);
        this._canvasIdentifierMap.set(canvas.identifier, canvas);

        this.dispatchEventToListeners(WI.CanvasManager.Event.CanvasAdded, {canvas});
    }

    canvasRemoved(canvasIdentifier)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.take(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        this._removeCanvas(canvas);
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

    recordingProgress(canvasIdentifier, framesPayload, bufferUsed)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        let existingFrames = this._recordingFrameMap.get(canvasIdentifier);
        if (!existingFrames) {
            existingFrames = [];
            this._recordingFrameMap.set(canvasIdentifier, existingFrames);
        }

        existingFrames.push(...framesPayload.map(WI.RecordingFrame.fromPayload));

        this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingProgress, {
            canvas,
            frameCount: existingFrames.length,
            bufferUsed,
        });
    }

    recordingFinished(canvasIdentifier, recordingPayload)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);

        let fromConsole = canvas !== this._recordingCanvas;
        if (!fromConsole)
            this._recordingCanvas = null;

        if (!canvas)
            return;

        let frames = this._recordingFrameMap.take(canvasIdentifier);
        let recording = recordingPayload ? WI.Recording.fromPayload(recordingPayload, frames) : null;
        if (recording) {
            recording.source = canvas;
            recording.createDisplayName(recordingPayload.name);

            canvas.recordingCollection.add(recording);
        }

        this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingStopped, {canvas, recording, fromConsole});
    }

    extensionEnabled(canvasIdentifier, extension)
    {
        // Called from WI.CanvasObserver.

        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.enableExtension(extension);
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

    _removeCanvas(canvas)
    {
        for (let program of canvas.shaderProgramCollection.items) {
            this._shaderProgramIdentifierMap.delete(program.identifier);
            this._dispatchShaderProgramRemoved(program);
        }

        this.dispatchEventToListeners(WI.CanvasManager.Event.CanvasRemoved, {canvas});
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);
        if (!event.target.isMainFrame())
            return;

        WI.Canvas.resetUniqueDisplayNameNumbers();

        for (let canvas of this._canvasIdentifierMap.values())
            this._removeCanvas(canvas);

        this._shaderProgramIdentifierMap.clear();
        this._canvasIdentifierMap.clear();
    }

    _dispatchShaderProgramRemoved(program)
    {
        this.dispatchEventToListeners(WI.CanvasManager.Event.ShaderProgramRemoved, {program});
    }
};

WI.CanvasManager.Event = {
    CanvasAdded: "canvas-manager-canvas-was-added",
    CanvasRemoved: "canvas-manager-canvas-was-removed",
    RecordingImported: "canvas-manager-recording-imported",
    RecordingStarted: "canvas-manager-recording-started",
    RecordingProgress: "canvas-manager-recording-progress",
    RecordingStopped: "canvas-manager-recording-stopped",
    ShaderProgramAdded: "canvas-manager-shader-program-added",
    ShaderProgramRemoved: "canvas-manager-shader-program-removed",
};
