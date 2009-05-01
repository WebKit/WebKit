description(

'Test for rdar:/68455379, a case-insensitive regex containing a character class containing a range with an upper bound of \uFFFF can lead to an infinite-loop.'

);

shouldBe('("A".match(/[\u0001-\uFFFF]/i) == "A")', 'true');

var successfullyParsed = true;
