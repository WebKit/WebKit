TestPage.registerInitializer(() => {
    function log(object, indent) {
        for (let key of Object.keys(object)) {
            let value = object[key];
            if (typeof value === "string")
                value = sanitizeURL(value);
            InspectorTest.log(indent + key + ": " + JSON.stringify(value));
        }
    }

    function logRecording(recording) {
        InspectorTest.log("initialState:");

        InspectorTest.log("  attributes:");
        log(recording.initialState.attributes, "    ");

        InspectorTest.log("  parameters:");
        log(recording.initialState.parameters, "    ");

        InspectorTest.log("  content: " + JSON.stringify(recording.initialState.content));

        InspectorTest.log("frames:");
        for (let i = 0; i < recording.frames.length; ++i) {
            let frame = recording.frames[i];

            let frameText = `  ${i}:`;
            if (!isNaN(frame.duration))
                frameText += " (duration)";
            if (frame.incomplete)
                frameText += " (incomplete)";
            InspectorTest.log(frameText);

            for (let j = 0; j < frame.actions.length; ++j) {
                let action = frame.actions[j];
                let actionText = `    ${j}: ${action.name}`;

                let parameters = action.parameters.map((parameter) => {
                    if (typeof parameter === "object" && !Array.isArray(parameter))
                        return String(parameter);
                    return JSON.stringify(parameter);
                });
                if (action.isFunction)
                    actionText += "(" + parameters.join(", ") + ")";
                else if (!action.isGetter)
                    actionText += " = " + parameters[0];

                InspectorTest.log(actionText);

                if (action.swizzleTypes.length) {
                    let swizzleNames = action.swizzleTypes.map((item) => WI.Recording.displayNameForSwizzleType(item));
                    InspectorTest.log("      swizzleTypes: [" + swizzleNames.join(", ") + "]");
                }

                if (action.trace.length) {
                    InspectorTest.log("      trace:");

                    for (let k = 0; k < action.trace.length; ++k) {
                        let functionName = action.trace[k].functionName || "(anonymous function)";
                        InspectorTest.log(`        ${k}: ` + functionName);
                    }
                }

                if (action.snapshot)
                    InspectorTest.log("      snapshot: " + JSON.stringify(action.snapshot));
            }
        }
    }

    window.getCanvas = function(type) {
        let canvases = WI.canvasManager.canvases.filter((canvas) => canvas.contextType === type);
        InspectorTest.assert(canvases.length === 1, `There should only be one canvas with type "${type}".`);
        if (canvases.length !== 1)
            return null;
        return canvases[0];
    };

    window.startRecording = function(type, resolve, reject, {singleFrame, memoryLimit} = {}) {
        let canvas = getCanvas(type);
        if (!canvas) {
            reject(`Missing canvas with type "${type}".`);
            return;
        }

        let bufferUsed = 0;
        let frameCount = 0;
        function handleRecordingProgress(event) {
            InspectorTest.assert(canvas.recordingFrameCount > frameCount, "Additional frames were captured for this progress event.");
            frameCount = canvas.recordingFrameCount;

            InspectorTest.assert(canvas.recordingBufferUsed > bufferUsed, "Total memory usage increases with each progress event.");
            bufferUsed = canvas.recordingBufferUsed;
        }
        canvas.addEventListener(WI.Canvas.Event.RecordingProgress, handleRecordingProgress);

        canvas.awaitEvent(WI.Canvas.Event.RecordingStopped).then((event) => {
            canvas.removeEventListener(WI.Canvas.Event.RecordingProgress, handleRecordingProgress);

            InspectorTest.evaluateInPage(`cancelActions()`);

            let recording = event.data.recording;
            InspectorTest.assert(recording.source === canvas, "Recording should be of the given canvas.");
            InspectorTest.assert(recording.source.contextType === type, `Recording should be of a canvas with type "${type}".`);
            InspectorTest.assert(recording.source.recordingCollection.has(recording), "Recording should be in the canvas' list of recordings.");
            InspectorTest.assert(recording.frames.length === frameCount, `Recording should have ${frameCount} frames.`)

            return Promise.all(recording.actions.map((action) => action.swizzle(recording))).then(() => {
                logRecording(recording, type);
            });
        }).then(resolve, reject);

        CanvasAgent.startRecording(canvas.identifier, singleFrame, memoryLimit, (error) => {
            if (error) {
                reject(error);
                return;
            }

            InspectorTest.evaluateInPage(`performActions()`);
        });

        return canvas;
    };

    window.consoleRecord = function(type, resolve, reject) {
        let canvas = getCanvas(type);
        if (!canvas) {
            reject(`Missing canvas with type "${type}".`);
            return;
        }

        canvas.awaitEvent(WI.Canvas.Event.RecordingStopped).then((event) => {
            let recording = event.data.recording;

            InspectorTest.assert(recording.source === canvas, "Recording should be of the given canvas.");
            InspectorTest.assert(recording.source.contextType === type, `Recording should be of a canvas with type "${type}".`);
            InspectorTest.assert(recording.source.recordingCollection.has(recording), "Recording should be in the canvas' list of recordings.");

            InspectorTest.expectEqual(recording.displayName, "TEST", "The recording should have the name \"TEST\".");
            InspectorTest.expectEqual(recording.frames.length, 1, "The recording should have one frame.");
            InspectorTest.expectEqual(recording.frames[0].actions.length, 1, "The first frame should have one action.");
        }).then(resolve, reject);

        InspectorTest.evaluateInPage(`performConsoleActions()`);
    };
});
