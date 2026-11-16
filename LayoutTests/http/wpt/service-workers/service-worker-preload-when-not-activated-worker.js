oninstall = (event) => {
    if (self.port)
        return;
    event.waitUntil(new Promise(resolve => self.installingPromiseResolve = resolve));
}

onmessage = (event) => {
    if (event.data.port)
        self.port = event.data.port;
    if (self.installingPromiseResolve) {
        self.installingPromiseResolve();
        self.installingPromiseResolve = null;
    }
    if (self.activatingPromiseResolve) {
        self.activatingPromiseResolve();
        self.activatingPromiseResolve = null;
    }
};

onactivate = (event) => {
    event.waitUntil(new Promise(resolve => self.activatingPromiseResolve = resolve));
    self.port.postMessage("activating");
}

self.addEventListener('fetch', () => {
    if (self.port)
        self.port.postMessage("fetching");
});
