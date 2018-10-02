TestPage.registerInitializer(() => {
    let lines = [];
    let linesSourceCode = null;

    function insertCaretIntoStringAtIndex(str, index, caret="|") {
        return str.slice(0, index) + caret + str.slice(index);
    }

    window.findScript = function(regex) {
        for (let resource of WI.networkManager.mainFrame.resourceCollection) {
            if (regex.test(resource.url))
                return resource.scripts[0];
        }
    }

    window.loadLinesFromSourceCode = function(sourceCode) {
        linesSourceCode = sourceCode;
        return sourceCode.requestContent()
            .then((content) => {
                lines = sourceCode.content.split(/\n/);
                return lines;
            })
            .catch(() => {
                InspectorTest.fail("Failed to load script content.");
                InspectorTest.completeTest();
            });
    }

    window.loadMainPageContent = function() {
        return loadLinesFromSourceCode(WI.networkManager.mainFrame.mainResource);
    }

    window.setBreakpointsOnLinesWithBreakpointComment = function(resource) {
        if (!resource)
            resource = WI.networkManager.mainFrame.mainResource;

        function createLocation(resource, lineNumber, columnNumber) {
            return {url: resource.url, lineNumber, columnNumber};
        }

        let promises = [];

        for (let i = 0; i < lines.length; ++i) {
            let line = lines[i];
            if (/BREAKPOINT/.test(line)) {
                let lastPathComponent = parseURL(resource.url).lastPathComponent;
                InspectorTest.log(`Setting Breakpoint: ${lastPathComponent}:${i}:0`);
                promises.push(DebuggerAgent.setBreakpointByUrl.invoke({url: resource.url, lineNumber: i, columnNumber: 0}));
            }
        }

        return Promise.all(promises);
    }

    window.logResolvedBreakpointLinesWithContext = function(inputLocation, resolvedLocation, context) {
        if (resolvedLocation.sourceCode !== linesSourceCode && !WI.networkManager.mainFrame.mainResource.scripts.includes(resolvedLocation.sourceCode)) {
            InspectorTest.log("--- Source Unavailable ---");
            return;
        }

        InspectorTest.assert(inputLocation.lineNumber <= resolvedLocation.lineNumber, "Input line number should always precede resolve location line number.");
        InspectorTest.assert(inputLocation.lineNumber !== resolvedLocation.lineNumber || inputLocation.columnNumber <= resolvedLocation.columnNumber, "Input position should always precede resolve position.");

        const inputCaret = "#";
        const resolvedCaret = "|";

        let startLine = inputLocation.lineNumber - context;
        let endLine = resolvedLocation.lineNumber + context;
        for (let lineNumber = startLine; lineNumber <= endLine; ++lineNumber) {
            let lineContent = lines[lineNumber];
            if (typeof lineContent !== "string")
                continue;

            let hasInputLocation = lineNumber === inputLocation.lineNumber;
            let hasResolvedLocation = lineNumber === resolvedLocation.lineNumber;

            let prefix = "    ";
            if (hasInputLocation && hasResolvedLocation) {
                prefix = "-=> ";
                lineContent = insertCaretIntoStringAtIndex(lineContent, resolvedLocation.columnNumber, resolvedCaret);
                if (inputLocation.columnNumber !== resolvedLocation.columnNumber)
                    lineContent = insertCaretIntoStringAtIndex(lineContent, inputLocation.columnNumber, inputCaret);
            } else if (hasInputLocation) {
                prefix = " -> ";
                lineContent = insertCaretIntoStringAtIndex(lineContent, inputLocation.columnNumber, inputCaret);
            } else if (hasResolvedLocation) {
                prefix = " => ";
                lineContent = insertCaretIntoStringAtIndex(lineContent, resolvedLocation.columnNumber, resolvedCaret);
            }

            let number = lineNumber.toString().padStart(3);
            InspectorTest.log(`${prefix}${number}    ${lineContent}`);
        }
    }

    window.logLinesWithContext = function(location, context) {
        if (location.sourceCode !== linesSourceCode && !WI.networkManager.mainFrame.mainResource.scripts.includes(location.sourceCode)) {
            InspectorTest.log("--- Source Unavailable ---");
            return;
        }

        let startLine = location.lineNumber - context;
        let endLine = location.lineNumber + context;
        for (let lineNumber = startLine; lineNumber <= endLine; ++lineNumber) {
            let lineContent = lines[lineNumber];
            if (typeof lineContent !== "string")
                continue;

            let active = lineNumber === location.lineNumber;
            let prefix = active ? " -> " : "    ";
            let number = lineNumber.toString().padStart(3);
            lineContent = active ? insertCaretIntoStringAtIndex(lineContent, location.columnNumber) : lineContent;
            InspectorTest.log(`${prefix}${number}    ${lineContent}`);
        }
    }

    window.logPauseLocation = function() {
        let callFrame = WI.debuggerManager.activeCallFrame;
        let name = callFrame.functionName || "<anonymous>";
        let location = callFrame.sourceCodeLocation;
        let line = location.lineNumber + 1;
        let column = location.columnNumber + 1;
        InspectorTest.log(`PAUSE AT ${name}:${line}:${column}`);
        logLinesWithContext(location, 3);
        InspectorTest.log("");
    }

    let suite;
    let currentSteps = [];

    window.step = function(type) {
        switch (type) {
        case "in":
            InspectorTest.log("ACTION: step-in");
            WI.debuggerManager.stepInto();
            break;
        case "over":
            InspectorTest.log("ACTION: step-over");
            WI.debuggerManager.stepOver();
            break;
        case "out":
            InspectorTest.log("ACTION: step-out");
            WI.debuggerManager.stepOut();
            break;
        case "resume":
            InspectorTest.log("ACTION: resume");
            WI.debuggerManager.resume();
            break;
        default:
            InspectorTest.fail("Unhandled step.");
            WI.debuggerManager.resume();
            break;
        }
    }

    window.initializeSteppingTestSuite = function(testSuite) {
        suite = testSuite;
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.CallFramesDidChange, (event) => {
            if (!WI.debuggerManager.activeCallFrame)
                return;
            logPauseLocation();
            step(currentSteps.shift());
        });
    }

    window.addSteppingTestCase = function({name, description, expression, steps, pauseOnAllException, setup, teardown}) {
        suite.addTestCase({
            name, description, setup, teardown,
            test(resolve, reject) {
                // Setup.
                currentSteps = steps;
                InspectorTest.assert(steps[steps.length - 1] === "resume", "The test should always resume at the end to avoid timeouts.");
                WI.debuggerManager.allExceptionsBreakpoint.disabled = pauseOnAllException ? false : true;

                // Trigger entry and step through it.
                InspectorTest.evaluateInPage(expression);
                InspectorTest.log(`EXPRESSION: ${expression}`);
                InspectorTest.log(`STEPS: ${steps.join(", ")}`);
                WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Paused, (event) => {
                    InspectorTest.log(`PAUSED (${WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target).pauseReason})`);
                });
                WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Resumed, (event) => {
                    InspectorTest.log("RESUMED");
                    InspectorTest.expectThat(steps.length === 0, "Should have used all steps.");
                    resolve();
                });
            }
        });
    }
});

if (!window.testRunner) {
    window.addEventListener("load", () => {
        for (let property in window) {
            if (property.startsWith("entry")) {
                let button = document.body.appendChild(document.createElement("button"));
                button.textContent = property;
                button.onclick = () => { setTimeout(window[property]); };
                document.body.appendChild(document.createElement("br"));
            }
        }
    });
}
