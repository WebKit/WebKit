// Benchmark for String.prototype.toUpperCase

function test(string) {
    return string.toUpperCase();
}
noInline(test);

// Create strings dynamically to avoid constant folding
function makeString(base) {
    return base + "";
}
noInline(makeString);

// Already uppercase - exercises the JIT fast path (no C++ call)
const upperString = makeString("FOO");
// uppercase prefix with late lowercase - exercises failingIndex optimization
const mixedString = makeString("Foo");

for (var i = 0; i < 1e6; ++i) {
    test(upperString);
    test(mixedString);
}
