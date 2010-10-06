var initialize_ConsoleTest = function() {

InspectorTest.dumpConsoleMessages = function()
{
    var result = [];
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var element = messages[i].toMessageElement();
        InspectorTest.addResult(element.textContent.replace(/\u200b/g, ""));
    }
    return result;
}

InspectorTest.expandConsoleMessages = function()
{
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var element = messages[i].toMessageElement();
        var node = element;
        while (node) {
            if (node.treeElementForTest)
                node.treeElementForTest.expand();
            node = node.traverseNextNode(element);
        }
    }
}

}