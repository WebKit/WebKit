// Tests whether accelerated 2D canvases work in the main thread or in a worker. The pages include this
// script and call one of the run*Test() functions, and the workers run this script and post the state
// to the page.

// Draws to a 2D canvas that is forced to the accelerated rendering mode, and checks that the canvas is
// accelerated and that the pixels read back.
function canvasState()
{
    var state = { renderingMode: "", drawing: "fails" };
    try {
        var size = 64;
        var context = new OffscreenCanvas(size, size).getContext("2d", { renderingModeForTesting: "Accelerated" });
        if (!context)
            return state;
        context.fillStyle = "rgb(255, 0, 0)";
        context.fillRect(0, 0, size, size);
        context.fillStyle = "rgb(0, 255, 0)";
        context.fillRect(0, 0, size / 2, size / 2);
        var topLeft = context.getImageData(0, 0, 1, 1).data.join(",");
        var bottomRight = context.getImageData(size - 1, size - 1, 1, 1).data.join(",");
        state.drawing = topLeft == "0,255,0,255" && bottomRight == "255,0,0,255" ? "works" : "fails";
        state.renderingMode = String(context.getEffectiveRenderingModeForTesting());
    } catch (e) {
        state.renderingMode = "exception: " + e;
    }
    return state;
}

if (self.SharedWorkerGlobalScope && self instanceof SharedWorkerGlobalScope)
    self.onconnect = event => event.ports[0].postMessage(canvasState());
else if (self.ServiceWorkerGlobalScope && self instanceof ServiceWorkerGlobalScope)
    self.onmessage = event => event.source.postMessage(canvasState());
else if (self.DedicatedWorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope)
    self.postMessage(canvasState());

var workerScript = "resources/canvas-2d-accelerated-in-workers.js";
var state;

function checkState(contextState)
{
    state = contextState;
    shouldBeEqualToString("state.renderingMode", "Accelerated");
    shouldBeEqualToString("state.drawing", "works");
    finishJSTest();
}

function runMainThreadTest()
{
    jsTestIsAsync = true;
    checkState(canvasState());
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
