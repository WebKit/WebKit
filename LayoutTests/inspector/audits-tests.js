function frontend_collectAuditResults()
{
    WebInspector.panels.audits.showResults(WebInspector.panels.audits.auditResultsTreeElement.children[0].results);
    var liElements = WebInspector.panels.audits.visibleView.element.getElementsByTagName("li");
    for (var j = 0; j < liElements.length; ++j) {
        if (liElements[j].treeElement)
            liElements[j].treeElement.expand();
    }
    var output = [];
    frontend_collectTextContent(WebInspector.panels.audits.visibleView.element, 0, output);
    return output;
}

function frontend_collectTextContent(element, level, output)
{
    var nodeOutput = "";
    var child = element.firstChild;

    while (child) {
        if (child.nodeType === Node.TEXT_NODE) {
            for (var i = 0; i < level; ++i)
                nodeOutput += " ";
            nodeOutput += child.nodeValue;
        } else if (child.nodeType === Node.ELEMENT_NODE) {
            if (nodeOutput !== "") {
                output.push(nodeOutput);
                nodeOutput = "";
            }
            frontend_collectTextContent(child, level + 1, output);
        }
        child = child.nextSibling;
    }
    if (nodeOutput !== "")
        output.push(nodeOutput);
}
