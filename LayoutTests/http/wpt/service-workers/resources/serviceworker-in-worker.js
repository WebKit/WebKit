async function registerServiceWorker()
{
    try {
        const registration = await navigator.serviceWorker.register("../skipFetchEvent-worker.js", { scope : '' });
        activeWorker = registration.active;
        if (activeWorker) {
            self.postMessage('already active');
            return;
        }
        activeWorker = registration.installing;
        await new Promise(resolve => {
            activeWorker.addEventListener('statechange', () => {
                if (activeWorker.state === "activated")
                    resolve();
            });
        });
        self.postMessage('registration is now active');
    } catch (e) {
        self.postMessage('error: '+ e);
    }
}
registerServiceWorker();
