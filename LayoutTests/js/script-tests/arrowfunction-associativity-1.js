description('Tests for ES6 arrow function nested declaration');

var af1, af2, af3;
af1 = af2 = af3 => af1 = af2 = af3;
debug('af1 = af2 = af3 => af1 = af2 = af3')
shouldBe('af1', 'af2');
af1(13);
shouldBe('af2', '13');
shouldBe('af1', '13');

var successfullyParsed = true;
