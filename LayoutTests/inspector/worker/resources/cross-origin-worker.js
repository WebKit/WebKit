var passphrase = "worker-in-cross-origin-iframe";

self.addEventListener("message", (event) => {
    self.postMessage(passphrase);
});
