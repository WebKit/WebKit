description(
"This page tests for a length miscalculation in regular expression processing."
);

var re = /b|[^b]/g;
re.lastIndex = 1;
shouldBe("re.exec('a')", "null");

var successfullyParsed = true;
