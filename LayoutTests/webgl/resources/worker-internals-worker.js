// Tests that a worker has the internals object with the WebGL functions when its script starts. The
// pages include this script and call one of the run*Test() functions, and the workers run this script
// and post the state to the page.

// Reads the internals object when the script starts, before any events.
function internalsState()
{
    var state = {
        internals: typeof self.internals,
        settings: "",
        simulateEventForWebGLContext: "",
        webglMaxDrawingBufferSize: "",
        requestedGPU: "",
        webglMaxDrawingBufferSizeResult: "",
        requestedGPUResult: ""
    };
    if (!self.internals)
        return state;
    state.settings = typeof internals.settings;
    state.simulateEventForWebGLContext = typeof internals.simulateEventForWebGLContext;
    state.webglMaxDrawingBufferSize = typeof internals.webglMaxDrawingBufferSize;
    state.requestedGPU = typeof internals.requestedGPU;
    var gl = new OffscreenCanvas(1, 1).getContext("webgl");
    if (!gl)
        return state;
    var size = internals.webglMaxDrawingBufferSize(gl);
    state.webglMaxDrawingBufferSizeResult = size.length == 2 && size[0] > 0 && size[1] > 0 ? "valid" : "invalid: " + size;
    state.requestedGPUResult = internals.requestedGPU(gl);
    return state;
}

if (self.SharedWorkerGlobalScope && self instanceof SharedWorkerGlobalScope) {
    var workerState = internalsState();
    self.onconnect = function(event) { event.ports[0].postMessage(workerState); };
} else if (self.DedicatedWorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope)
    self.postMessage(internalsState());

var workerScript = "resources/worker-internals-worker.js";
var state;

function checkState(workerState)
{
    state = workerState;
    shouldBeEqualToString("state.internals", "object");
    shouldBeEqualToString("state.settings", "undefined");
    shouldBeEqualToString("state.simulateEventForWebGLContext", "function");
    shouldBeEqualToString("state.webglMaxDrawingBufferSize", "function");
    shouldBeEqualToString("state.requestedGPU", "function");
    shouldBeEqualToString("state.webglMaxDrawingBufferSizeResult", "valid");
    shouldBeEqualToString("state.requestedGPUResult", "default");
    finishJSTest();
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
