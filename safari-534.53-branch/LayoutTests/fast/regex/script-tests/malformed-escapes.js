description(
"This page tests handling of malformed escape sequences."
);

var regexp;

regexp = /\ug/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('ug')");
shouldBe("regexp.lastIndex", "2");

regexp = /\xg/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('xg')");
shouldBe("regexp.lastIndex", "2");

regexp = /\c_/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('\\\\c_')");
shouldBe("regexp.lastIndex", "3");

regexp = /[\B]/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('B')");
shouldBe("regexp.lastIndex", "1");

regexp = /[\b]/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('\\b')");
shouldBe("regexp.lastIndex", "1");

regexp = /\8/gm;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('\\\\8')");
shouldBe("regexp.lastIndex", "2");

regexp = /^[\c]$/;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('c')");

regexp = /^[\c_]$/;
debug("\nTesting regexp: " + regexp);
shouldBeFalse("regexp.test('c')");

regexp = /^[\c]]$/;
debug("\nTesting regexp: " + regexp);
shouldBeTrue("regexp.test('c]')");

var successfullyParsed = true;
