//@ skip if $memoryLimited
//@ slow!

function foo() {
  Object.freeze(arguments);
}

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

foo();

const a0 = [];
a0.__proto__ = {};
a0.length = 2**24
Object.defineProperty(a0, 0, {get: bar});

function bar() {
  new ArrayBuffer(1000);
  new Int16Array(a0);
}

for (let i=0; i<10000; i++) {
  new Promise(bar);
}

for (let i=0; i<100000; i++) {
    canThrow(() => {
        new Uint32Array(1000).subarray();
    }, `Error: Out of memory`);
}
