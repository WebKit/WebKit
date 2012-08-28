function test() {
    InspectorTest.addConsoleSniffer(addMessage, true);

    var messageCount = 0;
    function addMessage(message) {
        if (messageCount++)
            return;
        var messages = WebInspector.consoleView.messages;
        for (var i = 0; i < messages.length; ++i) {
            var m = messages[i];
            InspectorTest.addResult("Message[" + i + "]: " + WebInspector.displayNameForURL(m.url) + ":" + m.line + " " + m.message);
            var trace = m.stackTrace;
            if (!trace) {
                InspectorTest.addResult("FAIL: no stack trace attached to message #" + i);
            } else {
                InspectorTest.addResult("Stack Trace:\n");
                for (var j = 0; j < trace.length; j++) {
                    var url = trace[j].url;
                    var lineNumber = trace[j].lineNumber;
                    var columnNumber = trace[j].columnNumber;
                    if (!trace[j].url) {
                        url = "(internal script)";
                        lineNumber = "(line number)";
                        columnNumber = "(column number)";
                    }
                    InspectorTest.addResult("    " + j + ") " + url + " / " + trace[j].functionName + " / " + lineNumber + " / " + columnNumber);
                }
            }
        }
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateInPage("thisTest()");
}
