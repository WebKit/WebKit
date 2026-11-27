importScripts("/resources/testharness.js");
importScripts("canvas-videoframe-copyto-utilities.js", "videoFrame-webgpuCanvas-copyTo.js");

defineWebGPUCanvasVideoFrameTests(["worker OffscreenCanvas"]);
done();
