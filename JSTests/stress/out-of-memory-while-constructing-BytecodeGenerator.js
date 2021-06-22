//@ skip if $memoryLimited
//@ runDefault

function bar() {
    const a = [0];
    a.__proto__ = {};
    Object.defineProperty(a, 0, { get: foo });
    Object.defineProperty(a, 80000000, {});
    function foo() {
        new Uint8Array(a);
    }
    new Promise(foo);
    try {
        for (let i = 0; i < 10000000; i++)
            new ArrayBuffer(1000);
    } catch(e) {}
} 

function foo(a0, a1, a2) {
    eval();
}

bar();

var exception;
try {
    foo();
} catch (e) {
    exception = e;
}

if (exception && exception != "RangeError: Out of memory")
    throw "FAIL";
