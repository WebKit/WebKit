var x = 0;
eval("\u0009 \u000B \u000C \u00A0x = 1;");
shouldBe("x", "1");

// hex (non-normative)
shouldBe("0x0", "0");
shouldBe("0xF", "15");
shouldBe("0xFF", "255");

// octal (non-normative)
shouldBe("01", "1");
shouldBe("010", "8");
shouldBe("09", "9");
shouldBe("019", "19");
successfullyParsed = true
