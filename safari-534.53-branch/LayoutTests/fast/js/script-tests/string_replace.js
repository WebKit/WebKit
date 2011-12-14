description(
"This will test string.replace with {n, m} regexp patterns."
);

shouldBe('"YY".replace(/Y{1,4}/g,"YYYY")', '"YYYY"');
shouldBe('"MM".replace(/M{1,2}/g,"M")', '"M"');
shouldBe('"YY".replace(/Y{1,4}/g,"MMMM")', '"MMMM"');

var successfullyParsed = true;
