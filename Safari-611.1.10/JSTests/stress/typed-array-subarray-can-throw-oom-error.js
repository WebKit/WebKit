//@ skip if $memoryLimited
//@ slow!

function foo() {
  Object.freeze(arguments);
}

let counter = 0;
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
    if (errorThrown) {
        if (counter++ > 20)
            return true;
    }
    return false;
}

foo();

const a0 = [];
a0.__proto__ = {};
a0.length = 2**24
Object.defineProperty(a0, 0, {get: bar});

function bar() {
  new ArrayBuffer(100000);
  new Int16Array(a0);
}

for (let i=0; i<100; i++) {
  new Promise(bar);
}

for (let i=0; i<10000; i++) {
    let result = canThrow(() => {
        new Uint32Array(1000).subarray();
    }, `RangeError: Out of memory`);
    if (result)
        break;
}
