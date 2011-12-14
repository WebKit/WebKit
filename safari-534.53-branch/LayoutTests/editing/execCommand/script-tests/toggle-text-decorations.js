description("Test to make sure we can toggle text decorations correctly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand("styleWithCSS", false, true);
    document.execCommand(toggleCommand, false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed("one " + toggleCommand + " command converted " + initialContents + " to " + expectedContents);
    } else {
        testFailed("one " + toggleCommand + " command converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
    }
}

testSingleToggle("underline", "test", "<span class=\"Apple-style-span\" style=\"text-decoration: underline;\">test</span>");
testSingleToggle("underline", "<span class=\"Apple-style-span\" style=\"text-decoration: underline;\">test</span>", "test");
testSingleToggle("underline", "<span class=\"Apple-style-span\" style=\"text-decoration: underline line-through overline;\">test</span>", "<span class=\"Apple-style-span\" style=\"text-decoration: overline line-through;\">test</span>");
testSingleToggle("strikethrough", "test", "<span class=\"Apple-style-span\" style=\"text-decoration: line-through;\">test</span>");
testSingleToggle("strikethrough", "<span class=\"Apple-style-span\" style=\"text-decoration: line-through;\">test</span>", "test");
testSingleToggle("strikethrough", "<span class=\"Apple-style-span\" style=\"text-decoration: underline line-through overline;\">test</span>", "<span class=\"Apple-style-span\" style=\"text-decoration: underline overline;\">test</span>");

document.body.removeChild(testContainer);

var successfullyParsed = true;
