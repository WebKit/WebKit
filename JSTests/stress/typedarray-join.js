const typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

function test() {
    for (const constructor of typedArrays) {
        const ta = new constructor([1,2,3]);
        shouldBe(ta.join(), '1,2,3');
        shouldBe(ta.join(','), '1,2,3');
        shouldBe(ta.join('--'), '1--2--3');

        shouldBe(ta.join({ toString() { transferArrayBuffer(ta.buffer); return ','; } }), ',,');
        shouldThrowTypeError(() => ta.join());
    }
}

for (let i = 0; i < 1e3; i++)
    test();
