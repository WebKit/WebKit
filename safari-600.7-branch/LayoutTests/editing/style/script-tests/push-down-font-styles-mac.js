description('Test to make sure we push down inline styles properly.');

if (window.internals)
    internals.settings.setEditingBehavior('mac');
var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

var styleWithCSS = false;

function testSingleToggle(toggleCommand, value, selector, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    var selected = selector(testContainer);
    document.execCommand('styleWithCSS', false, styleWithCSS ? 'true' : 'false');
    document.execCommand(toggleCommand, false, value);
    var action = toggleCommand + ' ' + value + ' on ' + selected + ' of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML == expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
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


debug("Font size");
styleWithCSS = false;
testSingleToggle("fontsize", 2, selectAll, 'hello world', '<font size="2">hello world</font>');
testSingleToggle("fontsize", 4, selectAll, 'hello world', '<font size="4">hello world</font>');
testSingleToggle("fontsize", 5, selectFirstWord, 'hello world', '<font size="5">hello</font> world');
testSingleToggle("fontsize", 3, selectFirstWord, '<font size="7">hello <div>world</div></font>', 'hello <div style="font-size: -webkit-xxx-large;">world</div>');
testSingleToggle("fontsize", 3, selectFirstWord, '<font size="7"><div>hello</div><div>world</div></font>', '<div>hello</div><div style="font-size: -webkit-xxx-large;">world</div>');
testSingleToggle("fontsize", 3, selectSecondWord, '<font size="7"><div>hello</div>world</font>', '<div style="font-size: -webkit-xxx-large;">hello</div>world');

testSingleToggle("fontsize", 7, selectAll, '<font size="7"><div>hello</div>world</font>', '<font size="7"><div>hello</div>world</font>');
testSingleToggle("fontsize", 7, selectAll, '<font size="3"><div>hello</div>world</font>', '<div><font size="7">hello</font></div><font size="7">world</font>');
testSingleToggle("fontsize", 6, selectAll, '<font size="7"><div>hello</div>world</font>', '<div><font size="6">hello</font></div><font size="6">world</font>');
testSingleToggle("fontsize", 5, selectAll, '<font size="7"><div>hello</div>world</font>', '<div><font size="5">hello</font></div><font size="5">world</font>');
testSingleToggle("fontsize", 3, selectAll, '<font size="7"><div>hello</div>world</font>', '<div>hello</div>world');
testSingleToggle("fontsize", 3, selectAll, '<font size="3"><div>hello</div>world</font>', '<font size="3"><div>hello</div>world</font>');
testSingleToggle("fontsize", 1, selectAll, '<font size="3"><div>hello</div>world</font>', '<div><font size="1">hello</font></div><font size="1">world</font>');

debug("");
debug("Font size (with CSS)");
styleWithCSS = true;
testSingleToggle("fontsize", 7, selectAll, '<font size="7"><div>hello</div>world</font>', '<font size="7"><div>hello</div>world</font>');
testSingleToggle("fontsize", 7, selectAll, '<font size="3"><div>hello</div>world</font>', '<div><span style="font-size: -webkit-xxx-large;">hello</span></div><span style="font-size: -webkit-xxx-large;">world</span>');
testSingleToggle("fontsize", 6, selectAll, '<font size="7"><div>hello</div>world</font>', '<div><span style="font-size: xx-large;">hello</span></div><span style="font-size: xx-large;">world</span>');
testSingleToggle("fontsize", 5, selectAll, '<font size="7"><div>hello</div>world</font>', '<div><span style="font-size: x-large;">hello</span></div><span style="font-size: x-large;">world</span>');
testSingleToggle("fontsize", 3, selectAll, '<font size="7"><div>hello</div>world</font>', '<div>hello</div>world');
testSingleToggle("fontsize", 3, selectAll, '<font size="3"><div>hello</div>world</font>', '<font size="3"><div>hello</div>world</font>');
testSingleToggle("fontsize", 1, selectAll, '<font size="3"><div>hello</div>world</font>', '<div><span style="font-size: x-small;">hello</span></div><span style="font-size: x-small;">world</span>');

debug("");
debug("Font family");
styleWithCSS = false;
testSingleToggle("fontname", "Arial", selectAll, 'hello world', '<font face="Arial">hello world</font>');
testSingleToggle("fontname", "Arial", selectFirstWord, '<font face="sans-serif">hello world</font>', '<font face="Arial">hello</font><font face="sans-serif"> world</font>');
testSingleToggle("fontname", "Arial", selectFirstWord, '<font face="sans-serif">hello<div>world</div></font>', '<font face="Arial">hello</font><div style="font-family: sans-serif;">world</div>');
testSingleToggle("fontname", "Arial", selectSecondWord, '<font face="sans-serif">hello<div>world</div></font>', '<font face="sans-serif">hello</font><div><font face="Arial">world</font></div>');
testSingleToggle("fontname", "Sans-Serif", selectAll, '<font face="sans-serif"><div>hello</div><div>world</div></font>', '<font face="sans-serif"><div>hello</div><div>world</div></font>');
testSingleToggle("fontname", "Arial", selectAll, '<font face="sans-serif"><div>hello</div><div>world</div></font>', '<div><font face="Arial">hello</font></div><div><font face="Arial">world</font></div>');

debug("");
debug("Font family (with CSS)");
styleWithCSS = true;
testSingleToggle("fontname", "Arial", selectAll, 'hello world', '<span style="font-family: Arial;">hello world</span>');
testSingleToggle("fontname", "Arial", selectFirstWord, '<font face="sans-serif">hello world</font>', '<span style="font-family: Arial;">hello</span><font face="sans-serif"> world</font>');
testSingleToggle("fontname", "Arial", selectFirstWord, '<font face="sans-serif">hello<div>world</div></font>', '<span style="font-family: Arial;">hello</span><div style="font-family: sans-serif;">world</div>');
testSingleToggle("fontname", "Arial", selectSecondWord, '<font face="sans-serif">hello<div>world</div></font>', '<span style="font-family: sans-serif;">hello</span><div><span style="font-family: Arial;">world</span></div>');
testSingleToggle("fontname", "Sans-Serif", selectAll, '<font face="sans-serif"><div>hello</div><div>world</div></font>', '<font face="sans-serif"><div>hello</div><div>world</div></font>');
testSingleToggle("fontname", "Arial", selectAll, '<font face="sans-serif"><div>hello</div><div>world</div></font>', '<div><span style="font-family: Arial;">hello</span></div><div><span style="font-family: Arial;">world</span></div>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
