//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Test FixedCount parentheses containing nested NonGreedy multi-alternative groups.
// When the FixedCount group backtracks, it restores frame state (including nested
// groups' returnAddress slots). If the nested NonGreedy group matched 0 iterations
// in that snapshot, its returnAddress was never set by NestedAlternativeBegin/Next/End.
// The JIT must handle this gracefully (bail to interpreter) rather than jumping to
// an uninitialized code pointer.

// Minimized from rdar://173140757: the "cp" suffix forces FixedCount backtracking
// because it can never match "aabbbccc", exercising the restore path.
(function() {
    var r = /((()*.|.)*?[a-c]){2}cp/s;
    var result = r.exec('aabbbccc');
    shouldBe(result, null, "Minimized: FixedCount {2} + NonGreedy *? multi-alt with capture");
})();

// Original fuzzer test case.
(function() {
    var r = new RegExp(unescape('%28%28%28%29%28%29%28%28%28%29%28%29%2A%2E%7C%28%29%28%68%3E%28%28%28%29%28%29%28%29%28%29%29%2F%6C%2E%28%1F%28%29%28%2D%28%00%5B%5C%44%C3%5D%6F%5E%8E%28%29%29%28%29%21%29%28%29%29%29%38%7E%29%40%77%EB%29%2A%3F%28%28%3F%3A%2E%29%3F%3F%29%29%5B%0A%61%2D%63%5D%29%29%7B%33%7D%63%70'),'dimsu');
    'aabbbccc'.match(r);
    'aabbbccc'.replace(r,'$1,$2');
    r.exec('caffeeeee');
})();

// Variant: {3} with the same nested structure.
(function() {
    var r = /((()*.|.)*?[a-c]){3}cp/s;
    var result = r.exec('aabbbccc');
    shouldBe(result, null, "FixedCount {3} + NonGreedy *? multi-alt with capture");
})();
