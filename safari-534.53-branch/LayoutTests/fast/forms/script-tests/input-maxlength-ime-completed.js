description('There was a bug that users could input text longer than maxlength via IME.  This test confirms it was fixed.');

var input = document.createElement('input');
input.maxLength = 2;
document.body.appendChild(input);
input.focus();
textInputController.setMarkedText('abcd', 0, 4);
textInputController.insertText('abcd');  // Debug WebKit crashed by this without the change of bug#25253.
// Check the current value without input.value.
// In Release WebKit, input.value was 'ab' though the user-visible value was 'abcd'.
document.execCommand('SelectAll');
shouldBe('document.getSelection().toString()', '"ab"');

var successfullyParsed = true;
