description(
"This page tests for proper backtracking with greedy quantifiers and non-capturing parentheses."
);

var re = /(?:a*)a/;
shouldBe("re.exec('a')", "['a']");

var successfullyParsed = true;
