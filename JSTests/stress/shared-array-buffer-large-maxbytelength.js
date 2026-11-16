// One byte past MAX_ARRAY_BUFFER_SIZE, so neither constructor can satisfy it.
const MAX64 = 2 ** 34 + 1;
// Okay to throw, but should not crash.
try {
    new SharedArrayBuffer(4, { maxByteLength: MAX64 });
} catch (e) {
}
try {
    new ArrayBuffer(4, { maxByteLength: MAX64 });
} catch (e) {
}
