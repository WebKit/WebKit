description(
"This test checks whether converting function code to a string preserves semantically significant parentheses."
)

shouldBeTrue("(function () { return (x + y) * z; }).toString().search('return.*\\(') < 0");

var successfullyParsed = true;
