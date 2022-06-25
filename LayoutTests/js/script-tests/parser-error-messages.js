description("Tests error messages to make sure that they're sane");

function parseTest(source)
{
    try {
        eval(source);
    } catch (e) {
        return e.message
    }
}

shouldBe("parseTest('0x')", "\"No hexadecimal digits after '0x'\"");
shouldBe("parseTest('0xg')", "\"No hexadecimal digits after '0x'\"");
shouldBe("parseTest('0x1.2')", "\"Unexpected number '.2'. Parse error.\"");
shouldBe("parseTest('0x1g')", "\"No space between hexadecimal literal and identifier\"");
shouldBe("parseTest('0x1in')", "\"No space between hexadecimal literal and identifier\"");

