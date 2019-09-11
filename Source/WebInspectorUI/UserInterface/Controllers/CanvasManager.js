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

// FIXME: CanvasManager lacks advanced multi-target support. (Canvases per-target)

WI.CanvasManager = class CanvasManager extends WI.Object
{
    constructor()
    {
        super();

        this._enabled = false;
        this._canvasIdentifierMap = new Map;
        this._shaderProgramIdentifierMap = new Map;
        this._savedRecordings = new Set;

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Agent

    get domains() { return ["Canvas"]; }

    activateExtraDomain(domain)
    {
        console.assert(domain === "Canvas");

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    // Target

    initializeTarget(target)
    {
        if (!this._enabled)
            return;

        if (target.CanvasAgent) {
            target.CanvasAgent.enable();

            if (target.CanvasAgent.setRecordingAutoCaptureFrameCount && WI.settings.canvasRecordingAutoCaptureEnabled.value && WI.settings.canvasRecordingAutoCaptureFrameCount.value)
                target.CanvasAgent.setRecordingAutoCaptureFrameCount(WI.settings.canvasRecordingAutoCaptureFrameCount.value);
        }
    }

    // Static

    static supportsRecordingAutoCapture()
    {
        return InspectorBackend.domains.Canvas && InspectorBackend.domains.Canvas.setRecordingAutoCaptureFrameCount;
    }

    // Public

    get savedRecordings() { return this._savedRecordings; }

    get canvases()
    {
        return Array.from(this._canvasIdentifierMap.values());
    }

    get shaderPrograms()
    {
        return Array.from(this._shaderProgramIdentifierMap.values());
    }

    async processJSON({filename, json, error})
    {
        if (error) {
            WI.Recording.synthesizeError(error);
            return;
        }

        if (typeof json !== "object" || json === null) {
            WI.Recording.synthesizeError(WI.UIString("invalid JSON"));
            return;
        }

        let recording = WI.Recording.fromPayload(json);
        if (!recording)
            return;

        let extensionStart = filename.lastIndexOf(".");
        if (extensionStart !== -1)
            filename = filename.substring(0, extensionStart);
        recording.createDisplayName(filename);

        this._savedRecordings.add(recording);

        this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingSaved, {recording, imported: true, initiatedByUser: true});
    }

    enable()
    {
        console.assert(!this._enabled);

        this._enabled = true;

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    disable()
    {
        console.assert(this._enabled);

        for (let target of WI.targets) {
            if (target.CanvasAgent)
                target.CanvasAgent.disable();
        }

        this._canvasIdentifierMap.clear();
        this._shaderProgramIdentifierMap.clear();
        this._savedRecordings.clear();

        this._enabled = false;
    }

    setRecordingAutoCaptureFrameCount(enabled, count)
    {
        console.assert(!isNaN(count) && count >= 0);

        for (let target of WI.targets) {
            if (target.CanvasAgent)
                target.CanvasAgent.setRecordingAutoCaptureFrameCount(enabled ? count : 0);
        }

        WI.settings.canvasRecordingAutoCaptureEnabled.value = enabled && count;
        WI.settings.canvasRecordingAutoCaptureFrameCount.value = count;
    }

    // CanvasObserver

    canvasAdded(canvasPayload)
    {
        console.assert(!this._canvasIdentifierMap.has(canvasPayload.canvasId), `Canvas already exists with id ${canvasPayload.canvasId}.`);

        let canvas = WI.Canvas.fromPayload(canvasPayload);
        this._canvasIdentifierMap.set(canvas.identifier, canvas);

        this.dispatchEventToListeners(WI.CanvasManager.Event.CanvasAdded, {canvas});
    }

    canvasRemoved(canvasIdentifier)
    {
        let canvas = this._canvasIdentifierMap.take(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        this._removeCanvas(canvas);
    }

    canvasMemoryChanged(canvasIdentifier, memoryCost)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.memoryCost = memoryCost;
    }

    cssCanvasClientNodesChanged(canvasIdentifier)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.cssCanvasClientNodesChanged();
    }

    recordingStarted(canvasIdentifier, initiator)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.recordingStarted(initiator);
    }

    recordingProgress(canvasIdentifier, framesPayload, bufferUsed)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.recordingProgress(framesPayload, bufferUsed);
    }

    recordingFinished(canvasIdentifier, recordingPayload)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.recordingFinished(recordingPayload);
    }

    extensionEnabled(canvasIdentifier, extension)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.enableExtension(extension);
    }

    programCreated(canvasIdentifier, programIdentifier)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        console.assert(!this._shaderProgramIdentifierMap.has(programIdentifier), `ShaderProgram already exists with id ${programIdentifier}.`);

        let program = new WI.ShaderProgram(programIdentifier, canvas);
        this._shaderProgramIdentifierMap.set(program.identifier, program);

        canvas.shaderProgramCollection.add(program);
    }

    programDeleted(programIdentifier)
    {
        let program = this._shaderProgramIdentifierMap.take(programIdentifier);
        console.assert(program);
        if (!program)
            return;

        program.canvas.shaderProgramCollection.remove(program);
    }

    // Private

    _removeCanvas(canvas)
    {
        for (let program of canvas.shaderProgramCollection)
            this._shaderProgramIdentifierMap.delete(program.identifier);

        canvas.shaderProgramCollection.clear();

        for (let recording of canvas.recordingCollection) {
            recording.source = null;
            recording.createDisplayName(recording.displayName);
            this._savedRecordings.add(recording);
            this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingSaved, {recording});
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
};

WI.CanvasManager.Event = {
    CanvasAdded: "canvas-manager-canvas-was-added",
    CanvasRemoved: "canvas-manager-canvas-was-removed",
    RecordingSaved: "canvas-manager-recording-saved",
};
