//-------------------------------------------------------------------------------------------------------
// Java script library to run editing layout tests

var commandCount = 1;
var commandDelay = window.location.search.substring(1);
if (commandDelay == '')
    commandDelay = 0;
var selection = window.getSelection();

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

function execInsertNewlineCommand() {
    document.execCommand("InsertNewline");
}
function insertNewlineCommand() {
    if (commandDelay > 0) {
        window.setTimeout(execInsertNewlineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertNewlineCommand();
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

