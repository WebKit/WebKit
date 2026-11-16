// Regression test: YARR JIT ParenContext use-after-free when
// clearInnerParenContextHeadSlots missed isCopy groups and sibling/
// ancestor-sibling groups. restoreParenContext restores ALL frame slots
// (including parenContextHead for siblings), but the old clearing function
// only walked the current group's inner disjunction and skipped isCopy
// terms, leaving stale pointers to freed/recycled ParenContext objects.

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// ---------------------------------------------------------------------------
// 1. PoC pattern from the bug report.
//    ((c|b)*?(y|x)+?.){3}mp
//    +? expands to {1,1} + *?(isCopy=true). The *? copy uses ParenContext
//    and is a sibling of (c|b)*?. Both bugs trigger: isCopy skip + sibling
//    scope mismatch.
// ---------------------------------------------------------------------------
(function testPoCPattern() {
    var re = new RegExp('((c|b)*?(y|x)+?.){3}mp');
    // Use short inputs to avoid catastrophic backtracking in JIT
    // while still exercising the isCopy + sibling clearing paths.
    var shortInputs = ['bx.cx.by.mp', 'by.cy.bx.mp', 'byx.cyx.bxy.mp', 'nope', 'ab', ''];

    // Warm up YARR JIT
    for (var i = 0; i < 200; i++) {
        for (var j = 0; j < shortInputs.length; j++) {
            re.test(shortInputs[j]);
            re.exec(shortInputs[j]);
        }
    }

    shouldBe(re.exec('nope'), null, "PoC pattern should not match");
    shouldBe(re.exec('bx.cx.by.mp'), ["bx.cx.by.mp", "by.", "b", "y"], "PoC pattern match");
})();

// ---------------------------------------------------------------------------
// 2. isCopy UAF: +? quantifier creates copy groups.
//    (a)+? expands to (a){1,1}(a)*? where *? is isCopy=true with
//    quantityMaxCount=infinite, uses full ParenContext.
// ---------------------------------------------------------------------------
(function testIsCopyPlusNonGreedy() {
    var re = /((a|b)+?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("acabcd");
        re.exec("abcbcd");
        re.test("aaabbbcaaabbbcd");
        re.exec("xyzxyz");
    }

    shouldBe(re.exec("acabcd"), ["acabcd", "abc", "b"], "isCopy +? test 1");
    shouldBe(re.exec("zzz"), null, "isCopy +? no match");
})();

// ---------------------------------------------------------------------------
// 3. isCopy UAF: +? with multi-alternative inside fixed-count outer.
//    Forces FixedCount backtracking path (call site 1) to restore
//    sibling copy group's parenContextHead.
// ---------------------------------------------------------------------------
(function testIsCopyInsideFixedCount() {
    var re = /((x|y)+?(a|b)+?.){3}end/;

    for (var i = 0; i < 200; i++) {
        re.exec("xaxbyaybxbend");
        re.exec("yyaaxxbbyyaaend");
        re.test("nope");
    }

    shouldBe(re.exec("nope"), null, "isCopy inside FixedCount no match");
})();

// ---------------------------------------------------------------------------
// 4. Sibling group UAF: Two non-greedy siblings at same nesting level.
//    Backtracking the first group restores frame slots including the
//    second group's parenContextHead.
// ---------------------------------------------------------------------------
(function testSiblingGroups() {
    var re = /((a)*?(b)*?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcabcd");
        re.exec("aabcbbcd");
        re.exec("cccd");
        re.test("abcabc");
    }

    shouldBe(re.exec("cccd"), ["ccd", "c", undefined, undefined], "sibling test minimal");
})();

// ---------------------------------------------------------------------------
// 5. Ancestor-sibling group UAF: Inner group and an ancestor's sibling
//    share frame slot range. Restoring the inner group's parent context
//    overwrites the ancestor-sibling's parenContextHead.
// ---------------------------------------------------------------------------
(function testAncestorSiblingGroups() {
    var re = /((?:(x)*?y)(z)*?w){2}end/;

    for (var i = 0; i < 200; i++) {
        re.exec("xywzwendxywzwend");
        re.exec("ywwendywwend");
        re.test("fail");
    }

    shouldBe(re.exec("fail"), null, "ancestor-sibling no match");
})();

// ---------------------------------------------------------------------------
// 6. Greedy +? with captures: verify correct capture values after fix.
//    (([a-c])b*?\2){3} - backreference requiring within-iteration
//    backtracking combined with copy expansion.
// ---------------------------------------------------------------------------
(function testGreedyCopyWithCaptures() {
    var re = /(([a-c])b*?\2){3}/;

    for (var i = 0; i < 200; i++)
        re.exec("ababbbcbc");

    shouldBe(re.exec("ababbbcbc"), ["ababbbcbc", "cbc", "c"], "greedy copy captures");
})();

