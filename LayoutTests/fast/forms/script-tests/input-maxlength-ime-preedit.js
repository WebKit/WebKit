description('This test checks users can input pre-edit text longer than maxlength.');

var input = document.createElement('input');
document.body.appendChild(input);
input.maxLength = 2;
input.focus();
textInputController.setMarkedText('abcd', 0, 4);
// The selection should have 4 characters though maxLength is 2.
shouldBe('document.getSelection().toString()', '"abcd"');

var successfullyParsed = true;
