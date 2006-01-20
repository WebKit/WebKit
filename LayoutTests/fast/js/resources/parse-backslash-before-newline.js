shouldBe('"test\
string with CR LF"', '"teststring with CR LF"');

shouldBe('"test\
string with LF CR"', '"teststring with LF CR"');

shouldBe('"test\string with CR"', '"teststring with CR"');

shouldBe('"test\
string with LF"', '"teststring with LF"');

var successfullyParsed = true;
