description('Test to make sure WebKit adds just one element when applying inline style and removes redundant styled elements.');

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, value, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand('styleWithCSS', false, 'false');
    document.execCommand(toggleCommand, false, value);
    var action = toggleCommand + '(' + value + ') on all of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML == expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
}

testSingleToggle("fontSize", 4, 'hello <font size="4">world</font> WebKit', '<font size="4">hello world WebKit</font>');
testSingleToggle("fontName", "Arial", 'hello <b><font face="Arial">world</font></b> WebKit', '<font face="Arial">hello <b>world</b> WebKit</font>');
testSingleToggle("italic", null, 'hello <u><i title="message">world </i><i>WebKit</i></u>', '<i>hello <u><span title="message">world </span>WebKit</u></i>');
testSingleToggle("bold", null, 'hello <i><b>world</b> WebKit</i>', '<b>hello <i>world WebKit</i></b>');
testSingleToggle("bold", null, 'hello <i><b class="test">world</b> WebKit</i>', '<b>hello <i><span class="test">world</span> WebKit</i></b>');
testSingleToggle("bold", null, 'hello <b contenteditable="false">world</b> <b>WebKit </b><u><b>rocks</b></u>', '<b>hello </b><b contenteditable="false">world</b><b> WebKit <u>rocks</u></b>');
testSingleToggle("strikeThrough", null, 'hello <b>world <strike>WebKit</strike></b>', '<strike>hello <b>world WebKit</b></strike>');
testSingleToggle("strikeThrough", null, 'hello <i><strike>world</strike></i><b><strike>WebKit</strike></b> rocks', '<strike>hello <i>world</i><b>WebKit</b> rocks</strike>');
testSingleToggle("strikeThrough", null, 'hello <i><strike>world</strike></i> WebKit <b><strike>rocks</strike></b>', '<strike>hello <i>world</i> WebKit <b>rocks</b></strike>');

// block nodes and br tests
testSingleToggle("bold", null, 'hello<div><i>world</i> <b>WebKit</b></div><div>rocks</div>', '<b>hello</b><div><b><i>world</i> WebKit</b></div><div><b>rocks</b></div>');
testSingleToggle("bold", null, 'hello<br style="display: block;"><i><b>world</b></i><br><b>WebKit</b>', '<b>hello<br style="display: block;"><i>world</i><br>WebKit</b>');
testSingleToggle("bold", null, 'hello<p><b>world</b> <i><b>W</b>ebKit</i></p><b>rocks</b>', '<b>hello</b><p><b>world <i>WebKit</i></b></p><b>rocks</b>');

document.body.removeChild(testContainer);
var successfullyParsed = true;
