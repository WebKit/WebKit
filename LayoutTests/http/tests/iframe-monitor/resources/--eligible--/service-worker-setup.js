async function setupServiceWorker(url, receiver) {
    try {
        const registration = await navigator.serviceWorker.register(url);
        console.log("Service Worker registered with scope:", registration.scope);
        const worker = registration.installing ?? registration.waiting ?? registration.active;
        await waitForState(worker, "activated");

        await navigator.serviceWorker.ready;
        console.log("Service Worker is ready and controlling the page.");

        // Handle messages from Service Worker
        navigator.serviceWorker.addEventListener('message', async (e) => {
            receiver(e.data);
        });

        return worker;
    } catch(error) {
        console.error("Service Worker registration failed:", error);
        return false;
    }
}

async function waitForState(worker, state)
{
    if (worker.state === state)
        return;

    return new Promise(function(resolve) {
        worker.addEventListener('statechange', () => {
            if (worker.state === state)
                resolve(state);
        });
    });
}
