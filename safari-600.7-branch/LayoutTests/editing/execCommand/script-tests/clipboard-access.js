description("This test checks whether javascript programs can access clipboard.");

if (window.testRunner)
{
    window.testRunner.setJavaScriptCanAccessClipboard(false);
}

var nonEditableParagraph = document.createElement("p");
nonEditableParagraph.appendChild(document.createTextNode("x"));
document.body.appendChild(nonEditableParagraph);

var editableParagraph = document.createElement("p");
editableParagraph.appendChild(document.createTextNode("x"));
editableParagraph.setAttribute("contentEditable", "true");
document.body.appendChild(editableParagraph);

var editablePlainTextParagraph = document.createElement("p");
editablePlainTextParagraph.appendChild(document.createTextNode("x"));
editablePlainTextParagraph.setAttribute("contentEditable", "plaintext-only");
document.body.appendChild(editablePlainTextParagraph);

function enabled(command, element, selectionStart, selectionEnd)
{
    var selection = document.getSelection();
    selection.removeAllRanges();
    if (element) {
        var range = document.createRange();
        range.setStart(element.firstChild, selectionStart);
        range.setEnd(element.firstChild, selectionEnd);
        selection.addRange(range);
    }
    var result = document.queryCommandEnabled(command)
    selection.removeAllRanges();
    return result;
}

function whenEnabled(command)
{
    var enabledWithNoSelection = enabled(command);
    var enabledWithCaret = enabled(command, editableParagraph, 0, 0);
    var enabledWithEditableRange = enabled(command, editableParagraph, 0, 1);
    var enabledWithPlainTextCaret = enabled(command, editablePlainTextParagraph, 0, 0);
    var enabledWithPlainTextEditableRange = enabled(command, editablePlainTextParagraph, 0, 1);
    var enabledWithPoint = enabled(command, nonEditableParagraph, 0, 0);
    var enabledWithNonEditableRange = enabled(command, nonEditableParagraph, 0, 1);

    var summaryInteger = enabledWithNoSelection
        | (enabledWithCaret << 1)
        | (enabledWithEditableRange << 2)
        | (enabledWithPlainTextCaret << 3)
        | (enabledWithPlainTextEditableRange << 4)
        | (enabledWithPoint << 5)
        | (enabledWithNonEditableRange << 6);

    if (summaryInteger === 0x7F)
        return "always";

    if (summaryInteger === 0x54)
        return "range";

    if (summaryInteger === 0x1E)
        return "editable";
    if (summaryInteger === 0x0A)
        return "caret";
    if (summaryInteger === 0x14)
        return "editable range";

    if (summaryInteger === 0x06)
        return "richly editable";
    if (summaryInteger === 0x02)
        return "richly editable caret";
    if (summaryInteger === 0x04)
        return "richly editable range";

    if (summaryInteger === 0x5E)
        return "visible";

    return summaryInteger;
}

shouldBe("whenEnabled('Copy')", "0");
shouldBe("whenEnabled('Cut')", "0");
shouldBe("whenEnabled('Paste')", "0");
shouldBe("whenEnabled('PasteAndMatchStyle')", "0");

document.body.removeChild(nonEditableParagraph);
document.body.removeChild(editableParagraph);
document.body.removeChild(editablePlainTextParagraph);

var successfullyParsed = true;
