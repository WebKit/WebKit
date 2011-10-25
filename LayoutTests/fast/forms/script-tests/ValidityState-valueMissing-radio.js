description('valueMissing tests for radio buttons');

var parent = document.createElement('div');
document.body.appendChild(parent);

debug('Without form element');
parent.innerHTML = '<input name=victim type=radio required>'
    + '<input name=victim type=radio>'
    + '<input name=victim type=radio>';
var inputs = document.getElementsByName('victim');
debug('No checked button:');
shouldBeTrue('inputs[0].validity.valueMissing');
// The following result should be false because the element does not have
// "required".  It conforms to HTML5, and this behavior has no practical
// problems.
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The second button has been checked:');
inputs[1].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The first button has been checked:');
inputs[0].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The third button has been checked:');
inputs[2].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');

debug('With form element');
parent.innerHTML = '<form>'
    + '<input name=victim type=radio required>'
    + '<input name=victim type=radio>'
    + '<input name=victim type=radio>'
    + '</form>';
inputs = document.getElementsByName('victim');
debug('No checked button:');
shouldBeTrue('inputs[0].validity.valueMissing');
// The following result should be false.
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The first button has been checked:');
inputs[0].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The second button has been checked:');
inputs[1].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
debug('The third button has been checked:');
inputs[2].checked = true;
shouldBeFalse('inputs[0].validity.valueMissing');
shouldBeFalse('inputs[1].validity.valueMissing');
shouldBeFalse('inputs[2].validity.valueMissing');
