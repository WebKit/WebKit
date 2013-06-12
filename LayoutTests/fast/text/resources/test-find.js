var selection = getSelection();
var CaseSensitive = true;
var CaseInsensitive = false;

// FIXME: Rename these to SearchForward and SearchBackward, respectively.
var forward = false;
var backward = true;

function testFind(subjectString, pattern, backwards, caseSensitivity)
{
    caseSensitivity = caseSensitivity || CaseInsensitive;
    var textNode = document.createTextNode(subjectString);
    document.body.appendChild(textNode);
    selection.removeAllRanges();
    if (backwards) {
        var afterTextNodeRange = document.createRange();
        afterTextNodeRange.setStartAfter(textNode);
        afterTextNodeRange.setEndAfter(textNode);
        selection.addRange(afterTextNodeRange);
    } else {
        var beforeTextNodeRange = document.createRange();
        beforeTextNodeRange.setStartBefore(textNode);
        beforeTextNodeRange.setEndBefore(textNode);
        selection.addRange(beforeTextNodeRange);
    }
    var result;
    if (!find(pattern, caseSensitivity, backwards))
        result = "not found"
    else if (selection.rangeCount != 1)
        result = "internal inconsistency";
    else {
        var resultRange = selection.getRangeAt(0);
        if (resultRange.startContainer !== textNode || resultRange.endContainer !== textNode)
            result = "not found";
        else
            result = resultRange.startOffset + ", " + resultRange.endOffset;
    }
    document.body.removeChild(textNode);
    return result;
}

