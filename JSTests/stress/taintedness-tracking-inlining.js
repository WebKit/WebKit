//@ requireOptions("--forceDiskCache=0")
// $vm doesn't have a source provider that can cache to disk. Skip that testing configuration.

$vm.runTaintedString("function taintedFunc() { return $vm.vmTaintedState(); }");

function foo() {
    return taintedFunc();
}
noInline(foo);

for (let i = 0; i < 1e5; ++i) {
    let state = foo();
    if (state !== "KnownTainted")
        throw new Error(state);
}

setTimeout(() => {
    let state = foo();
    if (state !== "KnownTainted")
        throw new Error(state);
});