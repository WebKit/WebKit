function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrowTypeError(func) {
    try {
        func();
    } catch (error) {
        if (error instanceof TypeError)
            return;
        throw new Error('expected a TypeError but got: ' + error);
    }
    throw new Error('expected a TypeError but nothing was thrown');
}

asyncTestStart(1);

// The parent sends five messages: three that must arrive as clones, then one per form of the
// transfer argument, each of which the worker echoes back using that same form.
let worker = new Worker(`
    let count = 0;
    onmessage = function (event) {
        if (++count <= 3) {
            if (event.data.byteLength !== 4)
                throw new Error("buffer was transferred instead of cloned");
            return;
        }

        let view = new Uint8Array(event.data);
        let sum = 0;
        for (let byte of view)
            sum += byte;
        view[0] = sum;

        if (count === 4)
            postMessage(event.data, [event.data]);
        else
            postMessage(event.data, { transfer: [event.data] });

        if (event.data.byteLength !== 0)
            throw new Error("buffer was not detached in the worker");

        if (count === 5)
            close();
    };
`, { source: "string" });

let replies = 0;
worker.onmessage = function (event) {
    let view = new Uint8Array(event.data);
    shouldBe(view.length, 4);
    shouldBe(view[0], 10);
    shouldBe(view[3], 4);
    if (++replies === 2)
        asyncTestPassed();
};

let kept = new Uint8Array([1, 2, 3, 4]).buffer;

worker.postMessage(kept);
shouldBe(kept.byteLength, 4);
worker.postMessage(kept, null);
shouldBe(kept.byteLength, 4);
worker.postMessage(kept, { });
shouldBe(kept.byteLength, 4);

shouldThrowTypeError(() => worker.postMessage(kept, 42));
shouldThrowTypeError(() => worker.postMessage(kept, { transfer: 42 }));
shouldBe(kept.byteLength, 4);

let sequenceBuffer = new Uint8Array([1, 2, 3, 4]).buffer;
worker.postMessage(sequenceBuffer, [sequenceBuffer]);
shouldBe(sequenceBuffer.byteLength, 0);

let optionsBuffer = new Uint8Array([1, 2, 3, 4]).buffer;
worker.postMessage(optionsBuffer, { transfer: [optionsBuffer] });
shouldBe(optionsBuffer.byteLength, 0);
