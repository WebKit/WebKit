description('Test for HTMLTextAreaElement.textLength');

var textArea = document.createElement('textarea');
document.body.appendChild(textArea);
shouldBe('textArea.textLength', '0');

textArea.value = 'abcd';
shouldBe('textArea.textLength', '4');

textArea.focus();
eventSender.keyDown('e', []);
shouldBe('textArea.textLength', '5');

// Test for a character larger than U+FFFF.
textArea = document.createElement('textarea');
textArea.innerHTML = '&#x1d538;';
// Firefox 3.5 and Opera 10 return 2 for 1 surrogate pair.
shouldBe('textArea.textLength', '2');

// Test for combined characters.
textArea = document.createElement('textarea');
// U+3055 Hiragana Letter Sa
// U+3099 Combining Katakana-HIragana Voiced Sound Mark
textArea.innerHTML = '&#x3055;&#x3099;';
// Firefox 3.5 seems to apply NFC for the value, and .textLength and .value.length is 1.
// Opera 10 returns 2, and IE's .value.length is 2.
shouldBe('textArea.textLength', '2');

var successfullyParsed = true;

