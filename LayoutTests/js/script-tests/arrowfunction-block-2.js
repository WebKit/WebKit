description('Tests for ES6 arrow function declaration body as block');

var af = a => { a + 1; };

shouldBe("typeof af(0)", '"undefined"');

var successfullyParsed = true;
