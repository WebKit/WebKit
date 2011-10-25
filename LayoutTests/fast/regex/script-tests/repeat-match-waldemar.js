description(
"Some test cases identified by Waldemar Horwat in response to this bug: https://bugs.webkit.org/show_bug.cgi?id=48101"
);

shouldBe('/(?:a*?){2,}/.exec("aa")', '["aa"]');
shouldBe('/(?:a*?){2,}/.exec("a")', '["a"]');
shouldBe('/(?:a*?){2,}/.exec("")', '[""]');

shouldBe('/(?:a*?)/.exec("aa")', '[""]');
shouldBe('/(?:a*?)/.exec("a")', '[""]');
shouldBe('/(?:a*?)/.exec("")', '[""]');

shouldBe('/(?:a*?)(?:a*?)(?:a*?)/.exec("aa")', '[""]');
shouldBe('/(?:a*?)(?:a*?)(?:a*?)/.exec("a")', '[""]');
shouldBe('/(?:a*?)(?:a*?)(?:a*?)/.exec("")', '[""]');

shouldBe('/(?:a*?){2}/.exec("aa")', '[""]');
shouldBe('/(?:a*?){2}/.exec("a")', '[""]');
shouldBe('/(?:a*?){2}/.exec("")', '[""]');

shouldBe('/(?:a*?){2,3}/.exec("aa")', '["a"]');
shouldBe('/(?:a*?){2,3}/.exec("a")', '["a"]');
shouldBe('/(?:a*?){2,3}/.exec("")', '[""]');

shouldBe('/(?:a*?)?/.exec("aa")', '["a"]');
shouldBe('/(?:a*?)?/.exec("a")', '["a"]');
shouldBe('/(?:a*?)?/.exec("")', '[""]');

shouldBe('/(?:a*?)*/.exec("aa")', '["aa"]');
shouldBe('/(?:a*?)*/.exec("a")', '["a"]');
shouldBe('/(?:a*?)*/.exec("")', '[""]');
