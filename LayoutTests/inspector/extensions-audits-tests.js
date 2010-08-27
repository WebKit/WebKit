function extension_runAudits(callback)
{
    function onMessage(event)
    {
        if (event.data === "audit-tests-done")
            callback();
    }
    top.addEventListener("message", onMessage, false);
    webInspector.inspectedWindow.evaluate("evaluateInWebInspector('frontend_runExtensionAudits')");
}

function frontend_runExtensionAudits(testController)
{
    const launcherView = WebInspector.panels.audits._launcherView;
    launcherView._selectAllClicked(false);
    launcherView._auditPresentStateElement.checked = true;

    testController.waitUntilDone();
    var extensionCategories = document.evaluate("label[starts-with(.,'Extension ')]/input[@type='checkbox']",
        WebInspector.panels.audits._launcherView._categoriesElement, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

    for (var i = 0; i < extensionCategories.snapshotLength; ++i)
        extensionCategories.snapshotItem(i).click();

    function onAuditsDone()
    {
        var output = frontend_collectAuditResults();
        InjectedScriptAccess.getDefault().evaluate("log(unescape('" + escape("Audits complete:\n" + output.join("\n")) + "'));", function() {
            top.postMessage("audit-tests-done", "*");
        });
    }

    frontend_addSniffer(WebInspector.panels.audits, "_auditFinishedCallback", onAuditsDone, true);
    launcherView._launchButtonClicked();
}
