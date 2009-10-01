description('Tests for tooLong flag with &lt;textarea> elements.');

var textarea = document.createElement('textarea');
document.body.appendChild(textarea);

// No maxlength and no value
shouldBeFalse('textarea.validity.tooLong');

// Non-dirty value.
textarea.defaultValue = 'abcde';
textarea.maxLength = 3;
shouldBe('textarea.value.length', '5');
shouldBeFalse('textarea.validity.tooLong');

textarea.defaultValue = 'abcdef';
shouldBe('textarea.value.length', '6');
shouldBeFalse('textarea.validity.tooLong');

// Dirty value and longer than maxLength.
textarea = document.createElement('textarea');
document.body.appendChild(textarea);
textarea.defaultValue = 'abcde';
textarea.maxLength = 3;
textarea.focus();
textarea.setSelectionRange(5, 5);  // Move the cursor at the end.
document.execCommand('delete');
shouldBe('textarea.value.length', '4');
shouldBeTrue('textarea.validity.tooLong');
// Make the value <=maxLength.
document.execCommand('delete');
shouldBeFalse('textarea.validity.tooLong');

// Sets a value via DOM property.
textarea.value = 'abcde';
shouldBeTrue('textarea.validity.tooLong');

var successfullyParsed = true;
