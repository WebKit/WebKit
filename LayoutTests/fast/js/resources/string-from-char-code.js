description(
"This test ensures that String.fromCharCode doesn't crash."
);

shouldBe('String.fromCharCode(88)', '"X"');

var successfullyParsed = true;
