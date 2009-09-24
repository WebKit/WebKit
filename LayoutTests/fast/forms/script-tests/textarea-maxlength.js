description('Tests for HTMLTextAreaElement.maxLength behaviors.');

var textArea = document.createElement('textarea');
document.body.appendChild(textArea);

// No maxlength attribute
shouldBe('textArea.maxLength', '0');

// Invalid maxlength attributes
textArea.setAttribute('maxlength', '-3');
shouldBe('textArea.maxLength', '0');
textArea.setAttribute('maxlength', 'xyz');
shouldBe('textArea.maxLength', '0');

// Valid maxlength attributes
textArea.setAttribute('maxlength', '1');
shouldBe('textArea.maxLength', '1');
textArea.setAttribute('maxlength', '256');
shouldBe('textArea.maxLength', '256');

// Set values to .maxLength
textArea.maxLength = 13;
shouldBe('textArea.getAttribute("maxlength")', '"13"');

textArea.maxLength = -1;
shouldBe('textArea.maxLength', '4294967295');
shouldBe('textArea.getAttribute("maxlength")', '"4294967295"');

// maxLength doesn't truncate the default value.
textArea = document.createElement('textarea');
textArea.setAttribute('maxlength', '3');
textArea.innerHTML = 'abcd';
document.body.appendChild(textArea);
shouldBe('textArea.value', '"abcd"');

// maxLength doesn't truncate .value
textArea.maxLength = 3;
textArea.value = 'abcde';
shouldBe('textArea.value', '"abcde"');

// Set up for user-input tests
function createFocusedTextAreaWithMaxLength3() {
    if (textArea)
        document.body.removeChild(textArea);
    textArea = document.createElement('textarea');
    textArea.setAttribute('maxlength', '3');
    document.body.appendChild(textArea);
    textArea.focus();
}

// Insert text of which length is maxLength.
createFocusedTextAreaWithMaxLength3();
document.execCommand('insertText', false, 'abc');
shouldBe('textArea.value', '"abc"');

// Try to add characters to maxLength characters.
createFocusedTextAreaWithMaxLength3();
textArea.value = 'abc';
document.execCommand('insertText', false, 'def');
shouldBe('textArea.value', '"abc"');

// Replace text
createFocusedTextAreaWithMaxLength3();
textArea.value = 'abc';
document.execCommand('selectAll');
document.execCommand('insertText', false, 'def');
shouldBe('textArea.value', '"def"');

// Existing value is longer than maxLength.  We can't add text.
createFocusedTextAreaWithMaxLength3();
textArea.value = 'abcdef';
document.execCommand('insertText', false, 'ghi');
shouldBe('textArea.value', '"abcdef"');

// We can delete a character in the longer value.
createFocusedTextAreaWithMaxLength3();
textArea.value = 'abcdef';
document.execCommand('delete');
shouldBe('textArea.value', '"abcde"');

// A linebreak is 1 character.
createFocusedTextAreaWithMaxLength3();
document.execCommand('insertText', false, 'A');
document.execCommand('insertLineBreak');
document.execCommand('insertText', false, 'B');
shouldBe('textArea.value', '"A\\nB"');

// According to the HTML5 specification, maxLength is code-point length.
// However WebKit handles it as grapheme length.

// fancyX should be treated as 1 grapheme.
var fancyX = "x\u0305\u0332";// + String.fromCharCode(0x305) + String.fromCharCode(0x332);
// u10000 is one character consisted of a surrogate pair.
var u10000 = "\ud800\udc00";

// Inserts 5 code-points in UTF-16
createFocusedTextAreaWithMaxLength3();
document.execCommand('insertText', false, 'AB' + fancyX);
shouldBe('textArea.value', '"AB" + fancyX');
shouldBe('textArea.value.length', '5');

createFocusedTextAreaWithMaxLength3();
textArea.value = 'AB' + fancyX;
textArea.setSelectionRange(2, 5);  // Select fancyX
document.execCommand('insertText', false, 'CDE');
shouldBe('textArea.value', '"ABC"');

// Inserts 4 code-points in UTF-16
createFocusedTextAreaWithMaxLength3();
document.execCommand('insertText', false, 'AB' + u10000);
shouldBe('textArea.value', '"AB" + u10000');
shouldBe('textArea.value.length', '4');

createFocusedTextAreaWithMaxLength3();
textArea.value = 'AB' + u10000;
textArea.setSelectionRange(2, 4);  // Select u10000
document.execCommand('insertText', false, 'CDE');
shouldBe('textArea.value', '"ABC"');

var successfullyParsed = true;
