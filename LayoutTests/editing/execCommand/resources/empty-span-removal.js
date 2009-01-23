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

testSingleToggle("bold", "<span><span style='font-weight: bold'>test</span></span>", "<span>test</span>");
testSingleToggle("bold", "<span style='font-weight: bold'><span>test</span></span>", "<span>test</span>");
testSingleToggle("bold", "<span style='font-weight: bold'><span style='font-weight: bold'>test</span></span>", "test");
testSingleToggle("bold", "<span foo=\"bar\" style='font-weight: bold'>test</span>", "<span foo=\"bar\">test</span>");
testDoubleToggle("bold", "<span>test</span>", "<span>test</span>");

document.body.removeChild(testContainer);

var successfullyParsed = true;
