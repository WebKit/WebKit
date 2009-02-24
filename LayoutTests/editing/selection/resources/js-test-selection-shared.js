function objectAsString(object, properties) {
    var result = "[";
    for (var x = 0; x < properties.length; x++) {
        var property = properties[x];
        if (x != 0)
            result += " ";

        var value = object[property];
        if (value && value.nodeType) // textNode
            value = value + "( " + value.nodeValue + " )";

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

function assertSelectionAt(node, offset) {
    var sel = window.getSelection();
    if (sel.anchorNode == node
        && sel.focusNode == node
        && sel.anchorOffset == offset
        && sel.focusOffset == offset
        && sel.isCollapsed) {
        testPassed("Selection is at node: " + node + " offset: " + offset);
    } else {
        testFailed("Selection is " + selectionAsString(sel) + " should be at node: " + node + " offset: " + offset);
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