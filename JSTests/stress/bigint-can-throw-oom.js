//@ skip if $memoryLimited
//@ runDefault("--useConcurrentJIT=0")

function canThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (errorThrown && String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
    return false;
}

const a0 = [];
a0.__proto__ = {};
a0.length = 2**23
Object.defineProperty(a0, 0, { get: foo });

function foo() {
    new Int16Array(a0);
}

new Promise(foo);

try {
    for (let i = 0; i < 10000; i++) {
        new Uint8Array(100000);
    }
} catch {}

canThrow(() => {
    for (let i=0n; i<10000n; i++) {
        2n**i;
    }
}, `RangeError: Out of memory`);
