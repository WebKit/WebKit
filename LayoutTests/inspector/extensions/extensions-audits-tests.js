function extension_runAudits(callback)
{
    evaluateOnFrontend("InspectorTest.startExtensionAudits(reply);", callback);
}

// runs in front-end
var initialize_ExtensionsAuditsTest = function()
{
    InspectorTest.startExtensionAudits = function(callback)
    {
        const launcherView = WebInspector.panels.audits._launcherView;
        launcherView._selectAllClicked(false);
        launcherView._auditPresentStateElement.checked = true;

        var extensionCategories = document.evaluate("label[starts-with(.,'Extension ')]/input[@type='checkbox']",
            WebInspector.panels.audits._launcherView._categoriesElement, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

        for (var i = 0; i < extensionCategories.snapshotLength; ++i)
            extensionCategories.snapshotItem(i).click();

        function onAuditsDone()
        {
            InspectorTest.runAfterPendingDispatches(function() {
                InspectorTest.collectAuditResults();
                callback();
            });
        }
        InspectorTest.addSniffer(WebInspector.panels.audits, "auditFinishedCallback", onAuditsDone, true);

        launcherView._launchButtonClicked();
    }

    InspectorTest.dumpAuditProgress = function()
    {
        var progress = document.querySelector(".panel.audits progress");
        InspectorTest.addResult("Progress: " + Math.round(100 * progress.value / progress.max) + "%");
    }
}
