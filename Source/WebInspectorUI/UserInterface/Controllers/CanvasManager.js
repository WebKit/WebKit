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
        this._canvasCollection = new WI.CanvasCollection;
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

        if (target.hasDomain("Canvas")) {
            target.CanvasAgent.enable();

            // COMPATIBILITY (iOS 12): Canvas.setRecordingAutoCaptureFrameCount did not exist yet.
            if (target.hasCommand("Canvas.setRecordingAutoCaptureFrameCount") && WI.settings.canvasRecordingAutoCaptureEnabled.value && WI.settings.canvasRecordingAutoCaptureFrameCount.value)
                target.CanvasAgent.setRecordingAutoCaptureFrameCount(WI.settings.canvasRecordingAutoCaptureFrameCount.value);
        }
    }

    // Static

    static supportsRecordingAutoCapture()
    {
        return InspectorBackend.hasCommand("Canvas.setRecordingAutoCaptureFrameCount");
    }

    // Public

    get canvasCollection() { return this._canvasCollection; }
    get savedRecordings() { return this._savedRecordings; }

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
            if (target.hasDomain("Canvas"))
                target.CanvasAgent.disable();
        }

        this._canvasCollection.clear();
        this._canvasIdentifierMap.clear();
        this._shaderProgramIdentifierMap.clear();
        this._savedRecordings.clear();

        this._enabled = false;
    }

    setRecordingAutoCaptureFrameCount(enabled, count)
    {
        console.assert(!isNaN(count) && count >= 0);

        for (let target of WI.targets) {
            // COMPATIBILITY (iOS 12): Canvas.setRecordingAutoCaptureFrameCount did not exist yet.
            if (target.hasCommand("Canvas.setRecordingAutoCaptureFrameCount"))
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
        this._canvasCollection.add(canvas);
        this._canvasIdentifierMap.set(canvas.identifier, canvas);
    }

    canvasRemoved(canvasIdentifier)
    {
        let canvas = this._canvasIdentifierMap.take(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        this._saveRecordings(canvas);

        this._canvasCollection.remove(canvas);

        for (let program of canvas.shaderProgramCollection)
            this._shaderProgramIdentifierMap.delete(program.identifier);

        canvas.shaderProgramCollection.clear();
    }

    canvasMemoryChanged(canvasIdentifier, memoryCost)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.memoryCost = memoryCost;
    }

    clientNodesChanged(canvasIdentifier)
    {
        let canvas = this._canvasIdentifierMap.get(canvasIdentifier);
        console.assert(canvas);
        if (!canvas)
            return;

        canvas.clientNodesChanged();
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

    programCreated(shaderProgramPayload)
    {
        let canvas = this._canvasIdentifierMap.get(shaderProgramPayload.canvasId);
        console.assert(canvas);
        if (!canvas)
            return;

        let programId = shaderProgramPayload.programId;
        console.assert(!this._shaderProgramIdentifierMap.has(programId), `ShaderProgram already exists with id ${programId}.`);

        // COMPATIBILITY (iOS 13.0): `Canvas.ShaderProgram.programType` did not exist yet.
        let programType = shaderProgramPayload.programType;
        if (!programType)
            programType = WI.ShaderProgram.ProgramType.Render;

        let options = {};

        // COMPATIBILITY (iOS 13.0): `Canvas.ShaderProgram.sharesVertexFragmentShader` did not exist yet.
        if (shaderProgramPayload.sharesVertexFragmentShader)
            options.sharesVertexFragmentShader = true;

        let program = new WI.ShaderProgram(programId, programType, canvas, options);
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

    _saveRecordings(canvas)
    {
        for (let recording of canvas.recordingCollection) {
            recording.source = null;
            recording.createDisplayName(recording.displayName);
            this._savedRecordings.add(recording);
            this.dispatchEventToListeners(WI.CanvasManager.Event.RecordingSaved, {recording});
        }
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);
        if (!event.target.isMainFrame())
            return;

        WI.Canvas.resetUniqueDisplayNameNumbers();

        for (let canvas of this._canvasIdentifierMap.values())
            this._saveRecordings(canvas);

        this._canvasCollection.clear();
        this._canvasIdentifierMap.clear();
        this._shaderProgramIdentifierMap.clear();
    }
};

WI.CanvasManager.Event = {
    RecordingSaved: "canvas-manager-recording-saved",
};
