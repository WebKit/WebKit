var initialize_ConsoleTest = function() {

InspectorTest.showConsolePanel = function()
{
    WebInspector.showPanel("console");
    WebInspector.drawer.immediatelyFinishAnimation();
}

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

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    var result = [];
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var element = messages[i].toMessageElement();
        result.push(element.textContent.replace(/\u200b/g, "") + " " + element.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < messages.length; ++i)
        InspectorTest.addResult(result[i]);
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
