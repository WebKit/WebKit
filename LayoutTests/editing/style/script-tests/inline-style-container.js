description('Test to make sure WebKit adds style to the appropriate container.');

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

var styleWithCSS = false;
function testSingleToggle(toggleCommand, value, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand('styleWithCSS', false, styleWithCSS);
    document.execCommand(toggleCommand, false, value);
    var action = toggleCommand + '(' + value + ') on all of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML == expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
}

debug('styleWithCSS = false');
testSingleToggle("fontSize", 3, '<font size="3">hello <font size="4">world</font></font>', '<font size="3">hello world</font>');
testSingleToggle("fontSize", 4, '<font face="Arial">hello</font>', '<font face="Arial" size="4">hello</font>');
testSingleToggle("fontSize", 4, '<font color="blue"><font face="Arial">hello</font></font>', '<font color="blue"><font face="Arial" size="4">hello</font></font>');
testSingleToggle("fontSize", 4, '<b><font face="Arial">hello</font></b>', '<b><font face="Arial" size="4">hello</font></b>');
testSingleToggle("fontSize", 4, '<font face="Arial"><i>hello</i></font>', '<font face="Arial" size="4"><i>hello</i></font>');
testSingleToggle("fontName", "Arial", '<b><font face="Arial">hello</font></b> world', '<font class="Apple-style-span" face="Arial"><b>hello</b> world</font>');
testSingleToggle("fontName", "Arial", '<font color="blue">hello</font> world', '<font class="Apple-style-span" face="Arial"><font color="blue">hello</font> world</font>');
testSingleToggle("fontName", "Arial", '<b><u>hello</u> world</b>', '<b><font class="Apple-style-span" face="Arial"><u>hello</u> world</font></b>');
testSingleToggle("foreColor", 'blue', '<font><u style="color:red;">hello</u></font>', '<font color="#0000FF"><u>hello</u></font>');
testSingleToggle("bold", null, '<u><s>hello</s> <s>world</s></u>', '<u><b><s>hello</s> <s>world</s></b></u>');
testSingleToggle("bold", null, '<i>hello</i> <b>world</b>', '<b><i>hello</i> world</b>');
testSingleToggle("bold", null, '<s><i><u>hello <b>world</b></u></i> webkit</s>', '<s><b><i><u>hello world</u></i> webkit</b></s>');
testSingleToggle("bold", null,
    '<b contenteditable="false"><span style="font-weight: normal;">hello</span> world</b> world',
    '<b contenteditable="false"><span style="font-weight: normal;">hello</span> world</b><b> world</b>');
testSingleToggle("bold", null,
    '<i>hello</i> <b contenteditable="false">world</b>',
    '<b><i>hello</i> </b><b contenteditable="false">world</b>');
testSingleToggle("strikeThrough", null, '<i>hello</i> <b><s>world</s></b> WebKit', '<s><i>hello</i> <b>world</b> WebKit</s>');
testSingleToggle("strikeThrough", null, '<b><i>hello <s>world</s></i> WebKit</b>', '<b><s><i>hello world</i> WebKit</s></b>');

debug('')
debug('styleWithCSS = true')
styleWithCSS = true;
testSingleToggle("fontSize", 4, '<font face="Arial">hello</font>', '<font face="Arial" style="font-size: large;">hello</font>');
testSingleToggle("fontSize", 4, '<font face="Arial"><b>hello</b></font>', '<font face="Arial"><b style="font-size: large;">hello</b></font>');
testSingleToggle("fontSize", 4, '<i><b>hello</b></i>', '<i><b style="font-size: large;">hello</b></i>');
testSingleToggle("fontSize", 4, '<i><b>hello</b> world</i>', '<i style="font-size: large;"><b>hello</b> world</i>');
testSingleToggle("fontSize", 4, '<font color="blue"><b>hello</b></font>', '<font color="blue"><b style="font-size: large;">hello</b></font>');
testSingleToggle("bold", null, '<span style="font-style: italic;">hello</span>', '<span style="font-style: italic; font-weight: bold;">hello</span>');
testSingleToggle("underline", null, '<span style="font-style: italic;"><b>hello</b></span>', '<span style="font-style: italic; text-decoration: underline;"><b>hello</b></span>');
testSingleToggle("underline", null,
    '<span style="color: blue;"><i><span style="font-size: large;"><b>hello</b> world</span></i></span>',
    '<span style="color: blue;"><i><span style="font-size: large; text-decoration: underline;"><b>hello</b> world</span></i></span>');

document.body.removeChild(testContainer);
var successfullyParsed = true;
