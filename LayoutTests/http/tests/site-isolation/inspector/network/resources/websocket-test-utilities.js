TestPage.registerInitializer(function() {

// Resolves with the next WebSocketResource added to any frame whose URL contains `urlSubstring`.
window.awaitWebSocketResource = function awaitWebSocketResource(urlSubstring) {
    return new Promise((resolve) => {
        let listener = WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, (event) => {
            let resource = event.data.resource;
            if (!(resource instanceof WI.WebSocketResource) || !resource.url.includes(urlSubstring))
                return;
            WI.Frame.removeEventListener(WI.Frame.Event.ResourceWasAdded, listener);
            resolve(resource);
        });
    });
};

// Resolves once the resource reaches the Closed state. The frames for a socket arrive before its close.
window.awaitWebSocketClosed = function awaitWebSocketClosed(resource) {
    return new Promise((resolve) => {
        if (resource.readyState === WI.WebSocketResource.ReadyState.Closed) {
            resolve();
            return;
        }
        let listener = resource.addEventListener(WI.WebSocketResource.Event.ReadyStateChanged, () => {
            if (resource.readyState !== WI.WebSocketResource.ReadyState.Closed)
                return;
            resource.removeEventListener(WI.WebSocketResource.Event.ReadyStateChanged, listener);
            resolve();
        });
    });
};

// Resolves once the resource reaches the Open state.
window.awaitWebSocketOpen = function awaitWebSocketOpen(resource) {
    return new Promise((resolve) => {
        if (resource.readyState === WI.WebSocketResource.ReadyState.Open) {
            resolve();
            return;
        }
        let listener = resource.addEventListener(WI.WebSocketResource.Event.ReadyStateChanged, () => {
            if (resource.readyState !== WI.WebSocketResource.ReadyState.Open)
                return;
            resource.removeEventListener(WI.WebSocketResource.Event.ReadyStateChanged, listener);
            resolve();
        });
    });
};

// Counts the WebSocket resources for `url` across the whole frame tree, to catch duplicate rows.
window.countWebSocketResourcesForURL = function countWebSocketResourcesForURL(url) {
    let count = 0;
    for (let frame of WI.networkManager.frames) {
        for (let resource of frame.resourceCollection) {
            if (resource instanceof WI.WebSocketResource && resource.url === url)
                ++count;
        }
    }
    return count;
};

window.logWebSocketFrames = function logWebSocketFrames(resource) {
    for (let frame of resource.frames) {
        let direction = frame.isOutgoing ? "outgoing" : "incoming";
        let data = frame.opcode === WI.WebSocketResource.OpCodes.TextFrame ? ` "${frame.dataForTest}"` : "";
        InspectorTest.log(`Frame: ${direction} opcode ${frame.opcode}${data}`);
    }
};

});
