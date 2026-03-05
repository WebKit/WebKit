// Benchmark: String.prototype.indexOf with single char on a fresh rope each iteration.
// This tests the target use case: (a + b + c).indexOf("x") where the rope is temporary.
// The base string is pre-resolved so that (base + "z") creates a shallow depth-1 rope.

let base = (-1).toLocaleString().padEnd(315241, "hello ");
base.charCodeAt(0); // Force resolve so (base + suffix) creates a shallow rope

function testFoundEarly(a, b) {
    return (a + b).indexOf("h");
}
noInline(testFoundEarly);

function testNotFound(a, b) {
    return (a + b).indexOf("Q");
}
noInline(testNotFound);

for (let i = 0; i < 1e2; i++) {
    testFoundEarly(base, "z");
    testNotFound(base, "z");
}
