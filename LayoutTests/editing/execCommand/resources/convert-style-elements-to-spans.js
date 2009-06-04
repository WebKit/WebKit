description("Test to make sure we do not remove extra styling hidden on html styling elements (b, i, s, etc.) when removing those elements.")

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

function testCommandAndUndo(command, contents)
{
    testContainer.innerHTML = contents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(command, false, null);
    document.execCommand("undo", false, null);
    if (testContainer.innerHTML === contents) {
        testPassed(command + " followed by undo, correctly return to " + contents);
    } else {
        testFailed(command + " followed by undo converted " + contents + " to " + testContainer.innerHTML);
    }
}

function testCommandAndUndoWithIntermediateFunction(command, initialContents, expectedContents, intermediateFunction)
{
    testContainer.innerHTML = initialContents;
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(command, false, null);
    intermediateFunction();
    document.execCommand("undo", false, null);
    if (testContainer.innerHTML === expectedContents) {
        testPassed(command + " followed by undo converted " + initialContents + " to " + expectedContents);
    } else {
        testFailed(command + " followed by undo converted " + initialContents + " to " + testContainer.innerHTML + ", expected " + expectedContents);
    }
}

// It would be nice to expect a <tag>test</tag> result for these, but that's unrealistic.
// The first pass removes the implied styling converting the rest to <span style="...">
// the second pass cannot be expected to convert an "existing" <span style="text-decoration: underline"> into <u>
testSingleToggle("bold", "<b style=\"text-decoration: underline\">test</b>", "<span style=\"text-decoration: underline\">test</span>");
testSingleToggle("italic", "<i style=\"font-weight: bold\">test</i>", "<span style=\"font-weight: bold\">test</span>");

// Make sure we correctly remove double-styled tags
testSingleToggle("bold", "<b style=\"font-weight: bold\">test</b>", "test");

// Make sure we don't remove extra stuff
testSingleToggle("bold", "<b foo=\"bar\">test</b>", "<span foo=\"bar\">test</span>");
testSingleToggle("bold", "<b style='invalid'>test</b>", "test"); // It's OK to remove invalid styles

// FIXME: We have to call undo here to prevent WebKit from trying to
// undo all of our previous commands at once.  We have no way to
// "clear the undo stack" in WebKit.
document.execCommand("undo", false, null);
// That filled testContainer with lots of junk, but it will be cleared
// when testCommandAndUndo is called.

testCommandAndUndo("bold", "<b style=\"text-decoration: underline\">test</b>");
testCommandAndUndo("italic", "<i style=\"font-weight: bold\">test</i>");

testCommandAndUndoWithIntermediateFunction(
    "bold",
    "<b style=\"text-decoration: underline\">test</b>",
    "<b style=\"text-decoration: underline\" foo=\"bar\">test</b>",
    function() {
        // Change the <span> before undoing it
        try {
            testContainer.firstChild.setAttribute("foo", "bar");
        } catch(e) {
            testFailed("Exception caught: " + e);
        }
});

testCommandAndUndoWithIntermediateFunction(
    "bold",
    "<b style=\"text-decoration: underline\">test</b>",
    "",
    function() {
        // remove the span, undo should fail
        testContainer.removeChild(testContainer.firstChild);
});

testCommandAndUndoWithIntermediateFunction(
    "bold",
    "<b style=\"text-decoration: underline\">test</b>",
    "<span>foobar</span>",
    function() {
        // replace the span with a new span, undo should fail
        testContainer.removeChild(testContainer.firstChild);
        testContainer.innerHTML = "<span>foobar</span>";
});

testCommandAndUndoWithIntermediateFunction(
    "bold",
    "<b style=\"text-decoration: underline\">test</b>",
    "<b style=\"text-decoration: underline\">foobar</b>",
    function() {
        // replace the span contents, undo should succeed
        testContainer.firstChild.innerHTML = "foobar";
});

document.body.removeChild(testContainer);

var successfullyParsed = true;
