//@ requireOptions("--jitPolicyScale=0")

// This test should not crash.

function test(array) {
    return array.push(0, 0.1);
}

for (var i = 0; i < testLoopCount; ++i) {
    test([])
}

for (var i = 0; i < testLoopCount; ++i) {
    test([0])
}
