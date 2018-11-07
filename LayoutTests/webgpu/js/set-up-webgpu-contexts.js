'use strict';

let context, adapter, defaultDevice;

function runWebGPUTests(tests) {
    runWebGPUTestsOnCanvas(document.createElement("canvas"), tests);
}

async function runWebGPUTestsOnCanvas(canvas, tests) {
    try {
        await setUpContexts(canvas);

        for (let test of tests)
            test();

        debug("All tests complete.");
    } catch (error) {
        console.error(error);
        testFailed(`[${error}]: See console!`);
    }
}

async function setUpContexts(canvas) {
    context = canvas.getContext("webgpu");
    if (!context)
        testFailed("Could not create WebGPU context!");

    shouldBeDefined(window.webgpu);
    
    // FIXME: requestAdapter should take a WebGPUAdapterDescriptor.
    adapter = await window.webgpu.requestAdapter({});
    if (!adapter) {
        testFailed("Could not create default WebGPUAdapter!")
        return;
    }

    // FIXME: requestDevice should take a WebGPUDeviceDescriptor.
    defaultDevice = adapter.createDevice();
    if (!defaultDevice) {
        testFailed("Could not create WebGPUDevice!");
        return;
    }

    // FIXME: Default to "B8G8R8A8Unorm" format for now.
    context.configure({ device: defaultDevice, width: canvas.width, height: canvas.height });
}