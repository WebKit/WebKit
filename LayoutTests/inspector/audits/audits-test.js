function initialize_AuditTests()
{

InspectorTest.collectAuditResults = function()
{
    WebInspector.panels.audits.showResults(WebInspector.panels.audits.auditResultsTreeElement.children[0].results);
    var liElements = WebInspector.panels.audits.visibleView.element.getElementsByTagName("li");
    for (var j = 0; j < liElements.length; ++j) {
        if (liElements[j].treeElement)
            liElements[j].treeElement.expand();
    }
    InspectorTest.collectTextContent(WebInspector.panels.audits.visibleView.element, 0);
}

InspectorTest.collectTextContent = function(element, level)
{
    var nodeOutput = "";
    var child = element.firstChild;

    while (child) {
        if (child.nodeType === Node.TEXT_NODE) {
            for (var i = 0; i < level; ++i)
                nodeOutput += " ";
            nodeOutput += child.nodeValue.replace("\u200B", "");
        } else if (child.nodeType === Node.ELEMENT_NODE) {
            if (nodeOutput !== "") {
                InspectorTest.addResult(nodeOutput);
                nodeOutput = "";
            }
            InspectorTest.collectTextContent(child, level + 1);
        }
        child = child.nextSibling;
    }
    if (nodeOutput !== "")
        InspectorTest.addResult(nodeOutput);
}

}
