// Shared-worker side of fast/canvas/offscreencanvas-in-shared-worker-readback.html
"use strict";

function runCanvasWorkload() {
    const size = 2048; // Large enough to request an accelerated backing.
    const canvas = new OffscreenCanvas(size, size);
    const ctx = canvas.getContext("2d", { willReadFrequently: false });

    const tile = new OffscreenCanvas(256, 256);
    const tileCtx = tile.getContext("2d");
    tileCtx.fillStyle = "rgb(0, 255, 0)";
    tileCtx.fillRect(0, 0, 256, 256);
    const bitmap = tile.transferToImageBitmap();

    ctx.fillStyle = "rgb(255, 0, 0)";
    ctx.fillRect(0, 0, size, size);
    ctx.drawImage(bitmap, 0, 0);

    const topLeft = ctx.getImageData(0, 0, 1, 1).data;
    const bottomRight = ctx.getImageData(size - 1, size - 1, 1, 1).data;
    return {
        topLeft: [topLeft[0], topLeft[1], topLeft[2], topLeft[3]],
        bottomRight: [bottomRight[0], bottomRight[1], bottomRight[2], bottomRight[3]],
    };
}

self.onconnect = (event) => {
    const port = event.ports[0];
    port.onmessage = () => {
        let result;
        try {
            result = runCanvasWorkload();
        } catch (e) {
            result = { error: String(e && e.message || e) };
        }
        port.postMessage(result);
    };
    port.start();
};
