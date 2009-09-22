description(
"This test checks the case-insensitive matching of character literals."
);

shouldBeTrue("/\u00E5/i.test('/\u00E5/')");
shouldBeTrue("/\u00E5/i.test('/\u00C5/')");
shouldBeTrue("/\u00C5/i.test('/\u00E5/')");
shouldBeTrue("/\u00C5/i.test('/\u00C5/')");

shouldBeFalse("/\u00E5/i.test('P')");
shouldBeFalse("/\u00E5/i.test('PASS')");
shouldBeFalse("/\u00C5/i.test('P')");
shouldBeFalse("/\u00C5/i.test('PASS')");

shouldBeNull("'PASS'.match(/\u00C5/i)");
shouldBeNull("'PASS'.match(/\u00C5/i)");

shouldBe("'PAS\u00E5'.replace(/\u00E5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00E5'.replace(/\u00C5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00C5'.replace(/\u00E5/ig, 'S')", "'PASS'");
shouldBe("'PAS\u00C5'.replace(/\u00C5/ig, 'S')", "'PASS'");

shouldBe("'PASS'.replace(/\u00E5/ig, '%C3%A5')", "'PASS'");
shouldBe("'PASS'.replace(/\u00C5/ig, '%C3%A5')", "'PASS'");

var successfullyParsed = true;
