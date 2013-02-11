function initialize_EditorTests()
{

InspectorTest.createTestEditor = function(clientHeight, chunkSize, textEditorDelegate)
{
    WebInspector.debugDefaultTextEditor = true;
    var textEditor = new WebInspector.DefaultTextEditor("", textEditorDelegate || new WebInspector.TextEditorDelegate());
    textEditor.overrideViewportForTest(0, clientHeight || 100, chunkSize || 10);
    textEditor.show(WebInspector.inspectorView.element);
    return textEditor;
};

InspectorTest.fillEditorWithText = function(textEditor, lineCount)
{
    var textModel = textEditor._textModel;
    var lines = [];
    for (var i = 0; i < lineCount; ++i)
        lines.push(i);
    textModel.setText(lines.join("\n"));
}

InspectorTest.textWithSelection = function(text, selection)
{
    if (!selection)
        return text;

    function lineWithCursor(line, column, cursorChar)
    {
        return line.substring(0, column) + cursorChar + line.substring(column);
    }

    var lines = text.split("\n");
    selection = selection.normalize();
    var endCursorChar = selection.isEmpty() ? "|" : "<";
    lines[selection.endLine] = lineWithCursor(lines[selection.endLine], selection.endColumn, endCursorChar);
    if (!selection.isEmpty()) {
        lines[selection.startLine] = lineWithCursor(lines[selection.startLine], selection.startColumn, ">");
    }
    return lines.join("\n");
}

InspectorTest.insertTextLine = function(line)
{
    function enter()
    {
        eventSender.keyDown("\n");
    }

    function innerInsertTextLine()
    {
        textInputController.insertText(line);
    }
    setTimeout(innerInsertTextLine);
    setTimeout(enter);
}

InspectorTest.dumpEditorChunks = function(textEditor)
{
    InspectorTest.addResult("Chunk model");
    var chunks = textEditor._mainPanel._textChunks;
    for (var i = 0; i < chunks.length; ++i)
        InspectorTest.addResult("Chunk [" + i + "] " + chunks[i].startLine + ":" + chunks[i].endLine + " (" + (chunks[i]._expanded ? "expanded" : "collapsed") + ")");
};

InspectorTest.dumpEditorModel = function(textEditor)
{
    InspectorTest.addResult("Text model");
    var textModel = textEditor._textModel;
    for (var i = 0; i < textModel.linesCount; ++i) {
        var prefix = "[" + i + "]";
        while (prefix.length < 10)
            prefix += " ";
        InspectorTest.addResult(prefix + textModel.line(i));
    }
};

InspectorTest.dumpEditorDOM = function(textEditor)
{
    InspectorTest.addResult("Editor DOM");
    var element = textEditor._mainPanel._container;
    for (var node = element.firstChild; node; node = node.nextSibling) {
        if (node._chunk)
            var prefix = "[" + node._chunk.startLine + ":" + node._chunk.endLine + "]";
        else
            var prefix = "[" + node.lineNumber + "]";
        while (prefix.length < 10)
            prefix += " ";
        InspectorTest.addResult(prefix + node.outerHTML);
    }
};

InspectorTest.dumpEditorHTML = function(textEditor, mainPanelOnly)
{
    var element = mainPanelOnly ? textEditor._mainPanel.element : textEditor.element;
    var dumpedHTML = element.innerHTML.replace(/<div/g, "\n<div");
    var dumpedHTML = dumpedHTML.replace(/height: [0-9]+/g, "height: <number>");
    InspectorTest.addResult(dumpedHTML);
};

InspectorTest.getLineElement = function(textEditor, lineNumber)
{
    return textEditor._mainPanel.chunkForLine(lineNumber).expandedLineRow(lineNumber);
};

}
