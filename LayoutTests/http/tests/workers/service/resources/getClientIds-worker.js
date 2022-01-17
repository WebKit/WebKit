self.addEventListener("message", (event) => {
    source = event.source;
    clients.matchAll({ includeUncontrolled : true }).then(function(clients) {
        let ids = [];
        for (let client of clients)
            ids.push(self.internals.serviceWorkerClientInternalIdentifier(client));
        source.postMessage(ids);
    });
});
