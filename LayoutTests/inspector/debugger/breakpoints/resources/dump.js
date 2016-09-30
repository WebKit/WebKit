TestPage.registerInitializer(() => {
    // Debugger.Location
    function createLocation(script, lineNumber, columnNumber) {
        return {scriptId: script.id, lineNumber, columnNumber};
    }

    // Dump all pause locations test.
    // Tries to set a breakpoint at every line:column in the file and
    // logs all unique pause locations with the originator line:column.
    window.addDumpAllPauseLocationsTestCase = function(suite, {name, scriptRegex}) {
        suite.addTestCase({
            name, test(resolve, reject) {
                let script = window.findScript(scriptRegex);
                window.loadLinesFromSourceCode(script).then((lines) => {
                    // Iterate all possible pause locations in this file.
                    let pauseLocations = new Map;
                    let seenPauseLocations = new Set;

                    for (let line = script.range.startLine; line <= script.range.endLine; ++line) {
                        let max = lines[line].length;
                        for (let column = 0; column <= max; ++column) {
                            DebuggerAgent.setBreakpoint(createLocation(script, line, column), (error, breakpointId, location) => {
                                if (error)
                                    return;
                                let key = JSON.stringify(location);
                                if (seenPauseLocations.has(key))
                                    return;
                                pauseLocations.set({lineNumber: line, columnNumber: column}, location);
                            });
                        }
                    }

                    // Log the unique locations and the first input that produced it.
                    InspectorBackend.runAfterPendingDispatches(() => {
                        InspectorTest.log("");
                        for (let [inputLocation, payload] of pauseLocations) {
                            InspectorTest.log(`INSERTING AT: ${inputLocation.lineNumber}:${inputLocation.columnNumber}`);
                            InspectorTest.log(`PAUSES AT: ${payload.lineNumber}:${payload.columnNumber}`);
                            let resolvedLocation = script.createSourceCodeLocation(payload.lineNumber, payload.columnNumber);
                            window.logResolvedBreakpointLinesWithContext(inputLocation, resolvedLocation, 3);
                            InspectorTest.log("");
                        }
                        resolve();
                    });
                });
            }
        });
    }

    // Dump each line test.
    // Tries to set a breakpoint at every line:0 in the file and
    // logs its pause location. Clears breakpoints before each line.
    window.addDumpEachLinePauseLocationTestCase = function(suite, {name, scriptRegex}) {
        suite.addTestCase({
            name, test(resolve, reject) {
                let script = window.findScript(scriptRegex);
                window.loadLinesFromSourceCode(script).then(() => {
                    // Set one breakpoint per line.
                    for (let line = script.range.startLine; line <= script.range.endLine; ++line) {
                        DebuggerAgent.setBreakpoint(createLocation(script, line, 0), (error, breakpointId, payload) => {
                            InspectorTest.log("");
                            if (error) {
                                InspectorTest.log(`INSERTING AT: ${line}:0`);
                                InspectorTest.log(`PRODUCES: ${error}`);
                            } else {
                                let inputLocation = {lineNumber: line, columnNumber: 0};
                                let resolvedLocation = script.createSourceCodeLocation(payload.lineNumber, payload.columnNumber);
                                InspectorTest.log(`INSERTING AT: ${inputLocation.lineNumber}:${inputLocation.columnNumber}`);
                                InspectorTest.log(`PAUSES AT: ${payload.lineNumber}:${payload.columnNumber}`);                                
                                window.logResolvedBreakpointLinesWithContext(inputLocation, resolvedLocation, 3);
                                InspectorTest.log("");
                            }
                        });

                        // Clear the breakpoint we just set without knowing its breakpoint identifier.
                        DebuggerAgent.disable();
                        DebuggerAgent.enable();
                    }

                    // Resolve after all lines have been tried.
                    InspectorBackend.runAfterPendingDispatches(() => {
                        resolve();
                    });
                });
            }
        });
    }
});
