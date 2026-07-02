onmessage = async (event) => {
    if (event.data === "get-client") {
        const clients = await self.clients.matchAll();
        if (!clients.length) {
            event.source.postMessage("no client");
            return;
        }
        event.source.postMessage({ id: clients[0].id, url: clients[0].url });
        return;
    }

    if (event.data && event.data.navigate) {
        const clients = await self.clients.matchAll();
        if (!clients.length) {
            event.source.postMessage("failed, no clients");
            return;
        }
        await clients[0].navigate(event.data.navigate).then((client) => {
            event.source.postMessage(client ? { id: client.id, url: client.url } : "none");
        }, (e) => {
            event.source.postMessage("failed");
        });
        return;
    }
};
