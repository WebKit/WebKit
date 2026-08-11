// (a + b) creates a fresh shallow rope each iteration; the single-char fast
// path in endsWith uses JSString::tryGetCharAt to avoid resolving it.
let base = "x".padEnd(1000, "hello ");
base.charCodeAt(0);

function testHit(a, b) { return (a + b).endsWith("d"); }
noInline(testHit);

function testMiss(a, b) { return (a + b).endsWith("?"); }
noInline(testMiss);

for (let i = 0; i < 1e5; ++i) {
    testHit(base, "world");
    testMiss(base, "world");
}
