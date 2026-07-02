onconnect = async (e) => { 
    const  port = e.ports[0];
    try {
        const registration = await navigator.serviceWorker.register("../skipFetchEvent-worker.js", { scope : '' });
        activeWorker = registration.active;
        if (activeWorker) {
            port.postMessage('already active');
            return;
        }
        activeWorker = registration.installing;
        await new Promise(resolve => {
            activeWorker.addEventListener('statechange', () => {
                if (activeWorker.state === "activated")
                    resolve();
            });
        });
        port.postMessage('registration is now active');
    } catch (e) {
        port.postMessage('error: '+ e);
    }
}
