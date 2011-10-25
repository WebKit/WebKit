description('A test for maxlength attribute of an input element with non-text type');

var input = document.createElement('input');
input.maxLength = 2;
input.type = 'number';
document.body.appendChild(input);
input.focus();
document.execCommand('insertText', false, '1234');
shouldBe('input.value', '"1234"');

input.parentNode.removeChild(input);
