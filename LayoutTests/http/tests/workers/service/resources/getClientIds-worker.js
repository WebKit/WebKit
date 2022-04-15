self.addEventListener("message", async (event) => {
    source = event.source;
    const matchedClients = await clients.matchAll({ includeUncontrolled : true, type: 'all' })
    let data = { ids: [], workerCount: 0 };
    for (let client of matchedClients) {
        if (client.type === 'worker')
            data.workerCount++;
        else
            data.ids.push(self.internals.serviceWorkerClientInternalIdentifier(client));
    }
    source.postMessage(data);
});
