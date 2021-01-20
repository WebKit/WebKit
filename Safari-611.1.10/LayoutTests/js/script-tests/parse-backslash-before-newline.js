shouldBe('"test\
string with CR LF"', '"teststring with CR LF"');

shouldThrow(`"test\
string with LF CR"`, '"SyntaxError: Unexpected EOF"');

shouldBe('"test\string with CR"', '"teststring with CR"');

shouldBe('"test\
string with LF"', '"teststring with LF"');
