//@ runProfiler
// This tests the profiler DFG node, CountExecution, plays nicely with TailCalls.

"use strict";

function tail(a) { return 1; }
noInline(tail);

function inlineTail(a) {
    return tail(a);
}

function inlineTailVarArgs(a) {
    return tail(...[a])
}

function inlineTailTernary(a) {
    return true ? tail(a) : null;
}

function body() {
    for (var i = 0; i < 10000; i++) {
        inlineTail(1);
        inlineTailVarArgs(1);
        inlineTailTernary(1)
    }
}

body();
