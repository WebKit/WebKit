description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=39289">Wrong result in case of non-iterative matching of subpatterns in non-greedy cases in YARR Interpreter</a>'
);

shouldBe('"a".match(/(a)??/)', '["", undefined]');
shouldBe('"b".match(/(a)??/)', '["", undefined]');
shouldBe('"ab".match(/(a)??b/)', '["ab", "a"]');
shouldBe('"aaab".match(/(a+)??b/)', '["aaab", "aaa"]');
shouldBe('"abbc".match(/(a)??(b+)??c/)', '["abbc", "a", "bb"]');
shouldBe('"ac".match(/(a)??(b)??c/)', '["ac", "a", undefined]');
shouldBe('"abc".match(/(a(b)??)??c/)', '["abc", "ab", "b"]');
shouldBe('"ac".match(/(a(b)??)??c/)', '["ac", "a", undefined]');
