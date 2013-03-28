description("This tests removing multiple elements by RemoveFormat command.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);
if (window.internals)
    internals.settings.setEditingBehavior("mac");

function testRemoveFormat(initialContents, selector, expected)
{
    testContainer.innerHTML = initialContents;
    var selected = selector(testContainer);
    document.execCommand('RemoveFormat', false, null);
    var action = 'RemoveFormat on ' + selected + ' of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML === expected)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expected + '"');
}

function selectAll(container) {
    window.getSelection().selectAllChildren(container);
    return 'all';
}

function selectSecondWord(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('move', 'forward', 'word');
    window.getSelection().modify('extend', 'backward', 'word');
    return 'second word';
}

function selectFirstTwoWords(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'first two words';
}

function selectLastTwoWords(container) {
    window.getSelection().setPosition(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last two words';
}

function selectLastWord(container) {
    window.getSelection().setPosition(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last word';
}

testRemoveFormat('hello', selectAll, 'hello');
testRemoveFormat('<i>hello</i> <u>world</u>', selectAll, 'hello world');
testRemoveFormat('<b><u>hello</u> world</b> <a href="http://webkit.org/"><em>WebKit</em></a>', selectAll, 'hello world <a href="http://webkit.org/">WebKit</a>');
testRemoveFormat('<b><u>hello</u> world</b> <a href="http://webkit.org/"><em>WebKit</em></a>',
    selectSecondWord, '<b><u>hello</u> </b>world <a href="http://webkit.org/"><em>WebKit</em></a>');
testRemoveFormat('<sub><tt>hello world WebKit</tt></sub>', selectSecondWord, '<sub><tt>hello </tt></sub>world<sub><tt> WebKit</tt></sub>');
testRemoveFormat('<q><ins><var>hello wor</var>ld</ins> WebKit</q>', selectSecondWord, '<q><ins><var>hello </var></ins></q>world<q> WebKit</q>');    
testRemoveFormat('<b>hello <dfn>world <kbd>WebKit</kbd></dfn></b>', selectLastWord, '<b>hello <dfn>world </dfn></b>WebKit');
testRemoveFormat('<b>hello <dfn>world <kbd>WebKit</kbd></dfn></b>', selectSecondWord, '<b>hello </b>world<b><dfn> <kbd>WebKit</kbd></dfn></b>');
testRemoveFormat('<code>hello <strong>world WebKit</storng></code>', selectFirstTwoWords, 'hello world<code><strong> WebKit</strong></code>');
testRemoveFormat('<acronym><tt><mark><samp>hello</samp></mark> world <sub>WebKit</sub></tt></acronym>',
    selectFirstTwoWords, '<mark>hello</mark> world<acronym><tt> <sub>WebKit</sub></tt></acronym>');
testRemoveFormat('<b><div>hello world</div></b><div>WebKit</div>', selectLastTwoWords, '<div><b>hello </b>world</div><div>WebKit</div>');
testRemoveFormat('<q><b><div>hello world</div></b>WebKit</q>', selectLastTwoWords, '<div><q><b>hello </b></q>world</div>WebKit');
testRemoveFormat('<q><b><div>hello world</div></b>WebKit</q>', selectSecondWord, '<div><q><b>hello </b></q>world</div><q>WebKit</q>');

testRemoveFormat('<i style="font-weight:bold;">hello</i> <u>world</u>', selectAll, 'hello world');
testRemoveFormat('<font color="red"><b style="font-size: large;"><u>hello</u> world</b> WebKit</font>',
    selectSecondWord, '<font color="red"><b style="font-size: large;"><u>hello</u> </b></font>world<font color="red"> WebKit</font>');
testRemoveFormat('<font size="5"><i><u style="font-size: small;">hello</u> world</i><font size="3"> WebKit</font></font>',
    selectSecondWord, '<font size="5"><i><u style="font-size: small;">hello</u> </i></font>world<font size="5"><font size="3"> WebKit</font></font>');
testRemoveFormat('<sup><div style="text-decoration: underline; font-size: large;">hello <dfn style="font-size: normal;">world</dfn></div> WebKit</sup>',
    selectSecondWord, '<div><sup><font size="4"><u>hello </u></font></sup>world</div><sup> WebKit</sup>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
