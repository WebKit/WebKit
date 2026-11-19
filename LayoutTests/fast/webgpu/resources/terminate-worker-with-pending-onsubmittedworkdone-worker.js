function shaderCode(index)
{
    return `
        @group(0) @binding(0) var<storage, read_write> values: array<f32>;
        fn f${index}(x: f32) -> f32 {
            var result = x;
            for (var i = 0; i < 16; i++) {
                result = sin(result) * cos(result) + f32(i) * ${index}.0;
            }
            return result;
        }
        @compute @workgroup_size(1) fn main(@builtin(global_invocation_id) id: vec3u) {
            values[id.x] = f${index}(values[id.x]);
        }
    `;
}

onmessage = async () => {
    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    const buffer = device.createBuffer({ size: 4, usage: GPUBufferUsage.COPY_DST });
    const data = new Uint8Array(4);
    const deadline = performance.now() + 5000;
    for (let i = 0; performance.now() < deadline; ++i) {
        device.createShaderModule({ code: shaderCode(i) });
        device.queue.writeBuffer(buffer, 0, data);
        device.queue.onSubmittedWorkDone();
        device.queue.onSubmittedWorkDone();
        if (i == 100)
            postMessage('started');
    }
};
