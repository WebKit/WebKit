// Benchmark: String.prototype.startsWith with single char on rope strings.
// Simulates checking the leading character of dynamically built strings
// (e.g. protocol detection, path classification) without resolving the rope.

// A resolved base (~300 chars), just above the minLengthForRopeWalk threshold (296).
let base = "https://example.com/api/v2/users?query=" + "a".repeat(260) + "&token=xyz";
base.charCodeAt(0); // Force resolve

function startsWithMatch(a, b) {
    return (a + b).startsWith("h");
}
noInline(startsWithMatch);

function startsWithMismatch(a, b) {
    return (a + b).startsWith("/");
}
noInline(startsWithMismatch);

let suffix = "&page=1";
for (let i = 0; i < 5e4; i++) {
    startsWithMatch(base, suffix);
    startsWithMismatch(base, suffix);
}
