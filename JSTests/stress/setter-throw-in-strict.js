//@ runDefault("--jitPolicyScale=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function foo() {
    'use strict';
    for (let i=0; i<7; i++) {
        ({
            __proto__: undefined
        });
    }
}

for (let i=0; i < 100; i++) {
    try {
        foo();
        Object.defineProperty(Object.prototype, '__proto__', { set: undefined });
    } catch(e) {
        shouldBe(e.stack.split('\n')[0].startsWith("foo"), true);
    }
}
