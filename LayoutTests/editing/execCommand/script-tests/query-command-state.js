description("Test to make sure queryCommandState returns correct values.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testQueryCommandState(command, contents, selector, expectedState)
{
    testContainer.innerHTML = contents;
    var selected = selector(testContainer);
    var actualState = document.queryCommandState(command);
    var action = 'queryCommandState("' + command + '") returns ' + actualState + ' when selecting ' + selected + ' of "' + contents + '"';
    if (actualState === expectedState)
        testPassed(action);
    else
        testFailed(action + ', expected ' + expectedState + '');
}

function selectAll(container) {
    window.getSelection().selectAllChildren(container);
    return 'all';
}

function selectSecondWord(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('move', 'backward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'second word';
}

function selectFirstTwoWords(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'first two words';
}

function runTests(editingBehavior) {
    if (window.layoutTestController)
        layoutTestController.setEditingBehavior(editingBehavior);
    debug('Tests for ' + editingBehavior)

    testQueryCommandState("bold", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<i>hello</i>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello</b>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", 'hello <b>world</b>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello</b> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", 'hello <b>world</b> WebKit', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello</b> world <b>WebKit</b>', selectSecondWord, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<i>hello <b>hello</b> WebKit</i>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello <i>hello</i> WebKit</b>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<b><div>hello <i>hello</i> WebKit</div></b>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<b style="font-weight: normal;">hello</b>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<i style="font-weight: bold;">hello</i>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello</b> world <b>WebKit</b>', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("bold", '<b>hello</b><b> world</b>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("bold", '<div><b>hello</b></div><p><b> WebKit</b></p>', selectAll, {'mac': true, 'win': true}[editingBehavior]);

    testQueryCommandState("italic", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("italic", '<b>hello</b>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("italic", '<i>hello</i>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("italic", '<i>hello</i> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("italic", 'hello <i>world</i>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("italic", '<i><div>hello world</div></i>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("italic", '<div style="font-style: italic">hello <span style="font-style: normal;">hello</span></div>', selectAll, {'mac': true, 'win': false}[editingBehavior]);

    testQueryCommandState("subscript", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("subscript", '<sup>hello</sup>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("subscript", '<sub>hello</sub>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("subscript", '<sub>hello</sub> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("subscript", 'hello <sub>world</sub>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("subscript", '<div style="vertical-align: sub;">hello world</div>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("subscript", 'hello <span style="vertical-align: sub;">world</span> WebKit', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);

    testQueryCommandState("superscript", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("superscript", '<sub>hello</sub>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("superscript", '<sup>hello</sup>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("superscript", '<sup>hello</sup> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("superscript", 'hello <sup>world</sup>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("superscript", '<span style="vertical-align: super;">hello</span><span style="vertical-align: sub;">world</span>', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("superscript", 'hello<span style="vertical-align: super;">world</span>', selectAll, {'mac': false, 'win': false}[editingBehavior]);

    testQueryCommandState("underline", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("underline", '<s>hello</s>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("underline", '<u>hello</u>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("underline", '<u>hello</u> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("underline", 'hello <u>world</u>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("underline", '<u><div>hello world</div></u>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("underline", '<u><s><div>hello world WebKit</div></s></u>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("underline", '<s><u>hello</u> world</s> WebKit', selectSecondWord, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("underline", '<u><s>hello</s> world</u> WebKit', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("underline", '<s>hello <u>world</s> WebKit</u>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);

    testQueryCommandState("strikeThrough", 'hello', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<u>hello</u>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<s>hello</s>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<s>hello</s> world', selectAll, {'mac': true, 'win': false}[editingBehavior]);
    testQueryCommandState("strikeThrough", 'hello <s>world</s>', selectAll, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<s><div>hello world</div></s>', selectAll, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<s><u><div>hello world WebKit</div></u></s>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("strikeThrough", '<u><s>hello</s> world</u> WebKit', selectSecondWord, {'mac': false, 'win': false}[editingBehavior]);
    testQueryCommandState("strikeThrough", 'hello <s>world WebKit</s>', selectSecondWord, {'mac': true, 'win': true}[editingBehavior]);
    testQueryCommandState("strikeThrough", 'hello <s>world WebKit</s>', selectFirstTwoWords, {'mac': false, 'win': false}[editingBehavior]);
}

runTests('win');
debug('')
runTests('mac');

//document.body.removeChild(testContainer);
var successfullyParsed = true;
