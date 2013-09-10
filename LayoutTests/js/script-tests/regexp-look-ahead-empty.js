description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=42664">Yarr Interpreter is hanging in some cases of look-ahead regex patterns</a>.  It also tests some other related expressions.'
);

shouldBe('/(?:(?=x))+/.exec("x")', '[""]');
shouldBe('/(?:a?)*/.exec("a")', '["a"]');
shouldBe('/(a|ab)*/.exec("abab")', '["a","a"]');
shouldBe('/(ab)+/.exec("abab")', '["abab","ab"]');
// The following tests fail because of pcre interpreter bug(s).
shouldBe('/(|ab)*/.exec("ab")', '["ab","ab"]');
shouldBe('/(?:(|ab)*)/.exec("ab")', '["ab","ab"]');
shouldBe('/(?:(|ab)+)/.exec("ab")', '["ab","ab"]');
shouldBe('/(|ab)+/.exec("abab")', '["abab","ab"]');
