//@ run("--useConcurrentJIT=false")

// This test is set up delicately to:
// 1. cause the test() function to DFG compile with just the right amount of profiling
//    so that ...
// 2. the DFG identifies the "return Function()" path as dead, and ...
// 3. the DFG compiled function doesn't OSR exit too many times before ...
// 4. we change the implementation of the inlined foo() and execute test() again.
// 
// This test should not crash.

eval("\"use strict\"; var w;");
foo = function() { throw 0; }
var x;

(function() {
    eval("test = function() { ~foo(~(0 ? ~x : x) ? 0 : 0); return Function(); }");
})();

// This loop count of 2000 was empirically determined to be the right amount to get this
// this issue to manifest.  Dropping or raising it may mask the issue and prevent it from
// manifesting.
for (var i = 0; i < 2000; ++i) {
    try {
        test();
    } catch(e) {
    }
}

foo = function() { };
test();
