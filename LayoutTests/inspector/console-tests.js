// Inspected Page functions.

function dumpConsoleMessages() {
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i].text);
        notifyDone();
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}

function dumpConsoleMessagesWithClasses() {
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i].text + " " + result[i].clazz);
        notifyDone();
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}


// Frontend functions.

function frontend_dumpConsoleMessages()
{
    var result = [];
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var element = messages[i].toMessageElement();
        result.push({ text: element.textContent.replace(/\u200b/g, ""), clazz: element.getAttribute("class")});
    }
    return result;
}
