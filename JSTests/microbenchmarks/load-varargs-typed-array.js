// Microbenchmark for the typed-array fast path in loadVarargs: spreading a typed
// array through Function.prototype.apply (and f(...ta)). This is the pattern the
// TextDecoder polyfill hits via String.fromCharCode.apply(null, uint16Array).

function sink() {
    "use strict";
    let sum = 0;
    for (let i = 0; i < arguments.length; ++i)
        sum += arguments[i];
    return sum;
}
noInline(sink);

// Small per-element values keep the accumulated sum well below 2^53 so the exact
// equality check below stays valid for any testLoopCount.
const i8 = new Int8Array(64);
const u16 = new Uint16Array(64);
const i32 = new Int32Array(64);
const f64 = new Float64Array(64);
for (let i = 0; i < 64; ++i) {
    i8[i] = i % 100;
    u16[i] = i % 100;
    i32[i] = i % 100;
    f64[i] = i % 100;
}

let perElementSum = 0;
for (let i = 0; i < 64; ++i)
    perElementSum += i % 100;

const count = testLoopCount;

let total = 0;
for (let i = 0; i < count; ++i) {
    total += sink.apply(null, i8);
    total += sink.apply(null, u16);
    total += sink.apply(null, i32);
    total += sink.apply(null, f64);
    total += sink(...u16);
}
if (total !== perElementSum * 5 * count)
    throw new Error("bad sum: " + total);

// Real-world hot pattern: build a string from a Uint16Array of char codes.
const codes = new Uint16Array(200);
for (let i = 0; i < codes.length; ++i)
    codes[i] = 65 + (i % 26);
let length = 0;
for (let i = 0; i < count; ++i)
    length += String.fromCharCode.apply(null, codes).length;
if (length !== codes.length * count)
    throw new Error("bad length: " + length);
