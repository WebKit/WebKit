description(
"This page tests assertions followed by quantifiers."
);

var regexp;

regexp = /(?=a){0}/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('a')");
shouldBe("regexp.lastIndex", "0");

regexp = /(?=a){1}/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('a')");
shouldBe("regexp.lastIndex", "0");

regexp = /(?!a){0}/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('b')");
shouldBe("regexp.lastIndex", "0");

regexp = /(?!a){1}/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('b')");
shouldBe("regexp.lastIndex", "0");

shouldBeTrue('/^(?=a)?b$/.test("b")');

var successfullyParsed = true;
