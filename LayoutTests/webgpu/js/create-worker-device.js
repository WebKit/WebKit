async function tryCreateDevice() {
    if (!navigator.gpu)
        postMessage(false);

    const adapter = await navigator.gpu.requestAdapter();
    await adapter.requestDevice();

    postMessage(true);
}

tryCreateDevice();
