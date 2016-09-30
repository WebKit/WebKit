TestPage.registerInitializer(() => {
    let lines = [];

    // Switch back to String.prototype.padStart once this is fixed:
    // FIXME: <https://webkit.org/b/161944> stringProtoFuncRepeatCharacter will return `null` when it should not
    String.prototype.myPadStart = function(desired) {
        let length = this.length;
        if (length >= desired)
            return this;
        return " ".repeat(desired - length) + this;
    };

    function insertCaretIntoStringAtIndex(str, index) {
        return str.slice(0, index) + "|" + str.slice(index);
    }

    function logLinesWithContext(location, context) {
        if (!WebInspector.frameResourceManager.mainFrame.mainResource.scripts.includes(location.sourceCode)) {
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
            let number = lineNumber.toString().myPadStart(3);
            lineContent = active ? insertCaretIntoStringAtIndex(lineContent, location.columnNumber) : lineContent;
            InspectorTest.log(`${prefix}${number}    ${lineContent}`);
        }
    }

    window.logPauseLocation = function() {
        let callFrame = WebInspector.debuggerManager.activeCallFrame;
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
            WebInspector.debuggerManager.stepInto();
            break;
        case "over":
            InspectorTest.log("ACTION: step-over");
            WebInspector.debuggerManager.stepOver();
            break;
        case "out":
            InspectorTest.log("ACTION: step-out");
            WebInspector.debuggerManager.stepOut();
            break;
        case "resume":
            InspectorTest.log("ACTION: resume");
            WebInspector.debuggerManager.resume();
            break;
        default:
            InspectorTest.fail("Unhandled step.");
            WebInspector.debuggerManager.resume();
            break;
        }
    }

    window.initializeSteppingTestSuite = function(testSuite) {
        suite = testSuite;
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.CallFramesDidChange, (event) => {
            if (!WebInspector.debuggerManager.activeCallFrame)
                return;
            logPauseLocation();
            step(currentSteps.shift());
        });
    }

    window.addSteppingTestCase = function({name, description, expression, steps, pauseOnAllException}) {
        suite.addTestCase({
            name, description,
            test(resolve, reject) {
                // Setup.
                currentSteps = steps;
                InspectorTest.assert(steps[steps.length - 1] === "resume", "The test should always resume at the end to avoid timeouts.");
                WebInspector.debuggerManager.allExceptionsBreakpoint.disabled = pauseOnAllException ? false : true;

                // Trigger entry and step through it.
                InspectorTest.evaluateInPage(expression);
                InspectorTest.log(`EXPRESSION: ${expression}`);
                InspectorTest.log(`STEPS: ${steps.join(", ")}`);
                WebInspector.debuggerManager.singleFireEventListener(WebInspector.DebuggerManager.Event.Paused, (event) => {
                    InspectorTest.log(`PAUSED (${WebInspector.debuggerManager.pauseReason})`);
                });
                WebInspector.debuggerManager.singleFireEventListener(WebInspector.DebuggerManager.Event.Resumed, (event) => {
                    InspectorTest.log("RESUMED");
                    InspectorTest.expectThat(steps.length === 0, "Should have used all steps.");
                    resolve();
                });
            }
        });
    }

    window.loadMainPageContent = function() {
        return WebInspector.frameResourceManager.mainFrame.mainResource.requestContent()
            .then((content) => {
                lines = WebInspector.frameResourceManager.mainFrame.mainResource.content.split(/\n/);
            })
            .catch(() => {
                InspectorTest.fail("Failed to load page content.");
                InspectorTest.completeTest();
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
