description("Test to make sure we remove span tags with no attributes if we removed the last attribute.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);
if (window.internals)
    internals.settings.setEditingBehavior('mac');

function testSingleToggle(toggleCommand, initialContents, selector, expectedContents)
{
    testContainer.innerHTML = initialContents;
    var selected = selector(testContainer);
    document.execCommand(toggleCommand, false, 'http://webkit.org/');
    action = 'select ' + selected + ' of "' + initialContents + '" and ' + toggleCommand + ' (http://webkit.org/) yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML === expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
}

function selectAll(container) {
    window.getSelection().selectAllChildren(container);
    return 'all';
}

function selectFirstTwoWords(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'first two words';
}

function selectLastWord(container) {
    window.getSelection().setPosition(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last word';
}

testSingleToggle("createLink", 'hello <b>world</b>', selectAll, '<a href="http://webkit.org/">hello <b>world</b></a>');
testSingleToggle("createLink", '<u>hello world</u>', selectAll, '<u><a href="http://webkit.org/">hello world</a></u>');
testSingleToggle("createLink", 'hello <a href="http://bugs.webkit.org/">world</a>', selectAll, '<a href="http://webkit.org/">hello world</a>');
testSingleToggle("createLink", 'hello <a href="http://bugs.webkit.org/" style="font-weight: bold">world</a>', selectAll, '<a href="http://webkit.org/">hello <b>world</b></a>');
testSingleToggle("createLink", 'hello <b>world</b> WebKit', selectFirstTwoWords, '<a href="http://webkit.org/">hello <b>world</b></a> WebKit');
testSingleToggle("createLink", '<a href="http://trac.webkit.org/">hello <b>world</b></a> WebKit', selectFirstTwoWords, '<a href="http://webkit.org/">hello <b>world</b></a> WebKit');
testSingleToggle("createLink", '<a href="http://trac.webkit.org/" style="font-style: italic;">hello world</a> WebKit', selectFirstTwoWords, '<i><a href="http://webkit.org/">hello world</a></i> WebKit');
testSingleToggle("createLink", 'hello <a href="http://trac.webkit.org/"><b>world</b> WebKit</a>', selectFirstTwoWords, '<a href="http://webkit.org/">hello <b>world</b></a><a href="http://trac.webkit.org/"> WebKit</a>');
testSingleToggle("createLink", 'hello <a href="http://trac.webkit.org/" style="font-style: italic;"><b>world</b> WebKit</a>', selectFirstTwoWords,
    '<a href="http://webkit.org/">hello <b style="font-style: italic;">world</b></a><a href="http://trac.webkit.org/"><i> WebKit</i></a>');
testSingleToggle("createLink", 'hello <b>world</b> WebKit', selectLastWord, 'hello <b>world</b> <a href="http://webkit.org/">WebKit</a>');
testSingleToggle("createLink", '<u>hello <b>world</b> WebKit</u>', selectLastWord, '<u>hello <b>world</b> <a href="http://webkit.org/">WebKit</a></u>');
testSingleToggle("createLink", '<a href="http://trac.webkit.org/"><div>hello</div><div>world</div></a>', selectLastWord,
    '<a href="http://trac.webkit.org/"><div>hello</div></a><div><a href="http://webkit.org/">world</a></div>');
testSingleToggle("createLink", '<a href="http://trac.webkit.org/" style="font-weight: bold;"><div>hello</div><div>world</div></a>', selectLastWord,
    '<a href="http://trac.webkit.org/"><div style="font-weight: bold;">hello</div></a><div style="font-weight: bold;"><a href="http://webkit.org/">world</a></div>');
testSingleToggle("createLink", '<a href="http://trac.webkit.org/" style="font-weight: bold;"><div style="font-weight: normal;">hello</div><div>world</div></a>', selectLastWord,
    '<a href="http://trac.webkit.org/"><div style="font-weight: normal;">hello</div></a><div style="font-weight: bold;"><a href="http://webkit.org/">world</a></div>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
