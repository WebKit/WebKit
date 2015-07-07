function objectAsString(object, properties) {
    var result = "[";
    for (var x = 0; x < properties.length; x++) {
        var property = properties[x];
        if (x != 0)
            result += " ";

        var value = object[property];
        if (value && value.nodeType) // textNode
            value = value + "(" + value.nodeValue + ")";

        result += property + ": " + value;
    }
    result += ']';
    return result;
}

function selectionAsString(sel)
{
    var properties = ['anchorNode', 'anchorOffset', 'focusNode', 'focusOffset', 'isCollapsed'];
    return objectAsString(sel, properties);
}

function assertSelectionAt(anchorNode, anchorOffset, optFocusNode, optFocusOffset) {
    var focusNode = optFocusNode || anchorNode;
    var focusOffset = (optFocusOffset === undefined) ? anchorOffset : optFocusOffset;

    var sel = window.getSelection();
    if (sel.anchorNode == anchorNode
        && sel.focusNode == focusNode
        && sel.anchorOffset == anchorOffset
        && sel.focusOffset == focusOffset) {
        testPassed("Selection is " + selectionAsString(sel));
    } else {
        testFailed("Selection is " + selectionAsString(sel) + 
            " should be at anchorNode: " + anchorNode + " anchorOffset: " + anchorOffset +
            " focusNode: " + focusNode + " focusOffset: " + focusOffset);
    }
}

function clickAt(x, y) {
    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        return true;
    }
}
