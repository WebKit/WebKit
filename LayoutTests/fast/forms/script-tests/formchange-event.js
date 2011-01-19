description('Test for formchange events.');

function sendKey(element, keyName) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName);
    element.dispatchEvent(event);
}

function sendText(element, text) {
    var event = document.createEvent('TextEvent');
    event.initTextEvent('textInput', true, true, document.defaultView, text);
    element.dispatchEvent(event);
}

function sendMouseClick(element) {
    var x = element.clientLeft + 10;
    var y = element.clientTop + 10;
    var event = document.createEvent("MouseEvent");
    event.initMouseEvent("mousedown", true, true, document.defaultView, 1, x, y, x, y, false, false, false, false, 0, document);
    element.dispatchEvent(event);
    var event = document.createEvent("MouseEvent");
    event.initMouseEvent("mouseup", true, true, document.defaultView, 1, x, y, x, y, false, false, false, false, 0, document);
    element.dispatchEvent(event);
}

function $(id) {
    return document.getElementById(id);
}


var form = document.createElement('form');
form.setAttribute('id', 'form');
form.innerHTML =
    '<input type="text" id="default-input" value="Input here." />' +
    '<input type="text" id="input-text" value="Input here." />' +
    '<input type="number" id="input-number" value="24" />' +
    '<input type="radio" name="input-radio" id="input-radio1" value="1" checked/>' +
    '<input type="radio" name="input-radio" id="input-radio2" value="2" />' +
    '<input type="checkbox" name="input-check" id="input-check1" value="1" checked/>' +
    '<input type="checkbox" name="input-check" id="input-check2" value="2" />' +
    '<textarea id="input-textarea">Input here.</textarea>' +
    '<select id="input-select" multiple>' +
    '<option id="input-option1" value="option1">option1</option>' +
    '<option id="input-option2" value="option2">option2</option>' +
    '</select>' +

    '<input type="text" id="default-handler" onformchange="result = \'Fired\'" />' +
    '<input type="text" id="handler-text" onformchange="resultInputText = \'Delivered\'" />' +
    '<input type="number" id="handler-number" onformchange="resultInputNumber = \'Delivered\'" />' +
    '<input type="radio" name="handler-radio" id="handler-radio" onformchange="resultInputRadio = \'Delivered\'" />' +
    '<input type="checkbox" name="handler-check" id="handler-check" onformchange="resultInputCheckbox = \'Delivered\'" />' +
    '<textarea id="handler-textarea" onformchange="resultTextarea = \'Delivered\'"></textarea>' +
    '<keygen id="handler-keygen" onformchange="resultKeygen = \'Delivered\'" />' +
    '<object id="handler-object" onformchange="resultObject = \'Delivered\'" />' +
    '<output id="handler-output" onformchange="resultOutput = \'Delivered\'" />' +
    '<select id="handler-select" onformchange="resultSelect = \'Delivered\'" />' +

    '<p id="handler-p" onformchange="resultP = \'Delivered\'">Hi!</p>';
document.body.appendChild(form);


function onformchangeRemovingInput() {
    $('form-removing-input').removeChild($('handler-removing-input'));
    resultRemovingInput = 'Delivered';
}

var form_removing_input = document.createElement('form');
form_removing_input.setAttribute('id', 'form-removing-input');
form_removing_input.innerHTML =
    '<input type="text" id="input-removing-input" value="Input here." />' +
    '<input type="text" id="handler-removing-input" onformchange="onformchangeRemovingInput()" />';
document.body.appendChild(form_removing_input);


function onformchangeRemovingForm() {
    document.body.removeChild($('form-removing-form'));
    resultRemovingForm = 'Delivered';
}

var form_removing_form = document.createElement('form');
form_removing_form.setAttribute('id', 'form-removing-form');
form_removing_form.innerHTML =
    '<input type="text" id="input-removing-form" value="Input here." />' +
    '<input type="text" id="handler-removing-form" onformchange="onformchangeRemovingForm()" />';
document.body.appendChild(form_removing_form);


var result;

debug('Change for input (type=text)');
result = "Not fired";
$('input-text').focus();
sendText($('input-text'), 'New text.');
$('default-input').focus();
shouldBeEqualToString("result", "Fired");

debug('Change for input (type=number)');
result = "Not fired";
$('input-number').focus();
sendKey($('input-number'), 'Up');
$('default-input').focus();
shouldBeEqualToString("result", "Fired");

debug('Change for input (type=radio)');
result = "Not fired";
$('input-radio2').click();
shouldBeEqualToString("result", "Fired");

debug('Change for input (type=checkbox)');
result = "Not fired";
$('input-check2').click();
shouldBeEqualToString("result", "Fired");

debug('Change for textarea');
result = "Not fired";
$('input-textarea').focus();
sendText($('input-textarea'), 'New text.');
$('default-input').focus();
shouldBeEqualToString("result", "Fired");

debug('Change for select');
result = "Not fired";
sendMouseClick($('input-select'));
shouldBeEqualToString("result", "Fired");

debug('form.dispatchFormChange()');
result = "Not fired";
form.dispatchFormChange();
shouldBeEqualToString("result", "Fired");


debug('');


debug('Is formchange delivered for?');
var resultInputText = "Not delivered";
var resultInputNumber = "Not delivered";
var resultInputRadio = "Not delivered";
var resultInputCheckbox = "Not delivered";
var resultTextarea = "Not delivered";
var resultKeygen = "Not delivered";
var resultObject = "Not delivered";
var resultOutput = "Not delivered";
var resultSelect = "Not delivered";
var resultP = "Not delivered";
$('default-input').focus();
sendText($('default-input'), 'New text.');
$('default-handler').focus();
shouldBeEqualToString("resultInputText", "Delivered");
shouldBeEqualToString("resultInputNumber", "Delivered");
shouldBeEqualToString("resultInputRadio", "Delivered");
shouldBeEqualToString("resultInputCheckbox", "Delivered");
shouldBeEqualToString("resultTextarea", "Delivered");
shouldBeEqualToString("resultKeygen", "Delivered");
shouldBeEqualToString("resultObject", "Not delivered");
shouldBeEqualToString("resultOutput", "Delivered");
shouldBeEqualToString("resultSelect", "Delivered");
shouldBeEqualToString("resultP", "Not delivered");

var resultRemovingInput = "Not delivered";
$('input-removing-input').focus();
sendText($('input-removing-input'), 'New text.');
$('default-handler').focus();
shouldBeEqualToString("resultRemovingInput", "Delivered");

var resultRemovingForm = "Not delivered";
$('input-removing-form').focus();
sendText($('input-removing-form'), 'New text.');
$('default-handler').focus();
shouldBeEqualToString("resultRemovingForm", "Delivered");


document.body.removeChild(form);
debug('');


var successfullyParsed = true;
