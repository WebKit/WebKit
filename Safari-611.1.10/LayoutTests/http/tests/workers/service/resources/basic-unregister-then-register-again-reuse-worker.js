self.addEventListener("message", function(event) {
    event.waitUntil(new Promise((resolve, reject) => {
        setTimeout(() => {
            resolve();
        }, 30000);
    }));
    client = event.source;
    setTimeout(function() {
        client.postMessage("ExtendedLifetime");
    }, 0);
});
