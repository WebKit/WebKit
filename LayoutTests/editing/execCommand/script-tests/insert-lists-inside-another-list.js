description("Test to make sure inserting an ordered/unordered list inside another list works")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testList(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    e = document.getElementById('test');
    if (!e)
      e = testContainer;
    window.getSelection().selectAllChildren(e);
    document.execCommand(toggleCommand, false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed(initialContents + "\n got:" + expectedContents+"\n");
    } else {
        testFailed(initialContents + "\n got:" + testContainer.innerHTML + "\nexp.:" + expectedContents+"\n");
    }
}

debug('Select all nodes\n');

testList("InsertOrderedList", '<ul><ul><li>hello</li><li>world</li><li>WebKit</li></ul></ul>', '<ul><ol><li>hello</li><li>world</li><li>WebKit</li></ol></ul>');
testList("InsertOrderedList", '<ul><ul><li>hello</li></ul><ul><li>world</li></ul></ul>', '<ul><ol><li>hello</li><li>world</li></ol></ul>');
testList("InsertOrderedList", '<ul><ul><li>hello</li><li>world</li></ul></ul><ul><li>WebKit</li></ul>', '<ul><ol><li>hello</li><li>world</li></ol></ul><ol><li>WebKit</li></ol>');
testList("InsertUnorderedList", '<ol><ol><li>hello</li><li>world</li><li>WebKit</li></ol></ol>', '<ol><ul><li>hello</li><li>world</li><li>WebKit</li></ul></ol>');
testList("InsertUnorderedList", '<ol><ol><li>hello</li></ol><ol><li>world</li></ol></ol>', '<ol><ul><li>hello</li><li>world</li></ul></ol>');
testList("InsertUnorderedList", '<ol><ol><li>hello</li><li>world</li></ol></ol><ol><li>WebKit</li></ol>', '<ol><ul><li>hello</li><li>world</li></ul></ol><ul><li>WebKit</li></ul>');

debug('Select #test\n');
testList("InsertOrderedList", '<ul><ul id="test"><li>hello</li><li>world</li></ul><ol><li>WebKit</li></ol></ul>', '<ul><ol><li>hello</li><li>world</li><li>WebKit</li></ol></ul>');
testList("InsertOrderedList", '<ul><ol><li>hello</li></ol><ul id="test"><li>world</li></ul><ol><li>WebKit</li></ol></ul>', '<ul><ol><li>hello</li><li>world</li><li>WebKit</li></ol></ul>');
testList("InsertOrderedList", '<ul><ul id="test"><li>hello</li></ul>world<ol><li>WebKit</li></ol></ul>', '<ul><ol><li>hello</li></ol>world<ol><li>WebKit</li></ol></ul>');
testList("InsertUnorderedList", '<ol><ol id="test"><li>hello</li><li>world</li></ol><ul><li>WebKit</li></ul></ol>', '<ol><ul><li>hello</li><li>world</li><li>WebKit</li></ul></ol>');
testList("InsertUnorderedList", '<ol><ul><li>hello</li></ul><ol id="test"><li>world</li></ol><ul><li>WebKit</li></ul></ol>', '<ol><ul><li>hello</li><li>world</li><li>WebKit</li></ul></ol>');
testList("InsertUnorderedList", '<ol><ol id="test"><li>hello</li></ol>world<ul><li>WebKit</li></ul></ol>', '<ol><ul><li>hello</li></ul>world<ul><li>WebKit</li></ul></ol>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
