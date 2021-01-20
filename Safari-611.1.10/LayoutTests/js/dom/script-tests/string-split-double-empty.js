description(
'This test checks for a regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=44600">split() function returns wrong answer for second empty split</a>.'
);

shouldBe('"".split(/s+/)', '[""]');
shouldBe('"".split(/s+/)', '[""]');
