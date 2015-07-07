description('Test to make sure we push down inline styles properly.');

if (window.internals)
    internals.settings.setEditingBehavior('mac');
var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, selector, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    var selected = selector(testContainer);
    document.execCommand('styleWithCSS', false, 'false');
    document.execCommand(toggleCommand, false, null);
    var action = toggleCommand + ' on ' + selected + ' of ' + initialContents + " yields " + testContainer.innerHTML;
    if (testContainer.innerHTML == expectedContents)
        testPassed(action);
    else
        testFailed(action + ", expected " + expectedContents);
}

function selectAll(container) {
    window.getSelection().selectAllChildren(container);
    return 'all';
}

function selectTest(container) {
    window.getSelection().selectAllChildren(document.getElementById('test'));
    return 'test';
}

function selectFirstWord(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('extend', 'forward', 'word');
    return 'first word';
}

function selectSecondWord(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'second word';
}

function selectLastTwoWords(container) {
    window.getSelection().setPosition(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last two words';
}

testSingleToggle("bold", selectFirstWord, '<b><div>hello</div> world</b>', '<div>hello</div><b> world</b>');
testSingleToggle("bold", selectFirstWord, '<b><div>hello</div>world</b>', '<div>hello</div><b>world</b>');
testSingleToggle("bold", selectFirstWord, '<b><div>hello</div><em>world</em></b>', '<div>hello</div><em style="font-weight: bold;">world</em>');
testSingleToggle("bold", selectSecondWord, '<b>hello <div>world</div></b>', '<b>hello </b><div>world</div>');
testSingleToggle("bold", selectSecondWord, '<b><em>hello</em> <div>world</div></b>', '<em style="font-weight: bold;">hello</em> <div>world</div>');
testSingleToggle("bold", selectAll, '<b> <div>text</div> </b>', ' <div>text</div> ');
testSingleToggle("bold", selectAll, '<b><strike><div>text</div></strike></b>', '<strike><div>text</div></strike>');
testSingleToggle("bold", selectFirstWord, '<b><div>hello</div><div>world</div></b>', '<div>hello</div><div style="font-weight: bold;">world</div>');
testSingleToggle("bold", selectFirstWord, '<b><div>hello</div><div style="font-weight: normal;">world</div>webkit</b>', '<div>hello</div><div style="font-weight: normal;">world</div><b>webkit</b>');
testSingleToggle("bold", selectSecondWord, '<b style="font-style: italic;">hello world</b>', '<b style="font-style: italic;">hello</b><span style="font-style: italic;"> world</span>');

testSingleToggle("underline", selectSecondWord, '<u>hello <b>world</b> webkit</u>', '<u>hello</u> <b>world</b><u> webkit</u>');
testSingleToggle("underline", selectLastTwoWords, '<u>hello <b>world</b> webkit</u>', '<u>hello </u><b>world</b> webkit');
testSingleToggle("underline", selectLastTwoWords, '<u>hello <b>world webkit</b></u>', '<u>hello </u><b>world webkit</b>');
testSingleToggle("underline", selectSecondWord, '<u>hello <b>world webkit</b></u>', '<u>hello</u> <b>world<u> webkit</u></b>');
testSingleToggle("underline", selectSecondWord, '<u><b>hello world</b> webkit</u>', '<b><u>hello</u> world</b><u> webkit</u>');
testSingleToggle("underline", selectSecondWord, '<u><strike>hello world</strike></u>', '<strike><u>hello</u> world</strike>');
testSingleToggle("underline", selectSecondWord, '<u><strike>hello world webkit</strike></u>', '<strike><u>hello</u> world<u> webkit</u></strike>');
testSingleToggle("underline", selectSecondWord, '<u><strike>hello world</strike> webkit</u>', '<strike><u>hello</u> world</strike><u> webkit</u>');
testSingleToggle("underline", selectSecondWord, '<u>hello <em><code>world webkit</code></em> rocks</u>', '<u>hello</u> <em><code>world<u> webkit</u></code></em><u> rocks</u>');

testSingleToggle("strikeThrough", selectAll, '<s style="color: blue;">hello world</strike>', '<span style="color: blue;">hello world</span>');
testSingleToggle("strikeThrough", selectFirstWord, '<s style="color: blue;"><div>hello</div> <b>world</b> webkit</strike>', '<span style="color: blue;"><div>hello</div> <b style="text-decoration: line-through;">world</b><strike> webkit</strike></span>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
