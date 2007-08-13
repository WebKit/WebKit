description(
'Test for behavior of non-capturing groups, as described in <a href="http://blog.stevenlevithan.com/archives/npcg-javascript">' +
'a blog post by Steven Levithan</a> and <a href="http://bugs.webkit.org/show_bug.cgi?id=14931">bug 14931</a>.'
);

shouldBe('/(x)?\\1y/.test("y")', 'true');
shouldBe('/(x)?\\1y/.exec("y")', '["y", undefined]');
shouldBe('/(x)?y/.exec("y")', '["y", undefined]');
shouldBe('"y".match(/(x)?\\1y/)', '["y", undefined]');
shouldBe('"y".match(/(x)?y/)', '["y", undefined]');
shouldBe('"y".match(/(x)?\\1y/g)', '["y"]');
shouldBe('"y".split(/(x)?\\1y/)', '["", undefined, ""]');
shouldBe('"y".split(/(x)?y/)', '["", undefined, ""]');
shouldBe('"y".search(/(x)?\\1y/)', '0');
shouldBe('"y".replace(/(x)?\\1y/, "z")', '"z"');
shouldBe('"y".replace(/(x)?y/, "$1")', '""');
shouldBe('"y".replace(/(x)?\\1y/, function($0, $1){ return String($1); })', '"undefined"');
shouldBe('"y".replace(/(x)?y/, function($0, $1){ return String($1); })', '"undefined"');
shouldBe('"y".replace(/(x)?y/, function($0, $1){ return $1; })', '"undefined"');

var successfullyParsed = true;
