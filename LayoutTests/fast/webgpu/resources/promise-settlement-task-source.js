if (self.importScripts)
    importScripts('../../../resources/js-test.js');

description("Checks that each WebGPU promise settles from a task on the WebGPU task source, after the " +
            "tasks queued before it, and not directly when the GPU process replies or during the call.");

// The validation errors to generate before each call. Their uncapturederror events are tasks on the
// WebGPU task source, queued before the task that settles the promise of the call.
const errorCount = 20;

let eventCount = 0;
let listenerMicrotaskCount = 0;
let onUncapturedError = null;

function listenForUncapturedErrors(device)
{
    device.addEventListener('uncapturederror', () => {
        ++eventCount;
        if (onUncapturedError)
            onUncapturedError();
        // When the promise is settled from a task, this runs before its reactions. When it is settled
        // directly as the reply arrives, its reactions were queued first and run before this.
        queueMicrotask(() => ++listenerMicrotaskCount);
    });
}

function generateValidationErrors(device)
{
    // MAP_READ and MAP_WRITE cannot be combined.
    for (let i = 0; i < errorCount; ++i)
        device.createBuffer({ size: 4, usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.MAP_WRITE });
}

// Waits for the GPU process to handle the commands sent so far, so that their replies tend to arrive
// before this task ends.
function waitForGPUProcess()
{
    navigator.gpu.requestAdapter();
}

async function settle(promise)
{
    try {
        return { outcome: "resolved", value: await promise };
    } catch (error) {
        return { outcome: "rejected", value: error };
    }
}

// Generates errors, calls the entrypoint and checks that its promise settles after the uncapturederror
// events of the errors, and after the microtasks those events queue.
async function checkSettlesAfterEarlierEvents(name, device, call, expectedOutcome, isExpectedValue = () => true)
{
    eventCount = 0;
    listenerMicrotaskCount = 0;
    generateValidationErrors(device);
    const promise = call();
    waitForGPUProcess();
    const result = await settle(promise);
    const eventCountWhenSettled = eventCount;
    const listenerMicrotaskCountWhenSettled = listenerMicrotaskCount;
    onUncapturedError = null;

    if (result.outcome != expectedOutcome) {
        testFailed(`${name} ${result.outcome}: ${result.value}`);
        return;
    }
    if (!isExpectedValue(result.value)) {
        testFailed(`${name} ${result.outcome} with an unexpected value: ${result.value}`);
        return;
    }
    if (eventCountWhenSettled != errorCount || listenerMicrotaskCountWhenSettled != errorCount) {
        testFailed(`${name} ${result.outcome} after ${eventCountWhenSettled} of ${errorCount} earlier uncapturederror events and ${listenerMicrotaskCountWhenSettled} of their microtasks`);
        return;
    }
    testPassed(`${name} ${result.outcome} after the earlier uncapturederror events and their microtasks`);
}

// For the calls whose promise can settle before the call returns: checks that the promise has not settled
// after the microtasks queued by the call, and settles later.
async function checkSettlesInLaterTask(name, call, expectedOutcome, isExpectedValue = () => true)
{
    let settled = false;
    const promise = call();
    promise.then(() => settled = true, () => settled = true);
    for (let i = 0; i < 10; ++i)
        await null;
    const settledInSameTask = settled;
    const result = await settle(promise);

    if (result.outcome != expectedOutcome) {
        testFailed(`${name} ${result.outcome}: ${result.value}`);
        return;
    }
    if (!isExpectedValue(result.value)) {
        testFailed(`${name} ${result.outcome} with an unexpected value: ${result.value}`);
        return;
    }
    if (settledInSameTask) {
        testFailed(`${name} ${result.outcome} in the task that called it`);
        return;
    }
    testPassed(`${name} ${result.outcome} in a later task`);
}

const shaderCode = `
    @vertex fn vs() -> @builtin(position) vec4f { return vec4f(); }
    @fragment fn fs() -> @location(0) vec4f { return vec4f(); }
    @compute @workgroup_size(1) fn cs() { }
`;

