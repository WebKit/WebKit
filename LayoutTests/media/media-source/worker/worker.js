function logToMain(msg) {
    postMessage({topic: 'info', arg: msg});
}

function logErrorToMain(msg) {
    postMessage({topic: 'error', arg: msg});
}

onmessage = (e) => {
    try {
        const ms = new ManagedMediaSource();
        const handle = ms.handle;
        ms.onsourceopen = () => {
            logToMain("sourceopen event received");
        };
        // Transfer the MediaSourceHandle to the main thread for use in attaching to
        // the main thread media element that will play the content being buffered
        // here in the worker.
        postMessage({topic: 'handle', arg: handle}, [handle]);
    } catch (e) {
        logErrorToMain('MSE not supported');
    }
};
