description('Check if the pattern constraint is not applied to some input types.');

var input = document.createElement('input');
input.type = 'color';
input.pattern = '#[0-9A-F]{6}';  // Restrict to capital letters
input.value = '#0099ff';

// pattern doesn't work for type=color
shouldBe('input.validity.patternMismatch', 'false');

// works for type=text.
input.type = 'text';
shouldBe('input.validity.patternMismatch', 'true');

var successfullyParsed = true;