// ---------------------------------------------------------------------------
// 7. Non-greedy {n,m} quantifier with n>0: creates copy with bounded max.
//    (a){2,5}? expands to (a){2,2}(a){0,3}*? (copy, maxCount=3).
// ---------------------------------------------------------------------------
(function testBoundedNonGreedyCopy() {
    var re = /((a|b){2,5}?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcaacd");
        re.exec("aabcbbbcd");
        re.test("abcd");
    }

    shouldBe(re.exec("abcabcd"), ["abcabcd", "abc", "b"], "bounded non-greedy copy");
})();

// ---------------------------------------------------------------------------
// 8. Three-level nesting: outer FixedCount > middle NonGreedy > inner
//    NonGreedy with copy. Exercises deep tree walking.
// ---------------------------------------------------------------------------
(function testDeepNesting() {
    var re = /(((a|b)*?c)+?d){2}e/;

    for (var i = 0; i < 200; i++) {
        re.exec("acdcde");
        re.exec("abcbcdacdcde");
        re.test("xxx");
    }

    shouldBe(re.exec("xxx"), null, "deep nesting no match");
})();

// ---------------------------------------------------------------------------
// 9. Stress: run many patterns in a tight loop to maximize ParenContext
//    free-list reuse and expose any remaining stale pointer dereferences.
// ---------------------------------------------------------------------------
(function testStress() {
    var patterns = [
        /((a|b)*?(c|d)+?.){3}zz/,
        /((x)*?(y)*?z){2}w/,
        /((?:(p|q)*?r)(s)*?t){2}end/,
        /((a|b){2,4}?c){3}d/,
        /((m|n)+?(o|p)+?q){2}r/,
    ];
    var inputs = [
        "abcdabcdabcdzz",
        "xyzxyzw",
        "prstendprstend",
        "abcabcabcd",
        "monopqmonopqr",
        "zzzzzzzzzz",
        "",
        "a",
    ];

    for (var i = 0; i < 500; i++) {
        var re = patterns[i % patterns.length];
        var input = inputs[i % inputs.length];
        re.test(input);
        re.exec(input);
    }
})();

// ---------------------------------------------------------------------------
// 10. Non-greedy multi-alt group inside FixedCount: existing regression
//     pattern from the original fix. Verify it still works.
// ---------------------------------------------------------------------------
(function testOriginalRegressionPatterns() {
    /(?:(a|b)*?.){2}xx/.exec('aabbcc');
    /((a|b)*?[a-c]){3}cp/.exec('aabbbccc');
    /((a|b)*?.){2}cp/.exec('aabbbccc');

    var r5 = /((a|b)*?(.)??.){3}cp/s;
    shouldBe(r5.exec('aabbbccc'), null, "original regression 1");
    shouldBe(r5.exec('aabbccc'), null, "original regression 2");

    // Run many times to ensure JIT path is exercised.
    for (var i = 0; i < 200; i++) {
        /(?:(a|b)*?.){2}xx/.exec('aabbcc');
        /((a|b)*?[a-c]){3}cp/.exec('aabbbccc');
        r5.exec('aabbbccc');
    }
})();

// ===========================================================================
// Frame-location ordering invariant tests:
// restoreParenContext only restores frame slots from
// [currentGroup.frameLocation + 4, end). Previous siblings have lower
// frame locations and are never in the restore range, so their
// parenContextHead is never overwritten. These tests exercise patterns
// where previous-sibling NonGreedy/FixedCount groups must retain valid
// state after a later sibling or the outer group backtracks.
// ===========================================================================

// ---------------------------------------------------------------------------
// 11. Three NonGreedy siblings — when (c)*? or outer group backtracks,
//     (a)*? and (b)*? frame slots (lower addresses) must be unaffected.
// ---------------------------------------------------------------------------
(function testThreeNonGreedySiblings() {
    var re = /((a)*?(b)*?(c)*?d){2}e/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcdabcde");
        re.exec("adbde");
        re.exec("dde");
        re.test("nope");
    }

    shouldBe(re.exec("abcdabcde"), ["abcdabcde","abcd","a","b","c"], "three siblings full");
    shouldBe(re.exec("adbde"), ["adbde","bd",null,"b",null], "three siblings partial");
    shouldBe(re.exec("dde"), ["dde","d",null,null,null], "three siblings minimal");
    shouldBe(re.exec("nope"), null, "three siblings no match");
})();

// ---------------------------------------------------------------------------
// 12. Previous-sibling FixedCount with later NonGreedy sibling.
//     (a)+ is Greedy (previous sibling, lower frame), (b)*? is NonGreedy
//     (later sibling). Outer FixedCount backtrack restores (b)*?'s range
//     but (a)+'s frame is below that range.
// ---------------------------------------------------------------------------
(function testFixedCountPrevSiblingNonGreedyLater() {
    var re = /((a)+.(b)*?c){2}d/;

    for (var i = 0; i < 200; i++) {
        re.exec("aa.bcaa.bcd");
        re.exec("a.ca.cd");
        re.exec("a.bca.bcd");
        re.test("nope");
    }

    shouldBe(re.exec("aa.bcaa.bcd"), ["aa.bcaa.bcd","aa.bc","a","b"], "FC prev sibling full");
    shouldBe(re.exec("a.ca.cd"), ["a.ca.cd","a.c","a",null], "FC prev sibling no b");
    shouldBe(re.exec("a.bca.bcd"), ["a.bca.bcd","a.bc","a","b"], "FC prev sibling with b");
    shouldBe(re.exec("nope"), null, "FC prev sibling no match");
})();

