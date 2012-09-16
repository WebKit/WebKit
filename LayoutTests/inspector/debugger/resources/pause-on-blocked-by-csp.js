function test()
{
    WebInspector.showPanel("scripts");

    InspectorTest.addSniffer(WebInspector.CallStackSidebarPane.prototype, "setStatus", setStatus, false);

    function setStatus(status)
    {
        // FIXME: For whatever reason, this isn't getting called consistently.
        // Commenting it out to avoid flakeyness. https://bugs.webkit.org/show_bug.cgi?id=96867
        // InspectorTest.addResult("Callstack status set to: '" + status + "'.");
    }

    InspectorTest.startDebuggerTest(step1);

    function step1()
    {
        DebuggerAgent.setPauseOnExceptions(WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
        InspectorTest.evaluateInPage("setTimeout(testAction, 0);");
        InspectorTest.waitUntilPaused(step2);
    }

    function step2(callFrames)
    {
        InspectorTest.captureStackTrace(callFrames);
        InspectorTest.completeDebuggerTest();
    }
}

window.addEventListener('load', runTest);
