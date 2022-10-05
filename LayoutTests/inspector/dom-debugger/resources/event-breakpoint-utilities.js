TestPage.registerInitializer(() => {
    InspectorTest.EventBreakpoint = {};

    InspectorTest.EventBreakpoint.teardown = function(resolve, reject) {
        WI.domDebuggerManager.allAnimationFramesBreakpoint?.remove();
        WI.domDebuggerManager.allIntervalsBreakpoint?.remove();
        WI.domDebuggerManager.allListenersBreakpoint?.remove();
        WI.domDebuggerManager.allTimeoutsBreakpoint?.remove();

        for (let breakpoint of WI.domDebuggerManager.listenerBreakpoints)
            breakpoint.remove();

        resolve();
    };

    InspectorTest.EventBreakpoint.failOnPause = function(resolve, reject, pauseReason, eventName, message) {
        let paused = false;

        let listener = WI.debuggerManager.singleFireEventListener(WI.DebuggerManager.Event.Paused, (event) => {
            paused = true;

            let targetData = WI.debuggerManager.dataForTarget(WI.debuggerManager.activeCallFrame.target);
            InspectorTest.assert(targetData.pauseReason === pauseReason, `Pause reason should be "${pauseReason}".`);
            if (targetData.pauseData.eventName)
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

    InspectorTest.EventBreakpoint.createBreakpoint = function(type, options = {}) {
        if (type !== WI.EventBreakpoint.Type.Listener)
            options.eventName = null;

        InspectorTest.log(`Creating "${options.eventName || type}" Event Breakpoint...`);
        return InspectorTest.EventBreakpoint.addBreakpoint(new WI.EventBreakpoint(type, options));
    };

    InspectorTest.EventBreakpoint.addBreakpoint = function(breakpoint) {
        InspectorTest.log(`Adding "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

        breakpoint.disabled = false;
        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointAdded)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Added Breakpoint should be expected object.");
                InspectorTest.assert(!event.data.breakpoint.disabled, "Breakpoint should not be disabled initially.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.addEventBreakpoint(breakpoint);
        });
    };

    InspectorTest.EventBreakpoint.removeBreakpoint = function(breakpoint) {
        InspectorTest.log(`Removing "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.EventBreakpointRemoved)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Removed Breakpoint should be expected object.");
                InspectorTest.assert(!WI.domDebuggerManager.listenerBreakpoints.includes(breakpoint), "Breakpoint should not be in the list of breakpoints.");
                resolve(breakpoint);
            });

            breakpoint.remove();
        });
    };

    InspectorTest.EventBreakpoint.disableBreakpoint = function(breakpoint) {
        InspectorTest.log(`Disabling "${breakpoint.type + (breakpoint.eventName ? ":" + breakpoint.eventName : "")}" Event Breakpoint...`);

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
