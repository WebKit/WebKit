description("Test to make sure that getRangeAt does not modify the range when returning it.")

var div = document.createElement('div');
document.body.appendChild(div);
var textNode = document.createTextNode("asd");
div.appendChild(textNode);

var sel = window.getSelection();
sel.collapse(textNode, 0);
var range = sel.getRangeAt(0);

var result = range.comparePoint(textNode, 0);
if (result == 0) {
    testPassed("range is correctly (text, 0)");
} else {
    testFailed("range did not match (text, 0)");
    debug("window.getSelection():");
    debug("anchorNode: " + sel.anchorNode);
    debug("anchorOffset: " + sel.anchorOffset);
    debug("focusNode: " + sel.focusNode);
    debug("focusOffset: " + sel.focusOffset);

    debug("window.getSelection().getRangeAt(0):");
    debug("startContainer: " + range.startContainer);
    debug("startOffset: " + range.startOffset);
    debug("endContainer: " + range.endContainer);
    debug("endOffset: " + range.endOffset);
}

// Clean up after ourselves
document.body.removeChild(div);

var successfullyParsed = true;
