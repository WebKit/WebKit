//@ runDefault("--forceDebuggerBytecodeGeneration=true")

// This test should not crash.

function foo() {
    if (new.target) {}
}
+foo();
