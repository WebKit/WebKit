function extension_runAudits(callback)
{
    dispatchOnFrontend({ command: "run-audits" }, callback);
}

// runs in front-end
var initialize_ExtensionsAuditsTest = function()
{
    InspectorTest.startExtensionAudits = function(message, port)
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
            InspectorTest.collectAuditResults();
            port.postMessage("");
        }
        InspectorTest.addSniffer(WebInspector.panels.audits, "_auditFinishedCallback", onAuditsDone, true);

        launcherView._launchButtonClicked();
    }
}

var test = function()
{
    InspectorTest.dispatchOnMessage("run-audits", InspectorTest.startExtensionAudits);
    InspectorTest.runExtensionTests();
}
