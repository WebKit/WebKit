self.addEventListener("message", async (event) => {
    try { 
        if (event.data !== "TESTCACHE") {
             event.source.postMessage("FAIL: Unknown message");
        }
        var keys = await self.caches.keys();
        event.source.postMessage(keys.length === 0 ? "PASS" : "FAIL: caches is not empty, got: " + JSON.stringify(keys));
    } catch (e) {
        event.source.postMessage("FAIL: received exception " + e);
    }
});

