description('Test JS parser handling of regex literals starting with /=');

shouldBe("/=/.toString()", "'/=/'");
shouldBeFalse("/=/.test('')");
shouldBeTrue("/=/.test('=')");
shouldBe("'='.match(/=/)", "['=']");
shouldBe("'='.match(/\\=/)", "['=']");

var successfullyParsed = true;
