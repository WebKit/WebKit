self.addEventListener("message", function(e) {
    let client = e.source;

    client.postMessage("client.url: " + client.url);
    client.postMessage("client.frameType: " + client.frameType);
    client.postMessage("client.type: " + client.type);
    client.postMessage("client.id is non empty: " + (client.id.length ? "YES" : "NO"));

    client.postMessage("DONE");
});
