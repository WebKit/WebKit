// Tests whether WebGL works in the main thread or in a worker. The pages include this script and call
// one of the run*Test() functions, and the workers run this script and post the state to the page.

// Clears a WebGL context to green and checks that the pixel reads back.
function webglContextState(contextType)
{
    try {
        var gl = new OffscreenCanvas(1, 1).getContext(contextType);
        if (!gl)
            return "fails";
        gl.clearColor(0, 1, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);
        var pixel = new Uint8Array(4);
        gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
        return pixel.join(",") == "0,255,0,255" ? "works" : "fails";
    } catch (e) {
        return "fails";
    }
}

function webglState()
{
    return {
        webgl: webglContextState("webgl"),
        webgl2: webglContextState("webgl2")
    };
}

if (self.SharedWorkerGlobalScope && self instanceof SharedWorkerGlobalScope)
    self.onconnect = event => event.ports[0].postMessage(webglState());
else if (self.ServiceWorkerGlobalScope && self instanceof ServiceWorkerGlobalScope)
    self.onmessage = event => event.source.postMessage(webglState());
else if (self.DedicatedWorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope)
    self.postMessage(webglState());

var workerScript = "resources/webgl-in-workers.js";
var state;

function checkState(contextState)
{
    state = contextState;
    shouldBeEqualToString("state.webgl", "works");
    shouldBeEqualToString("state.webgl2", "works");
    finishJSTest();
}

function runMainThreadTest()
{
    jsTestIsAsync = true;
    checkState(webglState());
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
