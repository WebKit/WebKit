onmessage = async (event) => {
    let adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
        postMessage("NoAdapter");
        return;
    }

    let device = await adapter.requestDevice();
    if (event.data === "attribute")
        device.onuncapturederror = () => { };
    else
        device.addEventListener("uncapturederror", () => { });

    postMessage("DeviceReady");
};
