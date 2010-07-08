
function frontend_ensureDebuggerEnabled(callback)
{
    if (WebInspector.panels.scripts._debuggerEnabled) {
        callback();
        return;
    }

    frontend_addSniffer(WebInspector, "debuggerWasEnabled", callback);
    WebInspector.panels.scripts._toggleDebugging(false);
}

function frontend_ensureDebuggerDisabled(callback)
{
    if (!WebInspector.panels.scripts._debuggerEnabled) {
        callback();
        return;
    }

    frontend_addSniffer(WebInspector, "debuggerWasDisabled", callback);
    WebInspector.panels.scripts._toggleDebugging(false);
}

function frontend_completeDebuggerTest(testController)
{
    if (WebInspector.panels.scripts.paused) {
        WebInspector.panels.scripts._togglePause();
        testController.results.push("Resumed script execution.");
    }
    frontend_ensureDebuggerDisabled(function()
    {
        testController.results.push("Disabled debugger.");
        testController.notifyDone();
    });
}
