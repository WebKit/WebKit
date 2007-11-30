description(
'Tests that regular expressions treat non-BMP characters as two separate characters. '
+ 'From a Unicode correctness point of view this is wrong, but it is what other browsers do. '
+ 'And given that we store strings as UTF-16, it is also more efficient to implement. '
+ 'Also test some other cases related to UTF-8 and UTF-16.'
);

var surrogatePair = String.fromCharCode(0xD800) + String.fromCharCode(0xDC00);

shouldBe('/./.exec(surrogatePair).toString().length', '1');
shouldBe('/\\D/.exec(surrogatePair).toString().length', '1');
shouldBe('/\\S/.exec(surrogatePair).toString().length', '1');
shouldBe('/\\W/.exec(surrogatePair).toString().length', '1');
shouldBe('/[^x]/.exec(surrogatePair).toString().length', '1');

debug('');

shouldBe('/.{1,2}/.exec("!!" + String.fromCharCode(0xA1)).toString().length', '2');
shouldBe('/./.exec("")', 'null');

debug('');

var successfullyParsed = true;
