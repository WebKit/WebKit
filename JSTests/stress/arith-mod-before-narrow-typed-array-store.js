// Test: `ArithMod` elision before narrow typed-array stores.
// DFGStrengthReductionPhase rewires the PutByVal's value edge past `ArithMod(x, k*2^W)`
// to `x` when storing into a W-bit typed array (Int8, Uint8, Int16, Uint16). The
// semantics match because the store takes the low W bits and `x` agrees with
// `x % (k*2^W)` modulo 2^W. Uint8ClampedArray saturates, so the mod must NOT be
// elided there. The ArithMod node itself must not be converted to Identity because
// other users (e.g. the function's return value) still need `x % k`.

function uint8StoreMod(arr, i, a, b) {
    arr[i] = (a + b) % 256;
}
noInline(uint8StoreMod);

function int8StoreMod(arr, i, a, b) {
    arr[i] = (a + b) % 256;
}
noInline(int8StoreMod);

function uint16StoreMod(arr, i, a, b) {
    arr[i] = (a + b) % 65536;
}
noInline(uint16StoreMod);

function int16StoreMod(arr, i, a, b) {
    arr[i] = (a + b) % 65536;
}
noInline(int16StoreMod);

function uint8StoreModMultiple(arr, i, a, b) {
    // 256 * 3 = 768 is a multiple of 256; elision still valid.
    arr[i] = (a + b) % 768;
}
noInline(uint8StoreModMultiple);

function uint8StoreNegativeMod(arr, i, a, b) {
    // Negative divisor: `|k|` == 256, still a multiple of 2^8. Elision valid.
    arr[i] = (a + b) % -256;
}
noInline(uint8StoreNegativeMod);

function uint8StoreSmallMod(arr, i, a, b) {
    // 100 is not a multiple of 256 so the mod must NOT be elided.
    // We still require the produced value to be correct.
    arr[i] = (a + b) % 100;
}
noInline(uint8StoreSmallMod);

function clampedStoreMod(arr, i, a, b) {
    // Uint8ClampedArray saturates (0..255), so `(x % 256)` and `x` differ for
    // values outside [0,255]. The mod must NOT be elided here.
    arr[i] = (a + b) % 256;
}
noInline(clampedStoreMod);

const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;

function toUint8(x) { return ((x % 256) + 256) % 256; }
function toInt8(x) {
    const u = toUint8(x);
    return u >= 128 ? u - 256 : u;
}
function toUint16(x) { return ((x % 65536) + 65536) % 65536; }
function toInt16(x) {
    const u = toUint16(x);
    return u >= 32768 ? u - 65536 : u;
}
function toUint8Clamped(x) {
    if (Number.isNaN(x)) return 0;
    if (x <= 0) return 0;
    if (x >= 255) return 255;
    // Round half to even.
    const f = Math.floor(x);
    const diff = x - f;
    if (diff < 0.5) return f;
    if (diff > 0.5) return f + 1;
    return (f & 1) ? f + 1 : f;
}

const testPairs = [
    [0, 0],
    [1, 1],
    [0, -1],
    [-1, -1],
    [INT32_MIN, 0],
    [INT32_MIN, -1],
    [INT32_MAX, 0],
    [INT32_MAX, 1],
    [255, 1],
    [-255, -1],
    [256, 0],
    [-256, 0],
    [257, -1],
    [-257, 1],
    [32767, 1],
    [-32768, 0],
    [-32768, -1],
    [65535, 1],
    [65536, 0],
    [-65536, -1],
    [1_000_000, 123],
    [-1_000_000, -123],
];

const u8 = new Uint8Array(1);
const i8 = new Int8Array(1);
const u16 = new Uint16Array(1);
const i16 = new Int16Array(1);
const clamped = new Uint8ClampedArray(1);

// Warm up so DFG / FTL compile the hot stores and the strength-reduction runs.
for (let i = 0; i < testLoopCount; ++i) {
    uint8StoreMod(u8, 0, i, 3);
    int8StoreMod(i8, 0, i, 3);
    uint16StoreMod(u16, 0, i, 3);
    int16StoreMod(i16, 0, i, 3);
    uint8StoreModMultiple(u8, 0, i, 3);
    uint8StoreNegativeMod(u8, 0, i, 3);
    uint8StoreSmallMod(u8, 0, i, 3);
    clampedStoreMod(clamped, 0, i, 3);
}

function check(expectFn, storeFn, arr, a, b, label) {
    storeFn(arr, 0, a, b);
    const sum = a + b;
    const expected = expectFn(sum);
    if (arr[0] !== expected)
        throw new Error(`${label}(${a}, ${b}): expected ${expected}, got ${arr[0]}`);
}

for (const [a, b] of testPairs) {
    check(toUint8, uint8StoreMod, u8, a, b, "uint8StoreMod");
    check(toInt8, int8StoreMod, i8, a, b, "int8StoreMod");
    check(toUint16, uint16StoreMod, u16, a, b, "uint16StoreMod");
    check(toInt16, int16StoreMod, i16, a, b, "int16StoreMod");
    check(toUint8, uint8StoreModMultiple, u8, a, b, "uint8StoreModMultiple");
    check(toUint8, uint8StoreNegativeMod, u8, a, b, "uint8StoreNegativeMod");

    // Small mod: the actual stored value must match `(sum % 100)` narrowed to Uint8.
    const sum = a + b;
    const expectedSmall = toUint8(sum % 100);
    uint8StoreSmallMod(u8, 0, a, b);
    if (u8[0] !== expectedSmall)
        throw new Error(`uint8StoreSmallMod(${a}, ${b}): expected ${expectedSmall}, got ${u8[0]}`);

    // Clamped: the mod must remain so that saturation sees the already-reduced value.
    const expectedClamped = toUint8Clamped(sum % 256);
    clampedStoreMod(clamped, 0, a, b);
    if (clamped[0] !== expectedClamped)
        throw new Error(`clampedStoreMod(${a}, ${b}): expected ${expectedClamped}, got ${clamped[0]}`);
}

