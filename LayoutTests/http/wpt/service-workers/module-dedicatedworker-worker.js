async function doTest(event)
{
    if (event.data === "SET-FETCH") {
        self.receivedFetch = 0;
        self.addEventListener("fetch", (event) => {
            self.receivedFetch++;
        });
        event.source.postMessage("OK");
        return;
    }
    if (event.data === "GET-FETCH") {
        event.source.postMessage(self.receivedFetch);
        return;
    }
    if (event.data === "ping") {
        event.source.postMessage("pong");
        return;
    }
    event.source.postMessage("KO");
}

self.addEventListener("message", doTest);
