var client = null;
self.addEventListener("message", async (event) => {
    try {
        var client = event.source;

        if (!(client instanceof WindowClient)) {
            event.source.postMessage("FAIL: client source is not a WindowClient");
            return;
        }

        var retrievedClient = await self.clients.get(client.id);
        if (!retrievedClient) {
            event.source.postMessage("FAIL: did not retrieve any client through self.clients.get");
            return;
        }

        if (retrievedClient.id !== client.id) {
            event.source.postMessage("FAIL: client id is different from retrieved client id through self.clients.get: " + client.id + " / " + retrievedClient.id);
            return;
        }

        if (retrievedClient !== client) {
            event.source.postMessage("FAIL: client is different from the one retrieved through self.clients.get");
            return;
        }

        var badIds = [client.id + "0", "0-0", "0-1", "1-0", "0-", "-", "-0"];
        for (id in badIds) {
            retrievedClient = await self.clients.get(id);
            if (!!retrievedClient) {
                event.source.postMessage("FAIL: retrieved client with bad id " + id + " should be null");
                return;
            }
        }

        event.source.postMessage("PASS");
    } catch (e) {
        event.source.postMessage("FAIL: received exception " + e);
    }
});

