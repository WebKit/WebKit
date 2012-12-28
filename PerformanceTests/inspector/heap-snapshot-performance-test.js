function test()
{
    WebInspector.showPanel("profiles");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildEdgeIndexes");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildRetainers");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildDominatedNodes");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_calculateFlags");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildAggregates");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_calculateClassesRetainedSize");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_calculateDistances");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_calculateRetainedSizes");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_markDetachedDOMTreeNodes");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_markQueriableHeapObjects");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_markPageOwnedNodes");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_splitNodesAndContainmentEdges");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildPostOrderIndex");
    InspectorTest.measureFunction(WebInspector.HeapSnapshot.prototype, "_buildDominatorTree");
    InspectorTest.measureFunction(WebInspector.HeapSnapshotConstructorsDataGrid.prototype, "_aggregatesReceived");

    function performanceTest(timer)
    {
        var transferTimerCookie;
        var showTimerCookie;
        var changeViewTimerCookie;
        var clearTimerCookie;

        var testName = /([^\/]+)\.html$/.exec(WebInspector.inspectedPageURL)[1];
        var fullTimerCookie = timer.start("full-summary-snapshot-time");
        var backendTimerCookie = timer.start("take-snapshot");
        ProfilerAgent.takeHeapSnapshot(step0);

        function step0()
        {
            timer.finish(backendTimerCookie);
            transferTimerCookie = timer.start("transfer-snapshot");
            var profiles = WebInspector.panels.profiles.getProfiles("HEAP");
            WebInspector.panels.profiles.showProfile(profiles[profiles.length - 1]);
            InspectorTest.addSniffer(WebInspector.panels.profiles, "_finishHeapSnapshot", step1);
        }

        function step1(uid)
        {
            timer.finish(transferTimerCookie);
            showTimerCookie = timer.start("show-snapshot");
            var panel = WebInspector.panels.profiles;
            var profile = panel._profilesIdMap[panel._makeKey(uid, WebInspector.HeapSnapshotProfileType.TypeId)];
            profile.load(step2); // Add load callback.
        }

        function step2()
        {
            timer.finish(showTimerCookie);
            changeViewTimerCookie = timer.start("switch-to-containment-view");
            InspectorTest.switchToView("Containment", cleanup);
        }

        function cleanup()
        {
            timer.finish(changeViewTimerCookie);
            timer.finish(fullTimerCookie);
            clearTimerCookie = timer.start("clear-snapshot");
            ProfilerAgent.clearProfiles(done);
            WebInspector.panels.profiles._reset();
        }

        function done()
        {
            timer.finish(clearTimerCookie);
            timer.done(testName);
        }
    }

    InspectorTest.runPerformanceTest(performanceTest, 60000);
}
