let wasmInstances = [];

addEventListener("message", async (event) => {
    try {
        let {instance} = await WebAssembly.instantiateStreaming(fetch(event.data));
        wasmInstances.push(instance);
        postMessage({result: "instantiated"});
    } catch (error) {
        postMessage({error: String(error)});
    }
});
