//@ runNoCJIT("--forceDebuggerBytecodeGeneration=true", "--useBaselineJIT=0", "--alwaysUseShadowChicken=true")

function foo() {
    foo()
}
try {
    foo();
} catch(e) { }