// Regression guard for the benchmark pattern: narrow typed array filled in a
// tight loop with `(i + counter) % width`. Counter is kept live-in and mutated
// outside the loop so it's not an obvious constant.
function fillUint8(view, len, counter) {
    for (let i = 0; i < len; ++i)
        view[i] = (i + counter) % 256;
}
noInline(fillUint8);

const big = new Uint8Array(256);
for (let iter = 0; iter < testLoopCount; ++iter) {
    const counter = iter * 7 + 13;
    fillUint8(big, big.length, counter);
    for (let i = 0; i < big.length; ++i) {
        const expected = toUint8(i + counter);
        if (big[i] !== expected)
            throw new Error(`fillUint8 iter=${iter} i=${i}: expected ${expected}, got ${big[i]}`);
    }
}

// Regression guard: the ArithMod result is used both by a narrow typed-array store
// AND returned to the caller. The strength reduction must rewire only the
// PutByVal's value edge — converting the shared ArithMod node to Identity would
// make the return value observe `x` instead of `x % 256`.
function sharedMod(arr, x) {
    let m = x % 256;
    arr[0] = m;
    return m;
}
noInline(sharedMod);

const sharedArr = new Uint8Array(1);
for (let i = 0; i < testLoopCount; ++i) {
    const r = sharedMod(sharedArr, i);
    const expected = toUint8(i);
    if (r !== expected)
        throw new Error(`sharedMod i=${i}: return expected ${expected}, got ${r}`);
    if (sharedArr[0] !== expected)
        throw new Error(`sharedMod i=${i}: store expected ${expected}, got ${sharedArr[0]}`);
}

// Regression guard for the ArithMod -> UInt32ToNumber fallthrough in
// DFGStrengthReductionPhase: after rewiring past `ArithMod(x, 256)`, the
// value edge may now point to a `UInt32ToNumber` node that can also be
// stripped before the narrow-typed-array store. `x >>> 0` reinterprets x as
// an unsigned 32-bit value, which the DFG wraps in UInt32ToNumber when the
// high bit may be set.
function uint8StoreUnsignedShiftMod(arr, x) {
    arr[0] = (x >>> 0) % 256;
}
noInline(uint8StoreUnsignedShiftMod);

const shiftArr = new Uint8Array(1);
for (let i = 0; i < testLoopCount; ++i) {
    uint8StoreUnsignedShiftMod(shiftArr, i);
    const expected = toUint8((i >>> 0) % 256);
    if (shiftArr[0] !== expected)
        throw new Error(`uint8StoreUnsignedShiftMod i=${i}: expected ${expected}, got ${shiftArr[0]}`);
}

for (const x of [-1, -2, -256, -257, INT32_MIN, INT32_MIN + 1, INT32_MAX, -1_000_000]) {
    uint8StoreUnsignedShiftMod(shiftArr, x);
    const expected = toUint8((x >>> 0) % 256);
    if (shiftArr[0] !== expected)
        throw new Error(`uint8StoreUnsignedShiftMod x=${x}: expected ${expected}, got ${shiftArr[0]}`);
}

// Regression guard for iterative folding: two nested mods that are both
// multiples of the array element width should collapse in a single pass.
// `(x % 65536) % 256` stored into Uint8Array — the outer mod peels, then the
// inner mod peels, leaving just `x` at the PutByVal value edge.
function uint8StoreNestedMod(arr, x) {
    arr[0] = (x % 65536) % 256;
}
noInline(uint8StoreNestedMod);

const nestedArr = new Uint8Array(1);
for (let i = 0; i < testLoopCount; ++i) {
    uint8StoreNestedMod(nestedArr, i);
    const expected = toUint8(i);
    if (nestedArr[0] !== expected)
        throw new Error(`uint8StoreNestedMod i=${i}: expected ${expected}, got ${nestedArr[0]}`);
}

for (const x of [0, 1, -1, 255, -255, 256, -256, 32767, -32768, 65535, 65536, -65536, INT32_MIN, INT32_MAX, 1_000_000, -1_000_000]) {
    uint8StoreNestedMod(nestedArr, x);
    const expected = toUint8(x);
    if (nestedArr[0] !== expected)
        throw new Error(`uint8StoreNestedMod x=${x}: expected ${expected}, got ${nestedArr[0]}`);
}

// Mixed nesting: ArithMod over UInt32ToNumber over an Int32 value. The outer
// mod peels first, exposing UInt32ToNumber, which then also peels.
function uint8StoreModOverUnsignedShift(arr, x) {
    arr[0] = ((x * 3) >>> 0) % 65536;
}
noInline(uint8StoreModOverUnsignedShift);

const mixedArr = new Uint8Array(1);
for (let i = 0; i < testLoopCount; ++i) {
    uint8StoreModOverUnsignedShift(mixedArr, i);
    const expected = toUint8(((i * 3) >>> 0) % 65536);
    if (mixedArr[0] !== expected)
        throw new Error(`uint8StoreModOverUnsignedShift i=${i}: expected ${expected}, got ${mixedArr[0]}`);
}
