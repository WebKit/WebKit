description("Ensuring we continue to use strike tags for legacy strikethrough execCommands, not s tags. See https://bugs.webkit.org/show_bug.cgi?id=53475")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function test(initialContents, expectedContents)
{
    testContainer.innerHTML = initialContents;
    var selected = window.getSelection().selectAllChildren(testContainer);
    document.execCommand("strikethrough", false, '');
    action = 'Strikethrough of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML === expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
}
test("Don't change expected results for me!", "<strike>Don't change expected results for me!</strike>");

document.body.removeChild(testContainer);

var successfullyParsed = true;
