function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error('Expected ' + errorType.name + ' but got: ' + error);
    if (message !== undefined && String(error) !== message)
        throw new Error('bad error: ' + String(error));
}

shouldThrow(() => Worker("", { source: "string" }), TypeError);
shouldThrow(() => new Worker("", { type: "string" }), TypeError, "TypeError: Worker type must be 'classic' or 'module'.");
shouldThrow(() => new Worker("", { source: "inline" }), TypeError, "TypeError: Worker source must be 'file' or 'string'.");
shouldThrow(() => new Worker("./resources/this-file-does-not-exist.mjs", { type: "module" }), Error);
shouldThrow(() => new Worker("", 1), TypeError);
shouldThrow(() => new Worker("./resources/this-file-does-not-exist.js"), Error);
shouldThrow(() => Worker.prototype.postMessage.call({}, 1), TypeError);
shouldThrow(() => Worker.prototype.terminate.call({}), TypeError);

let worker = new Worker("", { source: "string" });
shouldThrow(() => worker.postMessage(function () { }), TypeError, "TypeError: Value could not be cloned.");
shouldThrow(() => worker.postMessage(1, [1]), TypeError, "TypeError: postMessage transfer list can only contain ArrayBuffers");
let buffer = new ArrayBuffer(8);
shouldThrow(() => worker.postMessage(buffer, [buffer, buffer]), TypeError, "TypeError: Duplicate transferable for structured clone");
if (buffer.byteLength !== 8)
    throw new Error("failed postMessage should not detach");
worker.terminate();
