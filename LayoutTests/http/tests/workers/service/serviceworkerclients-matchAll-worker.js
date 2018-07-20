function matchAllPromise1()
{
    return self.clients.matchAll().then((clients) => {
        return clients.length === 0 ? "PASS" : "FAIL: expected no matched client, got " + clients.length;
    }, (e) => {
        return "FAIL: matchAll 1 rejected with " + e;
    });
}

var matchedClients;
matchAllPromise2 = self.clients.matchAll({ includeUncontrolled : true }).then((c) => {
    matchedClients = c;
    return matchedClients.length === 1 ? "PASS" : "FAIL: expected one matched client, got " + matchedClients.length;
}, (e) => {
    return "FAIL: matchAll 2 rejected with " + e;
});

async function doTestAfterMessage(event)
{
    try {
        if (event.data !== "start") {
            event.source.postMessage("FAIL: received unexpected message from client");
            return;
        }

        let tries = 0;
        do {
            if (tries)
                await waitFor(50);
            result = await matchAllPromise1();
        } while (result !== "PASS" && ++tries <= 20);

        if (result !== "PASS") {
            event.source.postMessage(result);
            return;
        }

        var result = await matchAllPromise2;
        if (result !== "PASS") {
            event.source.postMessage(result);
            return;
        }
        event.source.postMessage("PASS");
    } catch (e) {
        event.source.postMessage("FAIL: received exception " + e);
    }
}

self.addEventListener("message", doTestAfterMessage);
