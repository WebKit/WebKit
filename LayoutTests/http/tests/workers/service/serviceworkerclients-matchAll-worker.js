function waitFor(duration)
{
    return new Promise((resolve) => setTimeout(resolve, duration));
}

async function serviceWorkerClientIdentifierFromInternalIdentifier(internalIdentifier)
{
     const clients = await self.clients.matchAll({ includeUncontrolled : true });
     for (let client of clients) {
         if (internals.serviceWorkerClientInternalIdentifier(client) == internalIdentifier)
            return client.id;
     }
}

function checkClientNotInControlledClients(clientIdentifier)
{
    return self.clients.matchAll().then((clients) => {
        for (let client of clients) {
            if (client.id == clientIdentifier)
                return "FAIL: Client should not have matched";
        }
        return "PASS";
    }, (e) => {
        return "FAIL: First matchAll() request rejected with " + e;
    });
}

function checkClientInUncontrolledClients(clientIdentifier)
{
    return self.clients.matchAll({ includeUncontrolled : true }).then((clients) => {
        for (let client of clients) {
            if (client.id == clientIdentifier)
                return "PASS";
        }
        return "FAIL: Client should have matched with includeUncontrolled set to true";
    }, (e) => {
        return "FAIL: Second matchAll() request rejected with " + e;
    });
}

async function doTestAfterMessage(event)
{
    try {
        if (event.data.test === "checkNewClientObject") {
            const clients1 = await self.clients.matchAll({ includeUncontrolled : true });
            const clients2 = await self.clients.matchAll({ includeUncontrolled : true });
            if (!clients1.length || !clients2.length) {
                event.source.postMessage("no clients");
                return;
            }
            for (let client1 of clients1) {
                for (let client2 of clients2) {
                    if (client1 === client2) {
                        event.source.postMessage("FAIL: reusing client objects");
                        return;
                    }
                }
            }
            event.source.postMessage("PASS");
            return;
        }

        if (event.data.test !== "checkClientIsUncontrolled") {
            event.source.postMessage("FAIL: received unexpected message from client");
            return;
        }

        const clientIdentifier = await serviceWorkerClientIdentifierFromInternalIdentifier(event.data.identifier);

        let result = await checkClientNotInControlledClients(clientIdentifier);
        if (result !== "PASS") {
            event.source.postMessage(result);
            return;
        }

        let tries = 0;
        do {
            if (tries)
                await waitFor(50);
            result = await checkClientInUncontrolledClients(clientIdentifier);
        } while (result !== "PASS" && ++tries <= 200);

        event.source.postMessage(result);
    } catch (e) {
        event.source.postMessage("FAIL: received exception " + e);
    }
}

self.addEventListener("message", doTestAfterMessage);
