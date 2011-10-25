description("This test performs a check that :valid/:invalid CSS psudo selectors are lively applied.");

// Setup for static elements.
var form = document.createElement('form');
document.body.appendChild(form);
var nonForm = document.createElement('div');
document.body.appendChild(nonForm);

function simulateClick(element) {
    var rect = element.getBoundingClientRect();
    var x = rect.left + rect.width / 2;
    var y = rect.top + rect.height / 2;

    if (!window.eventSender) {
        return;
    }
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function makeInvalid() {
    var select = document.createElement('select');
    select.name = 'foo';
    select.required = true;
    select.value = '';
    form.appendChild(select);
    return select;
}

function appendOption(value, select) {
    var option = document.createElement('option');
    option.value = value;
    option.innerText = value;
    select.add(option);
    return option;
}

function insertOptionBefore(value, select, before) {
    var option = document.createElement('option');
    option.value = value;
    option.innerText = value;
    select.add(option, before);
    return option;
}

function removeOption(option, select) {
    select.remove(option);
    return option;
}

function backgroundOf(el) {
    return document.defaultView.getComputedStyle(el, null).getPropertyValue('background-color');
}

var elBackground = 'backgroundOf(el)';
var invalidColor = 'rgb(255, 0, 0)';
var normalColor = 'rgb(255, 255, 255)';
var disabledColor = 'rgb(0, 0, 0)';
var readOnlyColor = 'rgb(0, 255, 0)'
var validColor = 'rgb(0, 0, 255)';

// --------------------------------
//     willValidate change
// --------------------------------
var el = makeInvalid();
var o1 = appendOption('', el);
var o2 = appendOption('X', el);
o1.selected = true;
// Confirm this element is invalid.
debug('Check the initial state:');
shouldBe(elBackground, 'invalidColor');

debug('Change name:');
el.name = '';
shouldBe(elBackground, 'invalidColor');
el.name = 'bar';
shouldBe(elBackground, 'invalidColor');

debug('Change disabled:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
el.disabled = true;
shouldBe(elBackground, 'disabledColor');
el.disabled = false;
shouldBe(elBackground, 'invalidColor');

debug('Inside/outside of a form:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
nonForm.appendChild(el);
shouldBe(elBackground, 'invalidColor');
form.appendChild(el);
shouldBe(elBackground, 'invalidColor');

// --------------------------------
//     value change
// --------------------------------

debug('Change the values of select elements without explicit initializing values by clicking:');
form.innerHTML = '<select id="select-multiple" multiple required>' +
'  <option id="multiple-empty">empty</option>' +
'  <option id="multiple-another">another</option>' +
'</select>' +
'<select id="select-size4" size="4" required>' +
'  <option id="size4-empty">empty</option>' +
'  <option id="size4-another">another</option>' +
'</select>';
var selectMultiple = document.getElementById("multiple-empty");
selectMultiple.focus();
simulateClick(selectMultiple);
var selectSize4 = document.getElementById("size4-empty");
selectSize4.focus();
simulateClick(selectSize4);
shouldBe('backgroundOf(selectMultiple)', 'validColor');
shouldBe('backgroundOf(selectSize4)', 'validColor');

debug('Change the value with a placeholder label option:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o2.selected = true;
shouldBe(elBackground, 'validColor');
o1.selected = true;
shouldBe(elBackground, 'invalidColor');

debug('Change the value without a placeholder label option:');
el = makeInvalid();
o1 = appendOption('X', el);
o2 = appendOption('', el);
o2.selected = true;
shouldBe(elBackground, 'validColor');
o1.selected = true;
shouldBe(elBackground, 'validColor');

debug('Insert/remove options:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
shouldBe(elBackground, 'invalidColor');
o3 = insertOptionBefore('Y', el, el.firstChild);
shouldBe(elBackground, 'validColor');
removeOption(o3, el);
shouldBe(elBackground, 'invalidColor');
o3 = appendOption('Z', el);
o3.selected = true;
shouldBe(elBackground, 'validColor');
el.length = 2;
shouldBe(elBackground, 'invalidColor');

debug('Set an attribute: multiple:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
shouldBe(elBackground, 'invalidColor');
el.multiple = true;
shouldBe(elBackground, 'validColor');
el.removeAttribute('multiple');
shouldBe(elBackground, 'invalidColor');

debug('Set an attribute: size:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
shouldBe(elBackground, 'invalidColor');
el.size = 2;
shouldBe(elBackground, 'validColor');
el.removeAttribute('size');
shouldBe(elBackground, 'invalidColor');

debug('SelectAll:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
el.multiple = true;
o1.selected = false;
o2.selected = false;
shouldBe(elBackground, 'invalidColor');
el.focus();
document.execCommand("SelectAll");
shouldBe(elBackground, 'validColor');
el.multiple = false;
o1.selected = false;
o2.selected = true;

debug('Reset:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o2.selected = true;
shouldBe(elBackground, 'validColor');
form.reset();
shouldBe(elBackground, 'invalidColor');

// --------------------------------
//     Constraints change
// --------------------------------
debug('Change required:');
el = makeInvalid();
o1 = appendOption('', el);
o2 = appendOption('X', el);
o1.selected = true;
el.required = false;
shouldBe(elBackground, 'validColor');
el.required = true;
shouldBe(elBackground, 'invalidColor');
