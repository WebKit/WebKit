//@ requireOptions("--destroy-vm", "--collectContinuously=true", "--forceMiniVMMode=true")

// This test passes if it does not hang or crash.
let a = new Uint8Array(10000);
for (let x of a) {}
