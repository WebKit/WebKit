const MAX64 = 4294967296 + 1;
// Okay to throw, but should not crash.
try {
    new SharedArrayBuffer(4, { maxByteLength: MAX64 });
} catch (e) {
}
try {
    new ArrayBuffer(4, { maxByteLength: MAX64 });
} catch (e) {
}