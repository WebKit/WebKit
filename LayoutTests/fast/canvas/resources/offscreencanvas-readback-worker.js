// Dedicated-worker side of fast/canvas/offscreencanvas-in-dedicated-worker-readback.html
"use strict";

self.onmessage = () => {
    let result;
    try {
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
        result = {
            topLeft: [topLeft[0], topLeft[1], topLeft[2], topLeft[3]],
            bottomRight: [bottomRight[0], bottomRight[1], bottomRight[2], bottomRight[3]],
        };
    } catch (e) {
        result = { error: String(e && e.message || e) };
    }
    self.postMessage(result);
};
