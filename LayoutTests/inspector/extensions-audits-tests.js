function extension_runAudits(callback)
{
    function onMessage(event)
    {
        if (event.data === "audits-complete")
            callback();
    }
    window.addEventListener("message", onMessage, false);
    top.postMessage("run-audits", "*");
}

// runs in front-end
var initialize_ExtensionsAuditsTest = function()
{
    InspectorTest.startExtensionAudits = function() 
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
            for (var i = 0; i < frames.length; ++i)
                frames[i].postMessage("audits-complete", "*");
        }

        InspectorTest._addSniffer(WebInspector.panels.audits, "_auditFinishedCallback", onAuditsDone, true);
        launcherView._launchButtonClicked();
    }
}

var test = function()
{
    InspectorTest.dispatchOnMessage("run-audits", InspectorTest.startExtensionAudits);
    InspectorTest.runExtensionTests();
}

