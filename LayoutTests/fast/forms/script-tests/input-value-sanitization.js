description('Tests for value sanitization algorithm.');

var input;

debug('');
debug('Number:');
input = document.createElement('input');
input.setAttribute('value', '65536');
input.type = 'number';
shouldBe('input.value', '"65536"');
shouldBe('input.value = "256"; input.value', '"256"');
shouldBe('input.value = ""; input.value', '""');


debug('');
debug('Range:');
input = document.createElement('input');
input.type = 'text';
input.value = ':)';
input.type = 'range';
shouldBe('input.value', '"50"');

// FIXME: Add more sanitization tests.
// https://bugs.webkit.org/show_bug.cgi?id=37024

var successfullyParsed = true;
