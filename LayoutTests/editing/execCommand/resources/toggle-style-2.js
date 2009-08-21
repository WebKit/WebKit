description("Test to make sure we remove span tags with no attributes if we removed the last attribute.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed("one " + toggleCommand + " command converted " + initialContents + " to " + expectedContents);
    } else {
        testFailed("one " + toggleCommand + " command converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
    }
}

function testDoubleToggle(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    document.execCommand(toggleCommand, false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed("two " + toggleCommand + " commands converted " + initialContents + " to " + expectedContents);
    } else {
        testFailed("two " + toggleCommand + " commands converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
    }
}

testSingleToggle("underline", "test", "<u>test</u>");
testSingleToggle("underline", "<u><b><s>test</s></b></u>", "<b><s>test</s></b>");
testDoubleToggle("underline", "test", "test");
testSingleToggle("strikethrough", "test", "<s>test</s>");
testSingleToggle("strikethrough", "<u><b><s>test</s></b></u>", "<u><b>test</b></u>");
testDoubleToggle("strikethrough", "test", "test");

testSingleToggle("strikethrough", "<u>test</u>", "<u><s>test</s></u>");
testSingleToggle("underline", "<s>test</s>", "<s><u>test</u></s>");

testSingleToggle("strikethrough", '<span style="text-decoration: overline;">test</span>', '<span style="text-decoration: overline;"><s>test</s></span>');
testSingleToggle("underline", '<span style="text-decoration: overline;">test</span>', '<span style="text-decoration: overline;"><u>test</u></span>');

document.body.removeChild(testContainer);

var successfullyParsed = true;