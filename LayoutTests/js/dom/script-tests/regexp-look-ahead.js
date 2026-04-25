description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=41458">Yarr Interpreter is crashing in some cases of look-ahead regex patterns</a>'
);

shouldBe('"ab".match(/a(?=b|c)/)', '["a"]');
shouldBe('"abd".match(/a(?=c|b)|d/)', '["a"]');
