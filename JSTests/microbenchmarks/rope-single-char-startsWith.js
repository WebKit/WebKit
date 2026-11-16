// (a + b) creates a fresh shallow rope each iteration; the single-char fast
// path in startsWith uses JSString::tryGetCharAt to avoid resolving it.
let base = "-".padEnd(1000, "hello ");
base.charCodeAt(0);

function testHit(a, b) { return (a + b).startsWith("-"); }
noInline(testHit);

function testMiss(a, b) { return (a + b).startsWith("z"); }
noInline(testMiss);

for (let i = 0; i < 1e5; ++i) {
    testHit(base, "world");
    testMiss(base, "world");
}
