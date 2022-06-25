description("This test performs a check that :valid/:invalid CSS psudo selectors are lively applied.");

// Setup for static elements.
var form = document.createElement('form');
document.body.appendChild(form);
var nonForm = document.createElement('div');
document.body.appendChild(nonForm);

function makeInvalid() {
    var i = document.createElement('textarea');
    i.name = 'foo';
    i.required = true;
    i.value = '';
    form.appendChild(i);
    return i;
}

function backgroundOf(el) {
    return document.defaultView.getComputedStyle(el, null).getPropertyValue('background-color');
}

var elBackground = 'backgroundOf(el)';
var invalidColor = 'rgb(255, 0, 0)';
var normalColor = 'rgb(255, 255, 255)';
var disabledColor = 'rgb(0, 0, 0)';
var validColor = 'rgb(0, 0, 255)';

// --------------------------------
//     willValidate change
// --------------------------------
var el = makeInvalid();
// Confirm this element is invalid.
debug('Chheck the initial state:');
shouldBe(elBackground, 'invalidColor');

debug('Change name:');
el.name = '';
shouldBe(elBackground, 'invalidColor');
el.name = 'bar';
shouldBe(elBackground, 'invalidColor');

debug('Change disabled:');
el = makeInvalid();
el.disabled = true;
shouldBe(elBackground, 'disabledColor');
el.disabled = false;
shouldBe(elBackground, 'invalidColor');

debug('Change readOnly:');
el = makeInvalid();
el.readOnly = true;
shouldBe(elBackground, 'normalColor');
el.readOnly = false;
shouldBe(elBackground, 'invalidColor');

debug('Inside/outside of a form:');
el = makeInvalid();
nonForm.appendChild(el);
shouldBe(elBackground, 'invalidColor');
form.appendChild(el);
shouldBe(elBackground, 'invalidColor');

// --------------------------------
//     value change
// --------------------------------
debug('Change the value by DOM attribute:');
el = makeInvalid();
el.value = 'abc';
shouldBe(elBackground, 'validColor');
el.value = '';
shouldBe(elBackground, 'invalidColor');

debug('Change the value by key input:');
el = makeInvalid();
el.focus();
eventSender.keyDown('a');
shouldBe(elBackground, 'validColor');
eventSender.keyDown('\x08', []);
shouldBe(elBackground, 'invalidColor');

// --------------------------------
//     Constraints change
// --------------------------------
debug('Change required:');
el = makeInvalid();
el.required = false;
shouldBe(elBackground, 'validColor');
el.required = true;
shouldBe(elBackground, 'invalidColor');

debug('Change maxlength:');
el = makeInvalid();
el.value = '1234567890';
shouldBe(elBackground, 'validColor');
// Make the value dirty by deleting the last character.
el.focus();
el.setSelectionRange(10, 10);
document.execCommand('delete');
el.maxLength = 5;
shouldBe(elBackground, 'invalidColor');
el.maxLength = 10;
shouldBe(elBackground, 'validColor');
