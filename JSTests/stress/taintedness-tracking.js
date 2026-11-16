//@ requireOptions("--forceDiskCache=0")
// $vm doesn't have a source provider that can cache to disk. Skip that testing configuration.

function check(expected, state = $vm.vmTaintedState()) {
    if (expected != state)
        throw new Error("Expected " + expected + " but state was " + state);

}

check("Untainted");

function callArg(foo) { return foo(); }
let state = callArg($vm.vmTaintedState)
check("Untainted", state);

state = null;
$vm.runTaintedString("function taintedFunc() { return $vm.vmTaintedState(); } state = $vm.vmTaintedState()");
check("KnownTainted", state);
check("IndirectlyTaintedByHistory");

let func = Function("return $vm.vmTaintedState();");

Promise.resolve().then(() => {
    check("IndirectlyTaintedByHistory");
});

setTimeout(() => {
    check("Untainted");
    state = func();
    check("IndirectlyTaintedByHistory", state);
    check("IndirectlyTaintedByHistory");
});

setTimeout(() => {
    state = taintedFunc();
    check("KnownTainted", state);
    check("IndirectlyTaintedByHistory");
});

let evalFunc = $vm.runTaintedString(`(function() {
    return eval("Function('return $vm.vmTaintedState();')");
})`);

setTimeout(() => {
    let func = evalFunc();
    setTimeout(() => {
        check("IndirectlyTainted", func());
    });
});

function shouldBeTainted() {
    if ($vm.vmTaintedState() == "Untainted")
        throw new Error("Expected tainted");
}

$vm.runTaintedString(`setTimeout(shouldBeTainted.bind("foo"), 0);`);

setTimeout(() => {
    // Test JSONP code paths, which can create code via setters.
    check("Untainted");
    globalThis.foo = { set bar(value) { this.baz = eval(value); }}
    $vm.runTaintedString("foo.bar = '(function() { return $vm.vmTaintedState(); })'");
    check("IndirectlyTaintedByHistory");
    setTimeout(() => {
        check("Untainted");
        state = foo.baz();
        check("IndirectlyTainted", state);
        check("IndirectlyTaintedByHistory");
    })
});

setTimeout(() => {
    state = null;
    $vm.runTaintedString("state = callArg($vm.vmTaintedState)");
    check("KnownTainted", state);
});

