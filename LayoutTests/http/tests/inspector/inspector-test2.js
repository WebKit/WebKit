var initializeInspectorTest = (function(completeTestCallId) {

var results = [];

InspectorTest.completeTest = function()
{
    InspectorBackend.didEvaluateForTestInFrontend(completeTestCallId, JSON.stringify(results));
};

InspectorTest.evaluateInConsole = function(code, callback)
{
    WebInspector.console.visible = true;
    WebInspector.console.prompt.text = code;
    WebInspector.console.promptElement.dispatchEvent(createKeyEvent("Enter"));

    addSniffer(WebInspector.ConsoleView.prototype, "addMessage",
        function(commandResult) {
            if (callback)
                callback(commandResult.toMessageElement().textContent);
        });
};

InspectorTest.addResult = function(text)
{
    results.push(text);
};

InspectorTest.reloadPage = function(callback)
{
    InspectorBackend.reloadPage();
    addSniffer(WebInspector, "reset", callback);
};

InspectorTest.ensureDebuggerEnabled = function(callback)
{
    if (WebInspector.panels.scripts._debuggerEnabled)
        callback();
    else {
        addSniffer(WebInspector, "debuggerWasEnabled", callback);
        WebInspector.panels.scripts._toggleDebugging(false);
    }
};

InspectorTest.ensureDebuggerDisabled = function(callback)
{
    if (!WebInspector.panels.scripts._debuggerEnabled)
        callback();
    else {
        addSniffer(WebInspector, "debuggerWasDisabled", callback);
        WebInspector.panels.scripts._toggleDebugging(false);
    }
};

InspectorTest.showScriptSource = function(scriptName, callback)
{
    function waitForAllScripts()
    {
        if (scriptsAreParsed([scriptName]))
            showScriptSource(scriptName, callback);
        else
            addSniffer(WebInspector, "parsedScriptSource", waitForAllScripts);
    }
    waitForAllScripts();
};

function createKeyEvent(keyIdentifier)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true, true, null, keyIdentifier, "");
    return evt;
}

function addSniffer(receiver, methodName, override, opt_sticky)
{
    var original = receiver[methodName];
    if (typeof original !== "function")
        throw ("Cannot find method to override: " + methodName);
    receiver[methodName] = function(var_args) {
        try {
            var result = original.apply(this, arguments);
        } finally {
            if (!opt_sticky)
                receiver[methodName] = original;
        }
        // In case of exception the override won't be called.
        try {
            override.apply(this, arguments);
        } catch (e) {
            throw ("Exception in overriden method '" + methodName + "': " + e);
        }
        return result;
    };
}

function scriptsAreParsed(scripts)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;

    // Check that all the expected scripts are present.
    for (var i = 0; i < scripts.length; i++) {
        var found = false;
        for (var j = 0; j < options.length; j++) {
            if (options[j].text === scripts[i]) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
};

function showScriptSource(scriptName, callback)
{
    var scriptSelect = document.getElementById("scripts-files");
    var options = scriptSelect.options;
    var scriptsPanel = WebInspector.panels.scripts;

    // Select page's script if it's not current option.
    var scriptResource;
    if (options[scriptSelect.selectedIndex].text === scriptName)
        scriptResource = options[scriptSelect.selectedIndex].representedObject;
    else {
        var pageScriptIndex = -1;
        for (var i = 0; i < options.length; i++) {
            if (options[i].text === scriptName) {
                pageScriptIndex = i;
                break;
            }
        }
        scriptResource = options[pageScriptIndex].representedObject;
        scriptsPanel._showScriptOrResource(scriptResource);
    }

    var view = scriptsPanel.visibleView;
    callback = callback.bind(null, view);
    if (!view.sourceFrame._loaded)
        addSniffer(view.sourceFrame, "setContent", callback);
    else
        callback();
};

});

var runTestCallId = 0;
var completeTestCallId = 1;

function runTest()
{
    if (!window.layoutTestController)
        return;

    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();

    var toEvaluate =
        "if (!window.InspectorTest) {" +
        "    var InspectorTest = {};" +
        "    (" + initializeInspectorTest + ")(" + completeTestCallId + ");" +
        "    WebInspector.showPanel('elements');" +
        "    (" + test + ")();" +
        "}";
    layoutTestController.evaluateInWebInspector(runTestCallId, toEvaluate);
}

function didEvaluateForTestInFrontend(callId, result)
{
    if (callId !== completeTestCallId)
        return;
    dumpArray(JSON.parse(result));
    layoutTestController.closeWebInspector();
    setTimeout(function() {
        layoutTestController.notifyDone();
    }, 0);
}

function dumpArray(result)
{
    if (result instanceof Array) {
        for (var i = 0; i < result.length; ++i)
            output(result[i]);
    } else
        output(result);
}

function dumpObject(object, nondeterministicProps, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    output(firstLinePrefix + "{");
    for (var prop in object) {
        var prefixWithName = prefix + "    " + prop + " : ";
        var propValue = object[prop];
        if (nondeterministicProps && prop in nondeterministicProps)
            output(prefixWithName + "<" + typeof propValue + ">");
        else if (typeof propValue === "object")
            dumpObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            output(prefixWithName + "\"" + propValue + "\"");
        else
            output(prefixWithName + propValue);
    }
    output(prefix + "}");
}

function output(text)
{
    var outputElement = document.createElement("div");
    outputElement.id = "output";
    outputElement.style.whiteSpace = "pre";
    outputElement.appendChild(document.createTextNode(text));
    outputElement.appendChild(document.createElement("br"));
    document.body.appendChild(outputElement);
}
