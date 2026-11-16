function testResize(ctor, bytesPerElement, initialBytes, shrinkBytes, resizeAt) {
    const newCount = shrinkBytes / bytesPerElement;
    const resizableArrayBuffer = new ArrayBuffer(initialBytes, { maxByteLength: initialBytes * 4 });
    const source = new ctor(resizableArrayBuffer);
    source[Symbol.iterator] = null;

    let callbacks = 0;
    ctor.from(source, (val, index) => {
        if (index === resizeAt)
            resizableArrayBuffer.resize(shrinkBytes);
        callbacks++;
        return val;
    });

    const expected = Math.max(resizeAt + 1, newCount);
    if (callbacks > expected)
        throw new Error(ctor.name + ": " + callbacks + " callbacks (expected <= " + expected + ")");
}

function testDetach(detachAt) {
    const arrayBuffer = new ArrayBuffer(256);
    const source = new Int32Array(arrayBuffer);
    source[Symbol.iterator] = null;

    let callbacks = 0;
    try {
        Int32Array.from(source, (val, index) => {
            if (index === detachAt)
                arrayBuffer.transfer();
            callbacks++;
            return val;
        });
    } catch (e) {
        return;
    }

    if (callbacks > detachAt + 1)
        throw new Error("detach: " + callbacks + " callbacks (expected <= " + (detachAt + 1) + ")");
}

testResize(Int32Array,   4, 4096, 16, 4);
testResize(Int32Array,   4, 4096,  8, 8);
testResize(Float64Array, 8, 8192, 32, 8);
testResize(Uint8Array,   1, 1024,  4, 8);
testResize(Int32Array,   4, 4096,  8, 0);
testDetach(4);
testDetach(0);
