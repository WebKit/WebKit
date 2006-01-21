//-------------------------------------------------------------------------------------------------------
// Java script library to run editing layout tests

var commandCount = 1;
var commandDelay = window.location.search.substring(1);
if (commandDelay == '')
    commandDelay = 0;
var selection = window.getSelection();

//-------------------------------------------------------------------------------------------------------

function execTransposeCharactersCommand() {
    document.execCommand("Transpose");
}
function transposeCharactersCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execTransposeCharactersCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execTransposeCharactersCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByCharacterCommand() {
    selection.modify("move", "forward", "character");
}
function moveSelectionForwardByCharacterCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionForwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByCharacterCommand() {
    selection.modify("extend", "forward", "character");
}
function extendSelectionForwardByCharacterCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionForwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByWordCommand() {
    selection.modify("move", "forward", "word");
}
function moveSelectionForwardByWordCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionForwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByWordCommand() {
    selection.modify("extend", "forward", "word");
}
function extendSelectionForwardByWordCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionForwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByLineCommand() {
    selection.modify("move", "forward", "line");
}
function moveSelectionForwardByLineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionForwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByLineCommand() {
    selection.modify("extend", "forward", "line");
}
function extendSelectionForwardByLineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionForwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByCharacterCommand() {
    selection.modify("move", "backward", "character");
}
function moveSelectionBackwardByCharacterCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionBackwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByCharacterCommand() {
    selection.modify("extend", "backward", "character");
}
function extendSelectionBackwardByCharacterCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionBackwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByWordCommand() {
    selection.modify("move", "backward", "word");
}
function moveSelectionBackwardByWordCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionBackwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByWordCommand() {
    selection.modify("extend", "backward", "word");
}
function extendSelectionBackwardByWordCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionBackwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByLineCommand() {
    selection.modify("move", "backward", "line");
}
function moveSelectionBackwardByLineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execMoveSelectionBackwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByLineCommand() {
    selection.modify("extend", "backward", "line");
}
function extendSelectionBackwardByLineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execExtendSelectionBackwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execBoldCommand() {
    document.execCommand("Bold");
}
function boldCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execBoldCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execBoldCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execUnderlineCommand() {
    document.execCommand("Underline");
}
function underlineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execUnderlineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execUnderlineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontNameCommand() {
    document.execCommand("FontName", false, "Courier");
}
function fontNameCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execFontNameCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execFontNameCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontSizeCommand(s) {
    if (arguments.length == 0 || s == undefined || s.length == 0)
        s = '12px';
    document.execCommand("FontSize", false, s);
}
function fontSizeCommand(s) {
    if (commandDelay > 0) {
        window.setTimeout(execFontSizeCommand, commandCount * commandDelay, s);
        commandCount++;
    }
    else {
        execFontSizeCommand(s);
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontSizeDeltaCommand(s) {
    if (arguments.length == 0 || s == undefined || s.length == 0)
        s = '1px';
    document.execCommand("FontSizeDelta", false, s);
}
function fontSizeDeltaCommand(s) {
    if (commandDelay > 0) {
        window.setTimeout(execFontSizeDeltaCommand, commandCount * commandDelay, s);
        commandCount++;
    }
    else {
        execFontSizeDeltaCommand(s);
    }
}

//-------------------------------------------------------------------------------------------------------

function execItalicCommand() {
    document.execCommand("Italic");
}
function italicCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execItalicCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execItalicCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyCenterCommand() {
    document.execCommand("JustifyCenter");
}
function justifyCenterCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execJustifyCenterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyCenterCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyLeftCommand() {
    document.execCommand("JustifyLeft");
}
function justifyLeftCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execJustifyLeftCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyLeftCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyRightCommand() {
    document.execCommand("JustifyRight");
}
function justifyRightCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execJustifyRightCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyRightCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertLineBreakCommand() {
    document.execCommand("InsertLineBreak");
}
function insertLineBreakCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execInsertLineBreakCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertLineBreakCommand();
    }
}

//-------------------------------------------------------------------------------------------------------
 
function execInsertParagraphCommand() {
    document.execCommand("InsertParagraph");
}
function insertParagraphCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execInsertParagraphCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertParagraphCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertNewlineInQuotedContentCommand() {
    document.execCommand("InsertNewlineInQuotedContent");
}
function insertNewlineInQuotedContentCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execInsertNewlineInQuotedContentCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertNewlineInQuotedContentCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execTypeCharacterCommand(c) {
    if (arguments.length == 0 || c == undefined || c.length == 0 || c.length > 1)
        c = 'x';
    document.execCommand("InsertText", false, c);
}
function typeCharacterCommand(c) {
    if (commandDelay > 0) {
        window.setTimeout(execTypeCharacterCommand, commandCount * commandDelay, c);
        commandCount++;
    }
    else {
        execTypeCharacterCommand(c);
    }
}

//-------------------------------------------------------------------------------------------------------

function execSelectAllCommand() {
    document.execCommand("SelectAll");
}
function selectAllCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execSelectAllCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execSelectAllCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execStrikethroughCommand() {
    document.execCommand("Strikethrough");
}
function boldCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execStrikethroughCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execStrikethroughCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execUndoCommand() {
    document.execCommand("Undo");
}
function undoCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execUndoCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execUndoCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execRedoCommand() {
    document.execCommand("Redo");
}
function redoCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execRedoCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execRedoCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execChangeRootSize() {
    document.getElementById("root").style.width = "600px";
}
function changeRootSize() {
    if (commandDelay > 0) {
        window.setTimeout(execChangeRootSize, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execChangeRootSize();
    }
}

//-------------------------------------------------------------------------------------------------------

function execCutCommand() {
    document.execCommand("Cut");
}
function cutCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execCutCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execCutCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execCopyCommand() {
    document.execCommand("Copy");
}
function copyCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execCopyCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execCopyCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execPasteCommand() {
    document.execCommand("Paste");
}
function pasteCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execPasteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execPasteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execPasteAndMatchStyleCommand() {
    document.execCommand("PasteAndMatchStyle");
}
function pasteAndMatchStyleCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execPasteAndMatchStyleCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execPasteAndMatchStyleCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execDeleteCommand() {
    document.execCommand("Delete");
}
function deleteCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execDeleteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execDeleteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execForwardDeleteCommand() {
    document.execCommand("ForwardDelete");
}
function forwardDeleteCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execForwardDeleteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execForwardDeleteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function runEditingTest() {
    var elem = document.getElementById("test");
    var selection = window.getSelection();
    selection.setPosition(elem, 0);
    editingTest();
}

//-------------------------------------------------------------------------------------------------------


function execBackColorCommand() {
    document.execCommand("BackColor", false, "Chartreuse");
}
function backColorCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execBackColorCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execBackColorCommand();
    }
}

//-------------------------------------------------------------------------------------------------------


function runCommand(command, arg1, arg2) {
    document.execCommand(command,arg1,arg2);
}

function executeCommand(command,arg1,arg2) {
    if (commandDelay > 0) {
        window.setTimeout(runCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        runCommand(command,arg1,arg2);
    }
}

