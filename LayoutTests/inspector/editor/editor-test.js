function initialize_EditorTests()
{

InspectorTest.createTestEditor = function(lineCount, clientHeight, chunkSize)
{
    var textEditor = new WebInspector.DefaultTextEditor("", new WebInspector.TextEditorDelegate());
    textEditor.overrideViewportForTest(0, clientHeight || 100, chunkSize || 10);
    textEditor.show(WebInspector.inspectorView.element);
    var textModel = textEditor._textModel;
    var lines = [];
    for (var i = 0; i < lineCount; ++i)
        lines.push("Line " + i);
    textModel.setText(lines.join("\n"));
    return textEditor;
};

InspectorTest.dumpEditorChunks = function(textEditor)
{
    var chunks = textEditor._mainPanel._textChunks;
    for (var i = 0; i < chunks.length; ++i)
        InspectorTest.addResult("Chunk [" + i + "] " + chunks[i].startLine + ":" + chunks[i].endLine + " (" + (chunks[i]._expanded ? "expanded" : "collapsed") + ")");
};

}
