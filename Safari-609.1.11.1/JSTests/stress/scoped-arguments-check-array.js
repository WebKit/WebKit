//@ defaultRun
//@ runNoLLInt("--useConcurrentJIT=false", "--forceEagerCompilation=True")

// This is a regression test that verifies we handle direct arguments as ArrayStorage.  This test should complete and not crash.
// It is a reduction of a fuzzing bug produced testcase.  All of the code present was needed to reproduce the issue.

let a;
let f2;
let args;

function setup(arg1) {
    function foo() { return arg1; }
    a = [0];
    a.unshift(0);
    for (let z of [4, 4, 4, 4, 4]) {};
    new Float64Array(a);
    f2 = function() {};
    args = arguments;
    args.length = 0;
};

function forOfArray() {
    for (let z of [true, true, true, true, true, true, true]) {
    }
}

function forOfArgs() {
    for (let v of args) {
    }
}

function callEveryOnArgs() {
    for (i = 0; i < 1000; ++i) {
        Array.prototype.every.call(args, f2, {});
    }
}

setup();
forOfArray();
forOfArgs();
callEveryOnArgs();
