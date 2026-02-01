function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error(`actual: ${actual}, expected: ${expected}`);
    }
}

for (var TA of [Float32Array, Float64Array, Int8Array, Int16Array, Int32Array, Uint8Array, Uint16Array, Uint32Array]) {
    var ta = new TA([10, 20, 30, 40, 50, 60]);
    ta.constructor = {
        [Symbol.species]: function () {
            return new TA(ta.buffer, 2 * TA.BYTES_PER_ELEMENT);
        },
    };
    var result = ta.slice(1, 4);
    shouldBe(result.length, 4);
    shouldBe(result[0], 20);
    shouldBe(result[1], 20);
    shouldBe(result[2], 20);
    shouldBe(result[3], 60);
}
