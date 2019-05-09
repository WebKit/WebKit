//@ runDefault("--useConcurrentJIT=0", "--useRandomizingFuzzerAgent=1", "--airRandomizeRegs=1", "--airRandomizeRegsSeed=3421187372", "--jitPolicyScale=0")

function foo() {
    try {
        foo.caller;
    } catch (e) {
        return Array.of(arguments).join();
    }
    throw new Error();
}

function bar() {
'use strict';
    try {
        return foo();
    } finally {
    }
}

for (var i = 0; i < 10000; ++i) {
    bar();
}
