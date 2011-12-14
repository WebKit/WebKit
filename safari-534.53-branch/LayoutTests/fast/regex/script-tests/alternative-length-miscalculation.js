description(
"This page tests for length miscalculations in regular expression processing."
);

var re = /b|[^b]/g;
re.lastIndex = 1;
shouldBe("re.exec('a')", "null");

var re2 = /[^a]|ab/;
shouldBe("re2.test('')", "false");

var successfullyParsed = true;
