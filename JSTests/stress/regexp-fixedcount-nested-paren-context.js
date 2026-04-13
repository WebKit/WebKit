function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// FixedCount generic paren containing a nested ParenContext-using paren.
// The YARR JIT used to corrupt the nested paren's ParenContext freelist while
// backtracking the outer FixedCount group, leading to a null/garbage deref.
// rdar://173140757

for (let i = 0; i < 100; ++i) {
    // Non-greedy nested *?
    shouldBe(/((a)*?b){2}x/.exec("ababa"), null);
    shouldBe(/((a)*?b){2}x/.exec("ababax"), null);
    shouldBe(/((a)*?b){2}x/.exec("ababx"), ["ababx", "ab", "a"]);
    shouldBe(/((a)*?b){2}x/.exec("abaabx"), ["abaabx", "aab", "a"]);

    // Greedy nested *
    shouldBe(/((a)*b){2}x/.exec("aabaabab"), null);
    shouldBe(/((a)*b){2}x/.exec("aabaababx"), ["aababx", "ab", "a"]);

    // Bounded non-greedy
    shouldBe(/((a){0,2}?b){2}x/.exec("ababa"), null);

    // Non-capturing outer
    shouldBe(/(?:(a)*?b){2}x/.exec("ababa"), null);
    shouldBe(/(?:(a)*b){2}x/.exec("aabaabab"), null);

    // {3}
    shouldBe(/(((.)*?(.??))[a-c]){3}cp/.exec("aabbbccc"), null);

    // FixedCount cases that should still JIT and work.
    shouldBe(/(a+){2}b/.exec("aaab"), ["aaab", "a"]);
    shouldBe(/(a|aaa){2}b/.exec("aaab"), ["aab", "a"]);
    shouldBe(/(ab){2}c/.exec("ababc"), ["ababc", "ab"]);
}
