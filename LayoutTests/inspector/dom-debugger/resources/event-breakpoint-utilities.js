TestPage.registerInitializer(() => {
    InspectorTest.EventBreakpoint = {};

    InspectorTest.EventBreakpoint.teardown = function(resolve, reject) {
        let breakpoints = WI.domDebuggerManager.eventBreakpoints;
        for (let breakpoint of breakpoints)
            WI.domDebuggerManager.removeEventBreakpoint(breakpoint);

        resolve();
    };

    InspectorTest.EventBreakpoint.failOnPause = function(resolve, reject, pauseReason, eventName, message) {
        let paused = false;

        let listener = WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Paused, (event) => {
            paused = true;

            let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
            InspectorTest.assert(targetData.pauseReason === pauseReason, `Pause reason should be "${pauseReason}".`);
            InspectorTest.assert(targetData.pauseData.eventName === eventName, `Pause data eventName should be "${eventName}".`);

            InspectorTest.fail(message);
            logActiveStackTrace();

            WI.debuggerManager.resume()
            .catch(reject);
        });

        InspectorTest.singleFireEventListener("TestPage-" + eventName, (event) => {
            if (!paused) {
                WI.debuggerManager.removeEventListener(WI.DebuggerManager.Event.Paused, listener);

                InspectorTest.pass(message);
            }

            resolve();
        });
    };

    InspectorTest.EventBreakpoint.addBreakpoint = function(type, eventName) {
        InspectorTest.log(`Adding "${eventName}" Event Breakpoint...`);

        return new Promise((resolve, reject) => {
            let breakpoint = new WI.EventBreakpoint(type, eventName);

            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointAdded)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint.type === type, `Breakpoint should be for expected type "${type}".`);
                InspectorTest.assert(event.data.breakpoint.eventName === eventName, `Breakpoint should be for expected event name "${eventName}".`);
                InspectorTest.assert(!event.data.breakpoint.disabled, "Breakpoint should not be disabled initially.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.addEventBreakpoint(breakpoint);
        });
    };

    InspectorTest.EventBreakpoint.removeBreakpoint = function(breakpoint) {
        InspectorTest.log(`Removing "${breakpoint.eventName}" Event Breakpoint...`);

        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointRemoved)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Removed Breakpoint should be expected object.");
                InspectorTest.assert(!WI.domDebuggerManager.eventBreakpoints.includes(breakpoint), "Breakpoint should not be in the list of breakpoints.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.removeEventBreakpoint(breakpoint);
        });
    };

    InspectorTest.EventBreakpoint.disableBreakpoint = function(breakpoint) {
        InspectorTest.log(`Disabling "${breakpoint.eventName}" Event Breakpoint...`);

        breakpoint.disabled = true;
        return breakpoint;
    };

    InspectorTest.EventBreakpoint.awaitEvent = function(context, eventName) {
        return function() {
            InspectorTest.log(`Firing "${eventName}" on ${context}...`);
            return InspectorTest.evaluateInPage(`trigger_${eventName}()`);
        };
    };
});
