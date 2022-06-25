async function doTest(event)
{
    if (!event.data.startsWith("TESTIDB")) {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }

    event.source.postMessage(!!self.indexedDB ? "PASS" : "FAIL: self.indexedDB is null or undefined");
}

self.addEventListener("message", doTest);
