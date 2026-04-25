TestPage.registerInitializer(() => {
    InspectorTest.URLBreakpoint = {};

    InspectorTest.URLBreakpoint.teardown = function(resolve, reject) {
        WI.domDebuggerManager.allRequestsBreakpoint?.remove();

        for (let breakpoint of WI.domDebuggerManager.urlBreakpoints)
            breakpoint.remove();

        resolve();
    };

    InspectorTest.URLBreakpoint.createBreakpoint = function(type, url) {
        return InspectorTest.URLBreakpoint.addBreakpoint(new WI.URLBreakpoint(type, url));
    };

    InspectorTest.URLBreakpoint.addBreakpoint = function(breakpoint) {
        if (breakpoint.url)
            InspectorTest.log(`Adding "${breakpoint.type + (breakpoint.url ? ":" + breakpoint.url : "")}" URL Breakpoint...`);
        else
            InspectorTest.log(`Adding All Requests URL Breakpoint...`);

        breakpoint.disabled = false;
        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.URLBreakpointAdded)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Added Breakpoint should be expected object.");
                InspectorTest.assert(!event.data.breakpoint.disabled, "Breakpoint should not be disabled initially.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.addURLBreakpoint(breakpoint);
        });
    };

    InspectorTest.URLBreakpoint.request = function(api) {
        let url = `resources/data${api}.json`;
        InspectorTest.log(`Request ${api} "${url}"...`);
        return InspectorTest.evaluateInPage(`loadResource${api}("${url}")`);
    };
});
