// Tests whether IOKit is blocked in the sandbox of the process of the main thread or of a worker. The
// pages include this script and call one of the run*Test() functions, and the workers run this script
// and post the state to the page.

function ioKitState()
{
    if (!self.internals)
        return "no internals";
    return internals.isIOKitAvailable() ? "available" : "blocked";
}

if (self.SharedWorkerGlobalScope && self instanceof SharedWorkerGlobalScope)
    self.onconnect = event => event.ports[0].postMessage(ioKitState());
else if (self.ServiceWorkerGlobalScope && self instanceof ServiceWorkerGlobalScope)
    self.onmessage = event => event.source.postMessage(ioKitState());
else if (self.DedicatedWorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope)
    self.postMessage(ioKitState());

var workerScript = "resources/iokit-blocked-in-workers.js";
var state;

function checkState(contextState)
{
    state = contextState;
    shouldBeEqualToString("state", "blocked");
    finishJSTest();
}

function runMainThreadTest()
{
    jsTestIsAsync = true;
    checkState(ioKitState());
}

function runDedicatedWorkerTest()
{
    jsTestIsAsync = true;
    var worker = new Worker(workerScript);
    worker.onmessage = event => checkState(event.data);
}

function runSharedWorkerTest()
{
    jsTestIsAsync = true;
    var worker = new SharedWorker(workerScript);
    worker.port.onmessage = event => checkState(event.data);
}

async function runServiceWorkerTest()
{
    jsTestIsAsync = true;
    var registration = await navigator.serviceWorker.register(workerScript);
    var worker = registration.active || registration.installing || registration.waiting;
    var result = new Promise(resolve => {
        navigator.serviceWorker.addEventListener("message", event => resolve(event.data), { once: true });
    });
    worker.postMessage("state");
    var workerState = await result;
    await registration.unregister();
    checkState(workerState);
}
