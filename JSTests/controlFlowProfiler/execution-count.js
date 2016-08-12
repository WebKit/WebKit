load("./driver/driver.js");

function noop() { ; }
function foo(num) {
    for (let i = 0; i < num; i++) {
        noop(); 
    }
}

function a() { ; }
function b() { ; }

function baz(num) {
    for (let i = 0; i < num; i++) {
        i % 2 ? a() : b();
    }
}

function jaz(num) {
    for (let i = 0; i < num; i++) {
        if (i % 2)
            a();
        else
            b();
    }
}

function testWhile(num) {
    let i = num;
    while (i--) {
        noop();
        if (i % 2)
            a();
        else
            b();
    }
}

foo(120);
assert(basicBlockExecutionCount(foo, "noop()") === 120);
noop();
assert(basicBlockExecutionCount(noop, ";") === 121);

baz(140);
assert(basicBlockExecutionCount(baz, "a()") === 140/2);
assert(basicBlockExecutionCount(baz, "b()") === 140/2);
assert(basicBlockExecutionCount(a, ";") === 140/2);
assert(basicBlockExecutionCount(b, ";") === 140/2);

jaz(140);
assert(basicBlockExecutionCount(jaz, "a()") === 140/2);
assert(basicBlockExecutionCount(jaz, "b()") === 140/2);

testWhile(140);
assert(basicBlockExecutionCount(testWhile, "noop()") === 140);
assert(basicBlockExecutionCount(testWhile, "a()") === 140/2);
assert(basicBlockExecutionCount(testWhile, "b()") === 140/2);

if (is32BitPlatform()) {
    function testMax() {
        let j = 0;
        let numIters = Math.pow(2, 32) + 10;
        for (let i = 0; i < numIters; i++) {
            j++;
        }
    }

    testMax();
    assert(basicBlockExecutionCount(testMax, "j++") === Math.pow(2, 32) - 1);
}
