TestPage.registerInitializer(() => {
    function sanitizeURL(url) {
        return url.replace(/^.*?LayoutTests\//, "");
    }

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
                    let swizzleNames = action.swizzleTypes.map((item, i) => {
                        let swizzleText = WI.Recording.displayNameForSwizzleType(item);
                        if (action.parameters[i] != action._payloadParameters[i] && Number.isInteger(action._payloadParameters[i]))
                            swizzleText += " (" + action._payloadParameters[i] + ")";
                        return swizzleText;
                    });
                    InspectorTest.log("      swizzleTypes: [" + swizzleNames.join(", ") + "]");
                }

                if (action.trace.length) {
                    InspectorTest.log("      trace:");

                    for (let k = 0; k < action.trace.length; ++k) {
                        let callFrame = action.trace[k];
                        let traceText = `        ${k}: `;
                        traceText += callFrame.functionName || "(anonymous function)";

                        if (callFrame.nativeCode)
                            traceText += " - [native code]";
                        else if (callFrame.programCode)
                            traceText += " - [program code]";
                        else if (callFrame.sourceCodeLocation) {
                            let location = callFrame.sourceCodeLocation;
                            traceText += " - " + sanitizeURL(location.sourceCode.url) + `:${location.lineNumber}:${location.columnNumber}`;
                        }

                        traceText += " (" + action._payloadTrace[k] + ")";
                        InspectorTest.log(traceText);
                    }
                }

                if (action.snapshot)
                    InspectorTest.log("      snapshot: " + JSON.stringify(action.snapshot) + " (" + action._payloadSnapshot + ")");
            }
        }

        InspectorTest.log("data:");
        log(recording.data, "  ");
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

        WI.canvasManager.awaitEvent(WI.CanvasManager.Event.RecordingStopped).then((event) => {
            InspectorTest.evaluateInPage(`cancelActions()`);

            let recording = event.data.recording;
            InspectorTest.assert(recording.source === canvas, "Recording should be of the given canvas.");
            InspectorTest.assert(recording.source.contextType === type, `Recording should be of a canvas with type "${type}".`);
            InspectorTest.assert(recording.source.recordingCollection.items.has(recording), "Recording should be in the canvas' list of recordings.");

            return recording.actions.then(() => {
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
});
