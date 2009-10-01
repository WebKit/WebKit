description('Tests for tooLong flag with &lt;input> elements.');

var input = document.createElement('input');
document.body.appendChild(input);

// No maxlength and no value
shouldBeFalse('input.validity.tooLong');

// Non-dirty value.
input.setAttribute('value', 'abcde');
input.maxLength = 3;
shouldBe('input.value.length', '5');
shouldBeFalse('input.validity.tooLong');

input.setAttribute('value', 'abcdef');
shouldBe('input.value.length', '6');
shouldBeFalse('input.validity.tooLong');

// Dirty value and longer than maxLength.
input = document.createElement('input');
document.body.appendChild(input);
input.setAttribute('value', 'abcde');
input.maxLength = 3;
input.focus();
input.setSelectionRange(5, 5);  // Move the cursor at the end.
document.execCommand('delete');
shouldBe('input.value.length', '4');
shouldBeTrue('input.validity.tooLong');
// Make the value <=maxLength.
document.execCommand('delete');
shouldBeFalse('input.validity.tooLong');

// Sets a value via DOM property.
input.value = 'abcde';
shouldBeTrue('input.validity.tooLong');

// Change the type with a too long value.
input.type = 'search';
shouldBeTrue('input.validity.tooLong');

input.type = 'number';
shouldBeFalse('input.validity.tooLong');

var successfullyParsed = true;
