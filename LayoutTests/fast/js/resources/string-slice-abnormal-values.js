description(
"This test checks for handling of abnormal values passed to String.slice"
);

shouldBe('"abc".slice(0)', '"abc"');
shouldBe('"abc".slice(0, Infinity)', '"abc"');

var successfullyParsed = true;
