// This test should not crash.

try {
    let x = eval("0o19");
} catch(e) {
}

try {
    let x = eval("0b19");
} catch(e) {
}
