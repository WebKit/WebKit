//@ runDefault("--useControlFlowProfiler=true")

function foo() {
    for (var x in []) {
        o[++x];
    }
}
foo()
