//@ skip if $memoryLimited
//@ slow!

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
}

const a0 = [];
a0.__proto__ = {};
a0.length = 2**26;
Object.defineProperty(a0, 0, {get: bar});

function bar() {
    new Uint8Array(a0);
}

new Promise(bar);

try {
    for (let i=0; i<10000; i++) {
        new Uint8Array(10000);
    }
} catch {}

function foo(x, y, z) {
    delete arguments[0];
    with (Object) {}
}

for (let i=0; i<10000; i++) {
    canThrow(() => {
        foo(0);
    }, `RangeError: Out of memory`);
}
