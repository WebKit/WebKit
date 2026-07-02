//@ requireOptions("--forceDiskCache=0")

let verbose = false;

function check(expected, state = $vm.vmTaintedState()) {
    if (expected != state)
        throw new Error("Expected " + expected + " but state was " + state);
}

function checkNot(expected, state = $vm.vmTaintedState()) {
    if (expected == state)
        throw new Error("Expected to not be " + expected);
}

function shouldBeUntainted() {
    if (verbose) {
        print("Ran shouldBeUntainted");
    }
    check("Untainted");
}

function shouldBeTainted() {
    if (verbose) {
        print("Ran shouldBeTainted");
    }
    checkNot("Untainted");
}

$vm.runTaintedString(`
for (let i = 0; i < testLoopCount; ++i) {
    setTimeout(shouldBeTainted.bind("foo"), 0);
}
`);

