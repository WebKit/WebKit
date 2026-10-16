// Shared driver for the WebGPU bind group benchmarks.
//
// Each run draws 1024 tiles of 8x8 pixels into a 256x256 rgba8unorm texture in one render pass.
// Every draw binds a 32-byte slice of one shared uniform buffer, at its own 256-byte offset, that
// holds the tile's position and color. The bind group is visible to the vertex and the fragment
// stage. The two benchmarks differ only in where the bind groups come from:
//
//  - "createPerDraw" creates a new bind group for every draw, the way ANGLE's WebGPU backend does
//    for every draw after a uniform change. No buffer is created per draw, so the difference to
//    "reusePrebuilt" is the cost of creating bind groups.
//  - "reusePrebuilt" uses 1024 bind groups created once.
//
// The protocol is that of PerformanceTests/WebGL/resources/webgl-bench.js:
//
//  1. Setup, then two runs, each checked by a 1-pixel probe and a full readback.
//  2. One warm-up and 10 measured iterations. Each iteration does runs until 100 milliseconds
//     have passed, and at least one, and reports runs per second. Each run ends by copying one
//     pixel into a buffer and mapping it, which waits for the GPU, and the pixel is checked.
//  3. A full readback check of the last run, before the last iteration's value is reported.
//
// Every run n writes colors derived from n, so dropped or stale work fails the checks. An
// incorrect result does not stop the benchmark; the failed checks are printed as "FAIL:" lines
// after the results. Other errors, such as a WebGPU validation error, stop the benchmark.

