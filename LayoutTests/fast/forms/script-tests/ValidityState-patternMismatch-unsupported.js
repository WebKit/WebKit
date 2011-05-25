description('Check if the pattern constraint is not applied to some input types.');

var input = document.createElement('input');
input.type = 'range';
input.pattern = '[0-9]';  // Restrict to single digit
input.value = '11';

// pattern doesn't work for type=range
shouldBe('input.validity.patternMismatch', 'false');

// works for type=text.
input.type = 'text';
shouldBe('input.validity.patternMismatch', 'true');

var successfullyParsed = true;

