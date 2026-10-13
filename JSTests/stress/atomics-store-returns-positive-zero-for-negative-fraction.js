function shouldBe(actual, expected, what) {
    if (!Object.is(actual, expected))
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

const u8 = new Uint8Array(new SharedArrayBuffer(8));
const i32 = new Int32Array(new SharedArrayBuffer(8));

function storeU8(v) { return Atomics.store(u8, 0, v); }
noInline(storeU8);

function storeI32(v) { return Atomics.store(i32, 0, v); }
noInline(storeI32);

for (let i = 1; i < testLoopCount; ++i) {
    shouldBe(storeU8(i + 0.5), i, "warm storeU8");
    shouldBe(storeI32(-i - 0.5), -i, "warm storeI32");
}

for (const value of [-0.5, -0.9, -1e-7, -5e-324, -0, NaN]) {
    shouldBe(storeU8(value), 0, "Atomics.store(u8, 0, " + value + ")");
    shouldBe(storeI32(value), 0, "Atomics.store(i32, 0, " + value + ")");
}
shouldBe(storeI32(-1.5), -1, "Atomics.store(i32, 0, -1.5)");
shouldBe(storeI32(-Infinity), -Infinity, "Atomics.store(i32, 0, -Infinity)");
