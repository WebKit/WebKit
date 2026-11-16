// Tests whether WebGPU works in the main thread or in a worker. The pages include this script and call
// one of the run*Test() functions, and the workers run this script and post the state to the page.

// Writes a value to a WebGPU buffer and checks that it reads back.
async function webgpuState()
{
    try {
        if (!self.navigator.gpu)
            return "fails";
        var adapter = await navigator.gpu.requestAdapter();
        if (!adapter)
            return "fails";
        var device = await adapter.requestDevice();
        var buffer = device.createBuffer({ size: 4, usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST });
        device.queue.writeBuffer(buffer, 0, new Uint32Array([1234]));
        await buffer.mapAsync(GPUMapMode.READ);
        var value = new Uint32Array(buffer.getMappedRange())[0];
        buffer.unmap();
        device.destroy();
        return value == 1234 ? "works" : "fails";
    } catch (e) {
        return "fails";
    }
}

if (self.SharedWorkerGlobalScope && self instanceof SharedWorkerGlobalScope)
    self.onconnect = event => webgpuState().then(state => event.ports[0].postMessage(state));
else if (self.ServiceWorkerGlobalScope && self instanceof ServiceWorkerGlobalScope)
    self.onmessage = event => webgpuState().then(state => event.source.postMessage(state));
else if (self.DedicatedWorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope)
    webgpuState().then(state => self.postMessage(state));

var workerScript = "resources/webgpu-in-workers.js";
var state;

function checkState(contextState)
{
    state = contextState;
    shouldBeEqualToString("state", "works");
    finishJSTest();
}

async function runMainThreadTest()
{
    jsTestIsAsync = true;
    checkState(await webgpuState());
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
