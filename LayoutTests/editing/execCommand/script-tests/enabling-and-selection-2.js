description("This tests enabling of non-execCommand commands based on whether the selection is a caret or range or in editable content.");

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
    var result = testRunner.isCommandEnabled(command);
    selection.removeAllRanges();
    return result;
}

function whenEnabled(command)
{
    if (!window.testRunner)
        return "no layout test controller";

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

shouldBe("whenEnabled('Print')", "'always'");
shouldBe("whenEnabled('SelectAll')", "'always'");

shouldBe("whenEnabled('Transpose')", "'caret'");

shouldBe("whenEnabled('Copy')", "'range'");

shouldBe("whenEnabled('Cut')", "'editable range'");
shouldBe("whenEnabled('Delete')", "'editable range'");

shouldBe("whenEnabled('DeleteBackward')", "'editable'");
shouldBe("whenEnabled('DeleteBackwardByDecomposingPreviousCharacter')", "'editable'");
shouldBe("whenEnabled('DeleteForward')", "'editable'");
shouldBe("whenEnabled('DeleteToBeginningOfLine')", "'editable'");
shouldBe("whenEnabled('DeleteToBeginningOfParagraph')", "'editable'");
shouldBe("whenEnabled('DeleteToEndOfLine')", "'editable'");
shouldBe("whenEnabled('DeleteToEndOfParagraph')", "'editable'");
shouldBe("whenEnabled('DeleteToMark')", "'editable'");
shouldBe("whenEnabled('DeleteWordBackward')", "'editable'");
shouldBe("whenEnabled('DeleteWordForward')", "'editable'");
shouldBe("whenEnabled('IgnoreSpelling')", "'editable'");
shouldBe("whenEnabled('InsertBacktab')", "'editable'");
shouldBe("whenEnabled('InsertLineBreak')", "'editable'");
shouldBe("whenEnabled('InsertNewline')", "'editable'");
shouldBe("whenEnabled('InsertTab')", "'editable'");
shouldBe("whenEnabled('MoveBackward')", "'editable'");
shouldBe("whenEnabled('MoveDown')", "'editable'");
shouldBe("whenEnabled('MoveForward')", "'editable'");
shouldBe("whenEnabled('MoveLeft')", "'editable'");
shouldBe("whenEnabled('MoveRight')", "'editable'");
shouldBe("whenEnabled('MoveToBeginningOfDocument')", "'editable'");
shouldBe("whenEnabled('MoveToBeginningOfLine')", "'editable'");
shouldBe("whenEnabled('MoveToBeginningOfParagraph')", "'editable'");
shouldBe("whenEnabled('MoveToBeginningOfSentence')", "'editable'");
shouldBe("whenEnabled('MoveToEndOfDocument')", "'editable'");
shouldBe("whenEnabled('MoveToEndOfLine')", "'editable'");
shouldBe("whenEnabled('MoveToEndOfParagraph')", "'editable'");
shouldBe("whenEnabled('MoveToEndOfSentence')", "'editable'");
shouldBe("whenEnabled('MoveUp')", "'editable'");
shouldBe("whenEnabled('MoveWordBackward')", "'editable'");
shouldBe("whenEnabled('MoveWordForward')", "'editable'");
shouldBe("whenEnabled('MoveWordLeft')", "'editable'");
shouldBe("whenEnabled('MoveWordRight')", "'editable'");
shouldBe("whenEnabled('Yank')", "'editable'");
shouldBe("whenEnabled('YankAndSelect')", "'editable'");

shouldBe("whenEnabled('AlignCenter')", "'richly editable'");
shouldBe("whenEnabled('AlignJustified')", "'richly editable'");
shouldBe("whenEnabled('AlignLeft')", "'richly editable'");
shouldBe("whenEnabled('AlignRight')", "'richly editable'");
shouldBe("whenEnabled('Indent')", "'richly editable'");
shouldBe("whenEnabled('MakeTextWritingDirectionLeftToRight')", "'richly editable'");
shouldBe("whenEnabled('MakeTextWritingDirectionNatural')", "'richly editable'");
shouldBe("whenEnabled('MakeTextWritingDirectionRightToLeft')", "'richly editable'");
shouldBe("whenEnabled('Outdent')", "'richly editable'");
shouldBe("whenEnabled('Subscript')", "'richly editable'");
shouldBe("whenEnabled('Superscript')", "'richly editable'");
shouldBe("whenEnabled('Underline')", "'richly editable'");
shouldBe("whenEnabled('Unscript')", "'richly editable'");

shouldBe("whenEnabled('Paste')", "'editable'");

shouldBe("whenEnabled('MoveBackwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveDownAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveForwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveLeftAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveParagraphBackwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveParagraphForwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveRightAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToBeginningOfDocumentAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToBeginningOfLineAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToBeginningOfParagraphAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToBeginningOfSentenceAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToEndOfDocumentAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToEndOfLineAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToEndOfParagraphAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveToEndOfSentenceAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveUpAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveWordBackwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveWordForwardAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveWordLeftAndModifySelection')", "'visible'");
shouldBe("whenEnabled('MoveWordRightAndModifySelection')", "'visible'");
shouldBe("whenEnabled('SelectLine')", "'visible'");
shouldBe("whenEnabled('SelectParagraph')", "'visible'");
shouldBe("whenEnabled('SelectSentence')", "'visible'");
shouldBe("whenEnabled('SelectWord')", "'visible'");
shouldBe("whenEnabled('SetMark')", "'visible'");

shouldBe("whenEnabled('OverWrite')", "'richly editable'");

document.body.removeChild(nonEditableParagraph);
document.body.removeChild(editableParagraph);
document.body.removeChild(editablePlainTextParagraph);

var successfullyParsed = true;
