description('Check stepMismatch results for type=range.');

var input = document.createElement('input');
// Needs a renderer.
document.body.appendChild(input);
input.type = 'range';

// The following test inputs are the same as ValidityState-stepMismatch-number.js,
// but all expected results should be 'false'.

// If the value is empty, it never mismatches.
input.value = '';
shouldBe('input.validity.stepMismatch', 'false');
input.min = '0.1';
input.step = '1.0';
shouldBe('input.validity.stepMismatch', 'false');

// Integers
input.value = '1';
input.min = '0';
input.step = '2';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '-3';
input.min = '-4';
input.step = '2';
shouldBe('input.validity.stepMismatch', 'false');
input.min = '0';
input.max = '5';
input.step = '3';
input.value = '5';
shouldBe('input.validity.stepMismatch', 'false');
shouldBe('input.value', '"3"');
// Invalid step attribute is ignored.
input.value = '-3';
input.min = '-4';
input.max = '';
input.step = '-2';
shouldBe('input.validity.stepMismatch', 'false');
input.step = null;
shouldBe('input.validity.stepMismatch', 'false');
input.step = undefined;
shouldBe('input.validity.stepMismatch', 'false');
// Huge numbers and small step; uncomparable
input.value = '1.7976931348623157e+308';
input.min = '';
input.step = '3';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '1.7976931348623156e+308';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '1.7976931348623155e+308';
shouldBe('input.validity.stepMismatch', 'false');
// Huge numbers and huge step
input.value = '1.60e+308';
input.step = '0.20e+308';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '1.60e+308';
input.step = '0.22e+308';
shouldBe('input.validity.stepMismatch', 'false');

// Fractional numbers
input.value = '0.9';
input.step = '0.1';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '0.9';
input.step = '0.1000001';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '0.9';
input.step = '0.1000000000000001';
shouldBe('input.validity.stepMismatch', 'false');
input.value = '1.0';
input.step = '0.3333333333333333';
shouldBe('input.validity.stepMismatch', 'false');

var successfullyParsed = true;
