description("Tests queryCommandState('justifyCenter'), queryCommandState('justifyLeft'), queryCommandState('justifyRight')")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function queryTextAlignment(selector, content, expected)
{
    testContainer.innerHTML = content;
    var selected = selector(testContainer);
    var center = document.queryCommandState('justifyCenter');
    var left = document.queryCommandState('justifyLeft');
    var right = document.queryCommandState('justifyRight');
    if ((center && left) || (left && right) || (right && center))
        testFailed('Inconsistent state when selecting ' + selected + ' of "' + content + '".  More than one of justifyCenter, justifyRight, and justifyLeft returned true.')

    var actual = center ? 'center' : left ? 'left' : right ? 'right' : '';
    var action = "queryCommand('format') returns \"" + actual + '" when selecting ' + selected + ' of "' + content + '"';
    if (actual == expected)
        testPassed(action);
    else
        testFailed(action + ' but expected "' + expected + '"');
}

function selectFirstPosition(container) {
    while (container.firstChild)
        container = container.firstChild;
    window.getSelection().setPosition(container, 0);
    return 'first position';
}

function selectMiddleOfHelloWorld(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
    return 'middle';
}

debug('Caret');
queryTextAlignment(function () {return 'no selection on'}, 'hello', '');
queryTextAlignment(selectFirstPosition, 'hello', '');
queryTextAlignment(selectFirstPosition, '<p>hello</p>', '');
queryTextAlignment(selectFirstPosition, '<p align="center">hello</p>', 'center');
queryTextAlignment(selectFirstPosition, '<p align="left">hello</p>', 'left');
queryTextAlignment(selectFirstPosition, '<p align="right">hello</p>', 'right');
queryTextAlignment(selectFirstPosition, '<p style="text-align: center;">hello</p>', 'center');
queryTextAlignment(selectFirstPosition, '<p style="text-align: left;">hello</p>', 'left');
queryTextAlignment(selectFirstPosition, '<p style="text-align: right;">hello</p>', 'right');
queryTextAlignment(selectFirstPosition, '<p align="right" style="text-align: left;">hello</p>', 'left');
queryTextAlignment(selectFirstPosition, '<p align="center" style="text-align: right;">hello</p>', 'right');
queryTextAlignment(selectFirstPosition, '<p align="left" style="text-align: center;">hello</p>', 'center');
queryTextAlignment(selectFirstPosition, '<p align="right" style="text-align: left;">hello</p>', 'left');
queryTextAlignment(selectFirstPosition, '<p align="center" style="text-align: right;">hello</p>', 'right');
queryTextAlignment(selectFirstPosition, '<p align="left" style="text-align: center;">hello</p>', 'center');
queryTextAlignment(selectFirstPosition, '<h1>hello</h1>', '');
queryTextAlignment(selectFirstPosition, '<h1 align="center">hello</h1>', 'center');
queryTextAlignment(selectFirstPosition, '<h2 align="left">hello</h2>', 'left');
queryTextAlignment(selectFirstPosition, '<h3 align="right">hello</h3>', 'right');
queryTextAlignment(selectFirstPosition, '<h4 align="center">hello</h4>', 'center');
queryTextAlignment(selectFirstPosition, '<h5 align="left">hello</h5>', 'left');
queryTextAlignment(selectFirstPosition, '<h6 align="right">hello</h6>', 'right');
queryTextAlignment(selectFirstPosition, '<div align="center">hello</div>', 'center');
queryTextAlignment(selectFirstPosition, '<div align="left">hello</div>', 'left');
queryTextAlignment(selectFirstPosition, '<div align="right">hello</div>', 'right');

function runRangeTests(editingBehavior)
{
    if (window.layoutTestController)
        layoutTestController.setEditingBehavior(editingBehavior);
    debug('Tests for ' + editingBehavior);

    queryTextAlignment(selectMiddleOfHelloWorld, '<p>hello</p><p>world</p>', '');
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="left">hello</p><p>world</p>', {'mac': 'left', 'win': ''}[editingBehavior]);
    queryTextAlignment(selectMiddleOfHelloWorld, '<p>hello</p><p align="left">world</p>', '');
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="left">hello</p><p align="right">world</p>', {'mac': 'left', 'win': ''}[editingBehavior]);
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="center">hello</p><p align="center">world</p>', 'center');
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="left">hello</p><p align="left">world</p>', 'left');
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="right">hello</p><p align="right">world</p>', 'right');
    queryTextAlignment(selectMiddleOfHelloWorld, '<div align="right">hello<p align="left">world</p></div>', {'mac': 'right', 'win': ''}[editingBehavior]);
    queryTextAlignment(selectMiddleOfHelloWorld, '<div align="left"><p align="center">world</p>hello</div>', {'mac': 'center', 'win': ''}[editingBehavior]);
    queryTextAlignment(selectMiddleOfHelloWorld, '<p align="left">hello</p><p>w</p><p align="left">orld</p>', {'mac': 'left', 'win': ''});
}

debug('');
runRangeTests('win');
debug('');
runRangeTests('mac');

document.body.removeChild(testContainer);
var successfullyParsed = true;
