description(
'This tests some sparse array operations.'
);

var array = [ ];

array[50000] = 100;

shouldBe('array[0]', 'undefined');
shouldBe('array[49999]', 'undefined');
shouldBe('array[50000]', '100');
shouldBe('array[50001]', 'undefined');
array[0]++;
shouldBe('array[0]', 'NaN');
shouldBe('array[49999]', 'undefined');
shouldBe('array[50000]', '100');
shouldBe('array[50001]', 'undefined');

debug('');

successfullyParsed = true;
