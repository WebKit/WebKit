//@ skip if $memoryLimited
//@ requireOptions("--useGC=0")
let z = new Float64Array(1000);

const a = [];
let realLength = 0;
try {
    a.__proto__ = null;
    Object.defineProperty(a, 0, { get: foo });
    Object.defineProperty(a, 2**29, {});
    function foo() {
        a[++realLength] = new Uint8Array(a);
    }

    foo();
} catch { }

let i;
try {
  for (i = 0; i < 100000000; i++) {
    new ArrayBuffer(1000);
  }
} catch { }

try {
    z.buffer;
} catch { }
