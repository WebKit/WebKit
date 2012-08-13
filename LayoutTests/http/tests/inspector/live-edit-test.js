var initialize_LiveEditTest = function() {

InspectorTest.replaceInSource = function(sourceFrame, string, replacement, callback)
{
    sourceFrame._textEditor._mainPanel.setReadOnly(false);
    var oldRange, newRange;
    var lines = sourceFrame._textEditor._textModel._lines;
    for (var i = 0; i < lines.length; ++i) {
        var column = lines[i].indexOf(string);
        if (column === -1)
            continue;
        lines[i] = lines[i].replace(string, replacement);
        var lineEndings = replacement.lineEndings();
        var lastLineLength = lineEndings[lineEndings.length - 1] - (lineEndings[lineEndings.length - 2] || 0);
        oldRange = new WebInspector.TextRange(i, column, i, column + string.length);
        newRange = new WebInspector.TextRange(i, column, i + lineEndings.length - 1, lastLineLength);
        break;
    }
    sourceFrame.onTextChanged(oldRange, newRange);
    sourceFrame._commitEditing();
}

};
