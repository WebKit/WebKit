//@ runDefault
// This test passes if it does not crash.
let x = new DataView(new ArrayBuffer(1));
Object.defineProperty(x, 'foo', {});
