//@ runWebAssembly
// This passes if it does not crash.
new WebAssembly.CompileError({
    valueOf() {
        throw new Error();
    }
});
