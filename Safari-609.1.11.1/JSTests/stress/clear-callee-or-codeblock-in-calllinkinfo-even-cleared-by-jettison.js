//@ runDefault("--osrExitCountForReoptimizationFromLoop=2", "--useFTLJIT=0", "--slowPathAllocsBetweenGCs=100", "--forceDebuggerBytecodeGeneration=1", "--forceEagerCompilation=1")

function foo(x, y) {
}
for (var i = 0; i < 1000; ++i)
    foo(0)
for (var i = 0; i < 100000; ++i)
    foo()
