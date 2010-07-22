description('Tests for value sanitization algorithm.');

var input = document.createElement('input');

input.type = 'text';
input.value = ':)';

input.type = 'range';
shouldBe('input.value', '"50"');

// FIXME: Add more sanitization tests.
// https://bugs.webkit.org/show_bug.cgi?id=37024

var successfullyParsed = true;
