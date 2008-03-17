description("Test converting strings and integers to identifiers and back")

embed = document.createElement("embed");
embed.type = "application/x-webkit-test-netscape";
document.body.appendChild(embed);

shouldBeEqualToString("embed.testIdentifierToString('foo')", "foo");
shouldBeEqualToString("embed.testIdentifierToString('123')", "123");
shouldBeEqualToString("embed.testIdentifierToString('null')", "null");

shouldBe("embed.testIdentifierToString(1)", "undefined");
shouldBe("embed.testIdentifierToString(-1)", "undefined");
shouldBe("embed.testIdentifierToString(1.40)", "undefined");

shouldBe("embed.testIdentifierToInt(1)", "1");
shouldBe("embed.testIdentifierToInt(-1)", "-1");
shouldBe("embed.testIdentifierToInt(10)", "10");
shouldBe("embed.testIdentifierToInt(10.234234)", "10");

shouldBe("embed.testIdentifierToInt('foo')", "0");
shouldBe("embed.testIdentifierToInt('10')", "0");

var successfullyParsed = true;