(function () {
"use strict";

const size = 256;
const tileSize = 8;
const tilesPerRow = size / tileSize;
const drawCount = tilesPerRow * tilesPerRow;
const uniformStride = 256;
const iterationTime = 100;
const measuredIterationCount = 10;

const shaderSource = `
struct Tile {
    position: vec2f,
    color: vec4f,
};
@group(0) @binding(0) var<uniform> tile: Tile;

@vertex fn vertexMain(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    var corners = array<vec2f, 6>(vec2f(0, 0), vec2f(1, 0), vec2f(0, 1), vec2f(0, 1), vec2f(1, 0), vec2f(1, 1));
    let pixel = corners[index] * ${tileSize}.0 + tile.position;
    // Rows of the texture go down, so that readback row r is tile row r / ${tileSize}.
    return vec4f(pixel.x / ${size / 2}.0 - 1.0, 1.0 - pixel.y / ${size / 2}.0, 0.0, 1.0);
}

@fragment fn fragmentMain() -> @location(0) vec4f {
    return tile.color;
}`;

function hash(value)
{
    value = Math.imul(value ^ (value >>> 16), 0x45d9f3b);
    value = Math.imul(value ^ (value >>> 16), 0x45d9f3b);
    return (value ^ (value >>> 16)) >>> 0;
}

// An opaque RGBA color, as bytes, for draw i of run n.
function colorInto(n, i, out)
{
    const value = hash(n * 65599 + i);
    out[0] = value & 0xff;
    out[1] = (value >>> 8) & 0xff;
    out[2] = (value >>> 16) & 0xff;
    out[3] = 255;
    return out;
}

class CheckFailure extends Error { }

class FailedChecks {
    constructor(phase)
    {
        this.phase = phase;
        this.count = 0;
        this.firstMessage = null;
    }

    async check(callback)
    {
        try {
            await callback();
        } catch (error) {
            if (!(error instanceof CheckFailure))
                throw error;
            if (!this.count++)
                this.firstMessage = error.message;
        }
    }

    log(total)
    {
        if (this.count)
            logFailure("Incorrect result " + this.phase + " (" + this.count + " of " + total + " checks): " + this.firstMessage);
    }
}

// PerfTestRunner.log() output is buffered until the runner finishes, so failures are written to
// the document directly.
function logFailure(message)
{
    let log = document.getElementById("log");
    if (!log) {
        log = document.createElement("pre");
        log.id = "log";
        document.body.appendChild(log);
    }
    log.textContent += "FAIL: " + message + "\n";
}

function pixelToString(pixels, offset)
{
    return "(" + pixels[offset] + ", " + pixels[offset + 1] + ", " + pixels[offset + 2] + ", " + pixels[offset + 3] + ")";
}

window.runBindGroupBenchmark = async function (mode) {
    let device;
    let n = 0;
    const before = new FailedChecks("before measurement");
    const during = new FailedChecks("during measurement");
    const after = new FailedChecks("after measurement");
    let probeCount = 0;

    let pipeline;
    let bindGroupLayout;
    let uniformBuffer;
    let target;
    let probeBuffer;
    let readbackBuffer;
    let prebuiltBindGroups;
    const uniformData = new Float32Array(drawCount * uniformStride / 4);
    const color = [0, 0, 0, 0];

    function bindGroupFor(i)
    {
        return device.createBindGroup({
            layout: bindGroupLayout,
            entries: [{ binding: 0, resource: { buffer: uniformBuffer, offset: i * uniformStride, size: 32 } }],
        });
    }

    async function setup()
    {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter)
            throw new Error("WebGPU is not available");
        device = await adapter.requestDevice();
        device.pushErrorScope("validation");

        const module = device.createShaderModule({ code: shaderSource });
        bindGroupLayout = device.createBindGroupLayout({
            entries: [{ binding: 0, visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } }],
        });
        pipeline = device.createRenderPipeline({
            layout: device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
            vertex: { module, entryPoint: "vertexMain" },
            fragment: { module, entryPoint: "fragmentMain", targets: [{ format: "rgba8unorm" }] },
            primitive: { topology: "triangle-list" },
        });
        uniformBuffer = device.createBuffer({ size: uniformData.byteLength, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
        target = device.createTexture({ size: [size, size], format: "rgba8unorm", usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC });
        probeBuffer = device.createBuffer({ size: 256, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
        readbackBuffer = device.createBuffer({ size: size * size * 4, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });

        // The tile positions never change; the colors are written by every run.
        for (let i = 0; i < drawCount; ++i) {
            uniformData[i * uniformStride / 4] = (i % tilesPerRow) * tileSize;
            uniformData[i * uniformStride / 4 + 1] = Math.floor(i / tilesPerRow) * tileSize;
        }
        if (mode == "reusePrebuilt") {
            prebuiltBindGroups = [];
            for (let i = 0; i < drawCount; ++i)
                prebuiltBindGroups.push(bindGroupFor(i));
        }

        const error = await device.popErrorScope();
        if (error)
            throw new Error("Setup: " + error.message);
    }

    // One run: write the colors of run n, then draw all tiles in one render pass, then copy one
    // pixel of the last tile into the probe buffer.
    function encodeRun()
    {
        for (let i = 0; i < drawCount; ++i) {
            colorInto(n, i, color);
            const offset = i * uniformStride / 4 + 4;
            uniformData[offset] = color[0] / 255;
            uniformData[offset + 1] = color[1] / 255;
            uniformData[offset + 2] = color[2] / 255;
            uniformData[offset + 3] = 1;
        }
        device.queue.writeBuffer(uniformBuffer, 0, uniformData);

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
            colorAttachments: [{ view: target.createView(), loadOp: "clear", clearValue: [0, 0, 0, 0], storeOp: "store" }],
        });
        pass.setPipeline(pipeline);
        for (let i = 0; i < drawCount; ++i) {
            pass.setBindGroup(0, mode == "reusePrebuilt" ? prebuiltBindGroups[i] : bindGroupFor(i));
            pass.draw(6);
        }
        pass.end();
        encoder.copyTextureToBuffer({ texture: target, origin: [size - tileSize / 2, size - tileSize / 2] }, { buffer: probeBuffer }, [1, 1]);
        device.queue.submit([encoder.finish()]);
    }

    async function probe()
    {
        await probeBuffer.mapAsync(GPUMapMode.READ);
        const pixel = new Uint8Array(probeBuffer.getMappedRange(), 0, 4).slice();
        probeBuffer.unmap();
        const expected = colorInto(n, drawCount - 1, [0, 0, 0, 0]);
        for (let c = 0; c < 4; ++c) {
            if (Math.abs(pixel[c] - expected[c]) > 1)
                throw new CheckFailure("Probe at (" + (size - tileSize / 2) + ", " + (size - tileSize / 2) + "): got " + pixelToString(pixel, 0) + ", expected " + pixelToString(expected, 0) + ".");
        }
    }

    async function verify()
    {
        const encoder = device.createCommandEncoder();
        encoder.copyTextureToBuffer({ texture: target }, { buffer: readbackBuffer, bytesPerRow: size * 4 }, [size, size]);
        device.queue.submit([encoder.finish()]);
        await readbackBuffer.mapAsync(GPUMapMode.READ);
        const pixels = new Uint8Array(readbackBuffer.getMappedRange()).slice();
        readbackBuffer.unmap();
        const expected = [0, 0, 0, 0];
        let mismatches = 0;
        let firstMismatch = "";
        for (let y = 0; y < size; ++y) {
            for (let x = 0; x < size; ++x) {
                const offset = (y * size + x) * 4;
                colorInto(n, Math.floor(y / tileSize) * tilesPerRow + Math.floor(x / tileSize), expected);
                let matches = true;
                for (let c = 0; c < 4; ++c)
                    matches = matches && Math.abs(pixels[offset + c] - expected[c]) <= 1;
                if (!matches && !mismatches++)
                    firstMismatch = " First at (" + x + ", " + y + "): got " + pixelToString(pixels, offset) + ", expected " + pixelToString(expected, 0) + ".";
            }
        }
        if (mismatches)
            throw new CheckFailure("Tiles: " + mismatches + " of " + (size * size) + " pixels are wrong." + firstMismatch);
    }

    async function checkErrors(label)
    {
        const error = await device.popErrorScope();
        if (error)
            throw new Error(label + ": " + error.message);
    }

    try {
        await setup();
        for (; n < 2; ++n) {
            device.pushErrorScope("validation");
            encodeRun();
            await before.check(probe);
            await before.check(verify);
            await checkErrors("Run " + n);
        }
    } catch (error) {
        logFailure(error.message);
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }

    // PerfTestRunner reports the results and notifies the test runner synchronously when the last
    // value arrives, so the final check runs before the last value is reported.
    let iterationsReported = 0;
    async function measureIteration()
    {
        PerfTestRunner.gc();
        let runs = 0;
        let elapsed = 0;
        const start = PerfTestRunner.now();
        try {
            device.pushErrorScope("validation");
            do {
                ++n;
                encodeRun();
                ++probeCount;
                await during.check(probe);
                ++runs;
                elapsed = PerfTestRunner.now() - start;
            } while (elapsed < iterationTime);
            await checkErrors("Iteration " + (iterationsReported + 1));
            if (iterationsReported == measuredIterationCount) {
                await after.check(verify);
                before.log(4);
                during.log(probeCount);
                after.log(1);
            }
        } catch (error) {
            logFailure(error.message);
            if (window.testRunner)
                testRunner.notifyDone();
            return;
        }
        ++iterationsReported;
        if (PerfTestRunner.measureValueAsync(runs * 1000 / elapsed))
            setTimeout(measureIteration, 0);
    }

    const description = mode == "reusePrebuilt"
        ? "Measures a render pass of 1024 draws, each with one of 1024 bind groups created once, of a uniform buffer slice visible to the vertex and fragment stages, followed by a 1x1 readback."
        : "Measures a render pass of 1024 draws, each with a bind group created for it, of a uniform buffer slice visible to the vertex and fragment stages, followed by a 1x1 readback.";
    PerfTestRunner.prepareToMeasureValuesAsync({
        description,
        unit: "runs/s",
        customIterationCount: measuredIterationCount,
    });
    setTimeout(measureIteration, 0);
};
})();
