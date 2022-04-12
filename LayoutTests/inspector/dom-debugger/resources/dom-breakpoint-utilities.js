TestPage.registerInitializer(() => {
    function handleDOMNodeDidChange(event) {
        let breakpoint = event.target;
        if (breakpoint.domNode)
            InspectorTest.log(`Breakpoint "domNode" set to "${breakpoint.domNode.displayName}".`);
        else
            InspectorTest.log(`Breakpoint "domNode" set to "null".`);
    };

    InspectorTest.DOMBreakpoint = {};

    InspectorTest.DOMBreakpoint.teardown = function(resolve, reject) {
        for (let breakpoint of WI.domDebuggerManager.domBreakpoints)
            breakpoint.remove();

        resolve();
    };

    InspectorTest.DOMBreakpoint.createBreakpoint = function(domNode, type) {
        let breakpoint = new WI.DOMBreakpoint(domNode, type);
        breakpoint.addEventListener(WI.DOMBreakpoint.Event.DOMNodeDidChange, handleDOMNodeDidChange);

        return InspectorTest.DOMBreakpoint.addBreakpoint(breakpoint);
    };

    InspectorTest.DOMBreakpoint.addBreakpoint = function(breakpoint) {
        InspectorTest.log(`Adding "${breakpoint.type}:${breakpoint.path}" DOM Breakpoint...`);

        breakpoint.disabled = false;
        return new Promise((resolve, reject) => {
            WI.domDebuggerManager.awaitEvent(WI.DOMDebuggerManager.Event.DOMBreakpointAdded)
            .then((event) => {
                InspectorTest.assert(event.data.breakpoint === breakpoint, "Added Breakpoint should be expected object.");
                InspectorTest.assert(!event.data.breakpoint.disabled, "Breakpoint should not be disabled initially.");
                resolve(breakpoint);
            });

            WI.domDebuggerManager.addDOMBreakpoint(breakpoint);
        });
    };

    InspectorTest.DOMBreakpoint.awaitQuerySelector = function(selector) {
        return new Promise((resolve, reject) => {
            WI.domManager.requestDocument((documentNode) => {
                if (!documentNode) {
                    reject();
                    return;
                }

                documentNode.querySelector(selector, (nodeId) => {
                    if (!nodeId) {
                        InspectorTest.fail("Selector returned no nodes.", selector);
                        reject();
                        return;
                    }

                    let node = WI.domManager.nodeForId(nodeId);
                    InspectorTest.assert(node, "Missing node for id.", nodeId);
                    if (!node) {
                        reject();
                        return;
                    }

                    resolve(node);
                });
            });
        });
    };

    // To prevent spurious `Breakpoint "domNode" set to "null".` logging upon removing breakpoints, we should remove the
    // event listener before the breakpoint has removed its DOM node.
    WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, (event) => {
        event.data.breakpoint.removeEventListener(WI.DOMBreakpoint.Event.DOMNodeDidChange, handleDOMNodeDidChange);
    });
});
