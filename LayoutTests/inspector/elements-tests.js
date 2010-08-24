// Frontend functions.

function frontend_expandDOMSubtree(node)
{
    node = node || WebInspector.domAgent.document;
    function processChildren(children)
    {
       for (var i = 0; children && i < children.length; ++i)
           frontend_expandDOMSubtree(children[i]);
    }
    WebInspector.domAgent.getChildNodesAsync(node, processChildren);
}

function frontend_expandDOMSubtreeAndRun(testController, node, continuation)
{
    frontend_expandDOMSubtree(node);
    testController.runAfterPendingDispatches(continuation.bind(this, testController));
}

function frontend_nodeForId(idValue)
{
    var innerMapping = WebInspector.domAgent._idToDOMNode;
    for (var nodeId in innerMapping) {
        var node = innerMapping[nodeId];
        if (node.getAttribute("id") === idValue)
            return node;
    }
    return null;
}

function frontend_selectElementAndRun(testController, idValue, continuation)
{
    var node = frontend_nodeForId(idValue);
    if (!node) {
        testController.notifyDone("No node found.");
        return;
    }

    WebInspector.updateFocusedNode(node.id);
    testController.runAfterPendingDispatches(continuation.bind(this, testController));
}

function frontend_dumpSelectedElementStyles(testController)
{
    testController.notifyDone(frontend_getSelectedElementStyles(false));
}

function frontend_getSelectedElementStyles(excludeComputed, excludeMatched)
{
    var result = [];
    var styleSections = WebInspector.panels.elements.sidebarPanes.styles.sections;
    for (var pseudoId in styleSections) {
        var pseudoName = WebInspector.StylesSidebarPane.PseudoIdNames[pseudoId];
        var sections = styleSections[pseudoId];
        for (var i = 0; i < sections.length; ++i) {
            var section = sections[i];
            if (section.computedStyle && excludeComputed)
                continue;
            if (section.rule && excludeMatched)
                continue;
            if (section.element.previousSibling && section.element.previousSibling.className === "styles-sidebar-separator")
                result.push("======== " + section.element.previousSibling.textContent + " ========");
            result.push((section.expanded ? "[expanded] " : "[collapsed] ") + section.titleElement.textContent + " (" + section.subtitleAsTextForTest + ")");
            section.expand();
            frontend_dumpStyleTreeOutline(section.propertiesTreeOutline, result);
            result.push("");
        }
        result.push("");
    }
    return result;
}

function frontend_getElementStylePropertyTreeItem(propertyName)
{
    var styleSections = WebInspector.panels.elements.sidebarPanes.styles.sections[0];
    var elementStyleSection = styleSections[1];
    var outline = elementStyleSection.propertiesTreeOutline;
    for (var i = 0; i < outline.children.length; ++i) {
        var treeItem = outline.children[i];
        if (treeItem.name === propertyName)
            return treeItem;
    }
    return null;
}

function frontend_dumpStyleTreeOutline(treeItem, result)
{
    var children = treeItem.children;
    for (var i = 0; i < children.length; ++i)
        frontend_dumpStyleTreeItem(children[i], result, "");
}

function frontend_dumpStyleTreeItem(treeItem, result, prefix)
{
    // Filter out width and height properties in order to minimize
    // potential diffs.
    if (!treeItem.listItemElement.textContent.indexOf("width") ||
        !treeItem.listItemElement.textContent.indexOf("height"))
        return;

    if (treeItem.listItemElement.hasStyleClass("inherited"))
        return;
    var typePrefix = "";
    if (treeItem.listItemElement.hasStyleClass("overloaded"))
        typePrefix += "/-- overloaded --/ ";
    if (treeItem.listItemElement.hasStyleClass("disabled"))
        typePrefix += "/-- disabled --/ ";
    var textContent = treeItem.listItemElement.textContent;

    // Add non-selectable url text.
    var textData = treeItem.listItemElement.querySelector("[data-uncopyable]");
    if (textData)
        textContent += textData.getAttribute("data-uncopyable");

    result.push(prefix + typePrefix + textContent);
    treeItem.expand();
    var children = treeItem.children;
    for (var i = 0; children && i < children.length; ++i)
        frontend_dumpStyleTreeItem(children[i], result, prefix + "    ");
}
