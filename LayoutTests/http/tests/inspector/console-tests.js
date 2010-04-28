// Inspected Page functions.

function dumpConsoleMessages(noNotifyDone) {
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i].text);
        if (!noNotifyDone)
            notifyDone();
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}

function dumpConsoleMessagesWithClasses(sortMessages) {
    function callback(result)
    {
        var messages = [];
        for (var i = 0; i < result.length; ++i)
            messages.push(result[i].text + " " + result[i].clazz);
        if (sortMessages)
            messages.sort();
        for (var i = 0; i < messages.length; ++i)
            output(messages[i]);
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
// Inspected Page functions.

function dumpConsoleMessages(noNotifyDone) {
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i].text);
        if (!noNotifyDone)
            notifyDone();
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}

function dumpConsoleMessagesWithClasses(sortMessages) {
    function callback(result)
    {
        var messages = [];
        for (var i = 0; i < result.length; ++i)
            messages.push(result[i].text + " " + result[i].clazz);
        if (sortMessages)
            messages.sort();
        for (var i = 0; i < messages.length; ++i)
            output(messages[i]);
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
// Inspected Page functions.

function dumpConsoleMessages(noNotifyDone) {
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i].text);
        if (!noNotifyDone)
            notifyDone();
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}

function dumpConsoleMessagesWithClasses(sortMessages) {
    function callback(result)
    {
        var messages = [];
        for (var i = 0; i < result.length; ++i)
            messages.push(result[i].text + " " + result[i].clazz);
        if (sortMessages)
            messages.sort();
        for (var i = 0; i < messages.length; ++i)
            output(messages[i]);
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
