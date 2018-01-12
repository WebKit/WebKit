self.addEventListener("message", (event) => {
    source = event.source;
    clients.matchAll({ includeUncontrolled : true }).then(function(clientList) {
        source.postMessage(clientList.length);
    });
});