// ---------------------------------------------------------------------------
// 13. Four NonGreedy siblings — stress that clearing only nulls slots in
//     [parenthesesFrameLocation+4, end), leaving earlier siblings intact.
// ---------------------------------------------------------------------------
(function testFourNonGreedySiblings() {
    var re = /((a)*?(b)*?(c)*?(d)*?e){2}f/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcdeabcdef");
        re.exec("aeaef");
        re.exec("eef");
        re.test("nope");
    }

    shouldBe(re.exec("abcdeabcdef"), ["abcdeabcdef","abcde","a","b","c","d"], "four siblings full");
    shouldBe(re.exec("aeaef"), ["aeaef","ae","a",null,null,null], "four siblings sparse");
    shouldBe(re.exec("eef"), ["eef","e",null,null,null,null], "four siblings minimal");
    shouldBe(re.exec("nope"), null, "four siblings no match");
})();

// ---------------------------------------------------------------------------
// 14. Previous sibling with deep nesting: (a)*? is nested inside the first
//     child group, (c)*? is a sibling. Backtracking (c)*? doesn't touch
//     (a)*?'s deeper frame location.
// ---------------------------------------------------------------------------
(function testDeepNestedPrevSibling() {
    var re = /(((a)*?b)(c)*?d){2}e/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcdabcde");
        re.exec("abdabde");
        re.test("bdde");
        re.test("nope");
    }

    shouldBe(re.exec("abcdabcde"), ["abcdabcde","abcd","ab","a","c"], "deep nested prev full");
    shouldBe(re.exec("abdabde"), ["abdabde","abd","ab","a",null], "deep nested prev no c");
    shouldBe(re.exec("bdde"), null, "deep nested prev no match");
    shouldBe(re.exec("nope"), null, "deep nested prev nope");
})();

// ---------------------------------------------------------------------------
// 15. Copy group as previous sibling: +? expands to {1,1} + *?(isCopy).
//     The isCopy group is a previous sibling of (c)*?. Confirm isCopy
//     group's parenContextHead is preserved when (c)*? backtracks.
// ---------------------------------------------------------------------------
(function testCopyGroupPrevSibling() {
    var re = /((a|b)+?(c)*?d){2}e/;

    for (var i = 0; i < 200; i++) {
        re.exec("acdacde");
        re.exec("abdabde");
        re.exec("bcdade");
        re.test("nope");
    }

    shouldBe(re.exec("acdacde"), ["acdacde","acd","a","c"], "copy prev sibling full");
    shouldBe(re.exec("abdabde"), ["abdabde","abd","b",null], "copy prev sibling no c");
    shouldBe(re.exec("bcdade"), ["bcdade","ad","a",null], "copy prev sibling mixed");
    shouldBe(re.exec("nope"), null, "copy prev sibling no match");
})();

// ---------------------------------------------------------------------------
// 16. Alternation with sibling groups in different alternatives.
//     (a)*? and (b)*? are in different alternatives of an inner group.
//     Exercises that clearParenContextHeadSlotsInRange walks the correct
//     alternative subtree.
// ---------------------------------------------------------------------------
(function testAlternationSiblingGroups() {
    var re = /((?:(a)*?x|(b)*?y)z){2}w/;

    for (var i = 0; i < 200; i++) {
        re.exec("axzbyzw");
        re.exec("xzyzw");
        re.exec("aaxzbbyzw");
        re.test("nope");
    }

    shouldBe(re.exec("axzbyzw"), ["axzbyzw","byz",null,"b"], "alt siblings mixed");
    shouldBe(re.exec("xzyzw"), ["xzyzw","yz",null,null], "alt siblings empty");
    shouldBe(re.exec("aaxzbbyzw"), ["aaxzbbyzw","bbyz",null,"b"], "alt siblings multi");
    shouldBe(re.exec("nope"), null, "alt siblings no match");
})();

// ---------------------------------------------------------------------------
// 17. Large sibling chain with full capture verification: three NonGreedy
//     siblings inside FixedCount{3}. Verify ALL capture groups match
//     interpreter output across multiple inputs.
// ---------------------------------------------------------------------------
(function testLargeSiblingChainCaptures() {
    var re = /((a)*?(b)*?(c)*?x){3}y/;

    for (var i = 0; i < 200; i++) {
        re.exec("abcxabcxabcxy");
        re.exec("axbxcxy");
        re.exec("xxxy");
        re.test("nope");
    }

    shouldBe(re.exec("abcxabcxabcxy"), ["abcxabcxabcxy","abcx","a","b","c"], "large chain full");
    shouldBe(re.exec("axbxcxy"), ["axbxcxy","cx",null,null,"c"], "large chain sparse");
    shouldBe(re.exec("xxxy"), ["xxxy","x",null,null,null], "large chain minimal");
    shouldBe(re.exec("nope"), null, "large chain no match");
})();