async function main()
{
    await checkSettlesInLaterTask("requestAdapter()", () => navigator.gpu.requestAdapter(), "resolved",
        adapter => adapter instanceof GPUAdapter);
    await checkSettlesInLaterTask("requestAdapter() with an unknown feature level", () => navigator.gpu.requestAdapter({ featureLevel: "unknown" }), "resolved",
        adapter => adapter === null);

    const adapter = await navigator.gpu.requestAdapter();
    await checkSettlesInLaterTask("requestDevice()", () => adapter.requestDevice(), "resolved",
        device => device instanceof GPUDevice);
    const limitsAdapter = await navigator.gpu.requestAdapter();
    await checkSettlesInLaterTask("requestDevice() with a limit above the adapter's", () => limitsAdapter.requestDevice({ requiredLimits: { maxBindGroups: limitsAdapter.limits.maxBindGroups + 1 } }), "rejected",
        error => error.name == "OperationError");

    const device = await (await navigator.gpu.requestAdapter()).requestDevice();
    listenForUncapturedErrors(device);
    const module = device.createShaderModule({ code: shaderCode });

    await checkSettlesAfterEarlierEvents("createComputePipelineAsync()", device, () => device.createComputePipelineAsync({
        layout: 'auto',
        compute: { module, entryPoint: 'cs' },
    }), "resolved", pipeline => pipeline instanceof GPUComputePipeline);
    await checkSettlesAfterEarlierEvents("createComputePipelineAsync() with a missing entry point", device, () => device.createComputePipelineAsync({
        layout: 'auto',
        compute: { module, entryPoint: 'doesNotExist' },
    }), "rejected", error => error.reason == "validation");

    await checkSettlesAfterEarlierEvents("createRenderPipelineAsync()", device, () => device.createRenderPipelineAsync({
        layout: 'auto',
        vertex: { module, entryPoint: 'vs' },
        fragment: { module, entryPoint: 'fs', targets: [{ format: 'rgba8unorm' }] },
    }), "resolved", pipeline => pipeline instanceof GPURenderPipeline);
    await checkSettlesAfterEarlierEvents("createRenderPipelineAsync() with a missing entry point", device, () => device.createRenderPipelineAsync({
        layout: 'auto',
        vertex: { module, entryPoint: 'doesNotExist' },
        fragment: { module, entryPoint: 'fs', targets: [{ format: 'rgba8unorm' }] },
    }), "rejected", error => error.reason == "validation");

    await checkSettlesAfterEarlierEvents("popErrorScope() of a scope without errors", device, () => {
        device.pushErrorScope('validation');
        return device.popErrorScope();
    }, "resolved", error => error === null);
    await checkSettlesAfterEarlierEvents("popErrorScope() of a scope with an error", device, () => {
        device.pushErrorScope('validation');
        device.createBuffer({ size: 4, usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.MAP_WRITE });
        return device.popErrorScope();
    }, "resolved", error => error instanceof GPUValidationError);
    await checkSettlesAfterEarlierEvents("popErrorScope() without a scope", device, () => device.popErrorScope(), "rejected",
        error => error.name == "OperationError");

    // The buffer stays pending until the task that settles the promise.
    let mapStates = [];
    const buffer = device.createBuffer({ size: 4, usage: GPUBufferUsage.MAP_READ });
    await checkSettlesAfterEarlierEvents("mapAsync()", device, () => {
        mapStates = [];
        onUncapturedError = () => mapStates.push(buffer.mapState);
        return buffer.mapAsync(GPUMapMode.READ);
    }, "resolved", () => mapStates.every(state => state == "pending") && buffer.mapState == "mapped");
    buffer.unmap();
    await checkSettlesAfterEarlierEvents("mapAsync() with a mode the buffer cannot be mapped for", device, () => {
        mapStates = [];
        onUncapturedError = () => mapStates.push(buffer.mapState);
        device.pushErrorScope('validation');
        const promise = buffer.mapAsync(GPUMapMode.WRITE);
        device.popErrorScope();
        return promise;
    }, "rejected", error => error.name == "OperationError" && mapStates.every(state => state == "pending") && buffer.mapState == "unmapped");

    await checkSettlesAfterEarlierEvents("onSubmittedWorkDone()", device, () => device.queue.onSubmittedWorkDone(), "resolved",
        value => value === undefined);

    await checkSettlesAfterEarlierEvents("getCompilationInfo()", device, () => module.getCompilationInfo(), "resolved",
        info => info instanceof GPUCompilationInfo);

    const lostDevice = await (await navigator.gpu.requestAdapter()).requestDevice();
    listenForUncapturedErrors(lostDevice);
    const lost = lostDevice.lost;
    await checkSettlesAfterEarlierEvents("lost", lostDevice, () => {
        lostDevice.destroy();
        return lost;
    }, "resolved", info => info.reason == "destroyed");

    device.destroy();
    finishJSTest();
}

self.jsTestIsAsync = true;
main().catch(error => {
    testFailed(`${error}`);
    finishJSTest();
});
