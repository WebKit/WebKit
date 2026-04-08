// Benchmark: String.prototype.endsWith with single char on rope strings.
// Simulates checking the trailing character of dynamically built strings
// (e.g. trailing slash detection, line ending check) without resolving the rope.

// A resolved base (~300 chars), just above the minLengthForRopeWalk threshold (296).
let base = "https://example.com/api/v2/users?query=" + "a".repeat(260) + "&token=xyz";
base.charCodeAt(0); // Force resolve

function endsWithMatch(a, b) {
    return (a + b).endsWith("/");
}
noInline(endsWithMatch);

function endsWithMismatch(a, b) {
    return (a + b).endsWith("?");
}
noInline(endsWithMismatch);

let suffix = "/";
for (let i = 0; i < 5e4; i++) {
    endsWithMatch(base, suffix);
    endsWithMismatch(base, suffix);
}
