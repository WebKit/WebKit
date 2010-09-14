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

testSingleToggle("bold", 'hello<b id="test">world</b>', '<b>hello<span id="test">world</span></b>');
testSingleToggle("bold", 'hello<b><i>world</i></b>', '<b>hello<i>world</i></b>');
testSingleToggle("italic", 'hello <i>world</i> <b>webkit</b>', '<i>hello world <b>webkit</b></i>');
testSingleToggle("italic", 'hello <i>world</i> webkit', '<i>hello world webkit</i>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
