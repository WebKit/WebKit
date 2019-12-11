let receivedInstallEvent = false;

self.addEventListener("install", (event) => {
    receivedInstallEvent = true;
});

self.addEventListener("message", (event) => {
    event.source.postMessage(receivedInstallEvent);
});

