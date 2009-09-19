description("Test to make sure styles toggle as expected and tag-based styles can be removed by editing commands.")

// Note that editing commands insert <b> instead of
// <span style="font-weight: bold"> when in quirks mode
// so edits to this file should be aware of parse mode.
// FIXME: This test could use iframe subdocuments to avoid
// needing to depend on the compatMode of this document
shouldBeEqualToString("document.compatMode", 'BackCompat');

function testToggleToRemove(toggleCommand, testContainer, testContent)
{
    document.body.appendChild(testContainer);
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    document.body.removeChild(testContainer);
    return (testContainer.firstChild == testContent);
}

function wrapInTag(tagName, content)
{
    var element = document.createElement(tagName);
    element.appendChild(content);
    return element;
}

function wrapInEditableContainer(content)
{
    var testContainer = wrapInTag('div', content);
    testContainer.contentEditable = true;
    return testContainer;
}

function wrapInCSSTag(testContent, cssProperty, cssValue)
{
    var wrapperElement = wrapInTag('span', testContent);
    wrapperElement.style.setProperty(cssProperty, cssValue, "");
    return wrapperElement;
}

function testCSSRemovalOnToggle(cssProperty, cssValue, toggleCommand)
{
    var testContent = document.createTextNode("test");
    var testWrapper = wrapInCSSTag(testContent, cssProperty, cssValue);
    var testContainer = wrapInEditableContainer(testWrapper);
    if (testToggleToRemove(toggleCommand, testContainer, testContent)) {
        testPassed(toggleCommand + " removing " + cssProperty + ": " + cssValue);
    } else {
        testFailed(toggleCommand + " removing " + cssProperty + ": " + cssValue + " -- " + testContainer.innerHTML);
    }
}

function testTagRemovalOnToggle(tagName, toggleCommand)
{
    var testContent = document.createTextNode("test");
    var testWrapper = wrapInTag(tagName, testContent);
    var testContainer = wrapInEditableContainer(testWrapper);
    if (testToggleToRemove(toggleCommand, testContainer, testContent)) {
        testPassed(toggleCommand + " removing " + tagName);
    } else {
        testFailed(toggleCommand + " removing " + tagName + " -- " + testContainer.innerHTML);
    }
}

function testBasicToggle(toggleCommand)
{
    var testContent = document.createTextNode("test");
    var testContainer = wrapInEditableContainer(testContent);
    document.body.appendChild(testContainer);
    window.getSelection().selectAllChildren(testContainer);
    document.execCommand(toggleCommand, false, null);
    document.execCommand(toggleCommand, false, null);
    if (testContainer.firstChild == testContent) {
        testPassed(toggleCommand + " toggle");
    } else {
        testFailed(toggleCommand + " toggle: " + testContainer.innerHTML);
    }
    document.body.removeChild(testContainer);
}

function runTests(toggleCommand, tagName, cssProperty, cssValue)
{
    testBasicToggle(toggleCommand);
    testTagRemovalOnToggle(tagName, toggleCommand);
    testCSSRemovalOnToggle(cssProperty, cssValue, toggleCommand);
}

runTests("bold", "b", "font-weight", "bold");
testTagRemovalOnToggle("strong", "bold"); // IE adds "strong" tags for bold, so we should remove them (even though FF doesn't)
runTests("italic", "i", "font-style", "italic");
testTagRemovalOnToggle("em", "italic"); // IE adds "em" tags for italic, so we should remove them (even though FF doesn't)
runTests("subscript", "sub", "vertical-align", "sub");
runTests("superscript", "sup", "vertical-align", "super");
runTests("strikethrough", "strike", "text-decoration", "line-through");
testTagRemovalOnToggle("s", "strikethrough");
runTests("underline", "u", "text-decoration", "underline");

var successfullyParsed = true;
