description("This test ensures that modifying the selection by line boundary granularity (like home/end do) functions properly in an inline editable context.<br>" +
    "You can run this test manually by placing the caret in the middle of the both of the shaded editable spans below and ensuring home/end (command left/right" +
    " on Mac) moves the caret to the boundaries of the shaded area.");

function testModifyByLine(root, direction, expectedOffset, description) {
    root.focus();

    var sel = window.getSelection();
    var initialOffset = 2;
    sel.setBaseAndExtent(root.firstChild, initialOffset, root.firstChild, initialOffset);
    sel.modify("move", direction, "lineboundary");
    
    var testName = "Modify moving " + direction + " in " + description;
    if (sel.baseOffset == expectedOffset)
        testPassed(testName);
    else if (sel.baseOffset == initialOffset)
        testFailed(testName + " had no effect.");
    else
        testFailed(testName + " resulted in an unexpected offset of " + sel.baseOffset);
}

var container = document.createElement('div');
document.body.appendChild(container);
container.innerHTML = '<span contentEditable="true" style="background-color: lightgrey">adjacent editable</span><span contentEditable="true">spans</span>';

var name = "adjacent, editable spans";
testModifyByLine(container.firstChild, "forward", 17, name);
testModifyByLine(container.firstChild, "backward", 0, name);

if (window.testRunner)
    container.innerHTML = '';

container = document.createElement('div')
document.body.appendChild(container);
container.innerHTML = 'This is a non-editable paragraph with <span contentEditable="true" style="background-color: lightgrey" id="root">an editable span</span> inside it.';

name = "editable span in non-editable content"
var root = document.getElementById('root');
testModifyByLine(root, "forward", 16, name);
testModifyByLine(root, "backward", 0, name);

if (window.testRunner)
    container.innerHTML = '';

var successfullyParsed = true;
