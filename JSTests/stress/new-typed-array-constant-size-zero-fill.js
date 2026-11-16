//@ requireOptions("--useConcurrentJIT=0")

function shouldBeZeroFilled(array, length, name) {
    if (array.length !== length)
        throw new Error(name + ": length expected " + length + " but got " + array.length);
    for (let i = 0; i < length; ++i) {
        if (array[i] !== (typeof array[i] === "bigint" ? 0n : 0))
            throw new Error(name + ": element " + i + " is " + array[i]);
    }
}
noInline(shouldBeZeroFilled);

// Zero length.
function f64_0() { return new Float64Array(0); }
noInline(f64_0);
function i8_0() { return new Int8Array(0); }
noInline(i8_0);

// Single element.
function f64_1() { return new Float64Array(1); }
noInline(f64_1);
function u8_1() { return new Uint8Array(1); }
noInline(u8_1);

// Odd byte sizes (round-up to 8).
function i8_3() { return new Int8Array(3); }
noInline(i8_3);
function i8_7() { return new Int8Array(7); }
noInline(i8_7);
function i8_9() { return new Int8Array(9); }
noInline(i8_9);
function u16_5() { return new Uint16Array(5); }
noInline(u16_5);
function f32_3() { return new Float32Array(3); }
noInline(f32_3);

// DFG unroll word-limit boundary (16 words).
function f64_16() { return new Float64Array(16); }
noInline(f64_16);
function f64_17() { return new Float64Array(17); }
noInline(f64_17);
function u32_32() { return new Uint32Array(32); }
noInline(u32_32);
function u32_33() { return new Uint32Array(33); }
noInline(u32_33);
function i8_128() { return new Int8Array(128); }
noInline(i8_128);
function i8_129() { return new Int8Array(129); }
noInline(i8_129);

// FTL splatWords unroll boundary (10 words).
function f64_10() { return new Float64Array(10); }
noInline(f64_10);
function f64_11() { return new Float64Array(11); }
noInline(f64_11);

// fastSizeLimit boundary (1000).
function f64_1000() { return new Float64Array(1000); }
noInline(f64_1000);
function f64_1001() { return new Float64Array(1001); }
noInline(f64_1001);

// BigInt typed arrays.
function bi64_2() { return new BigInt64Array(2); }
noInline(bi64_2);
function bu64_16() { return new BigUint64Array(16); }
noInline(bu64_16);

// Negative constant must throw.
function i32_neg() { return new Int32Array(-1); }
noInline(i32_neg);

// Allocator-reuse: dirty storage then verify fresh allocation is zero.
function dirty() {
    let a = new Float64Array(4);
    a[0] = 1.5; a[1] = -2.5; a[2] = 3.5; a[3] = -4.5;
    return a;
}
noInline(dirty);
function fresh() { return new Float64Array(4); }
noInline(fresh);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBeZeroFilled(f64_0(), 0, "f64_0");
    shouldBeZeroFilled(i8_0(), 0, "i8_0");
    shouldBeZeroFilled(f64_1(), 1, "f64_1");
    shouldBeZeroFilled(u8_1(), 1, "u8_1");
    shouldBeZeroFilled(i8_3(), 3, "i8_3");
    shouldBeZeroFilled(i8_7(), 7, "i8_7");
    shouldBeZeroFilled(i8_9(), 9, "i8_9");
    shouldBeZeroFilled(u16_5(), 5, "u16_5");
    shouldBeZeroFilled(f32_3(), 3, "f32_3");
    shouldBeZeroFilled(f64_16(), 16, "f64_16");
    shouldBeZeroFilled(f64_17(), 17, "f64_17");
    shouldBeZeroFilled(u32_32(), 32, "u32_32");
    shouldBeZeroFilled(u32_33(), 33, "u32_33");
    shouldBeZeroFilled(i8_128(), 128, "i8_128");
    shouldBeZeroFilled(i8_129(), 129, "i8_129");
    shouldBeZeroFilled(f64_10(), 10, "f64_10");
    shouldBeZeroFilled(f64_11(), 11, "f64_11");
    shouldBeZeroFilled(f64_1000(), 1000, "f64_1000");
    shouldBeZeroFilled(f64_1001(), 1001, "f64_1001");
    shouldBeZeroFilled(bi64_2(), 2, "bi64_2");
    shouldBeZeroFilled(bu64_16(), 16, "bu64_16");

    let threw = false;
    try { i32_neg(); } catch (e) { threw = e instanceof RangeError; }
    if (!threw)
        throw new Error("i32_neg: expected RangeError");

    dirty();
    shouldBeZeroFilled(fresh(), 4, "fresh-after-dirty");
}
