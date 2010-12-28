var initialize_ElementTest = function() {

InspectorTest.expandDOMSubtree = function(node)
{
    node = node || WebInspector.domAgent.document;
    function processChildren(children)
    {
       for (var i = 0; children && i < children.length; ++i)
           InspectorTest.expandDOMSubtree(children[i]);
    }
    WebInspector.domAgent.getChildNodesAsync(node, processChildren);
};

InspectorTest.expandDOMSubtreeAndRun = function(node, callback)
{
    InspectorTest.expandDOMSubtree(node);
    InspectorTest.runAfterPendingDispatches(callback);
};

InspectorTest.nodeForId = function(idValue)
{
    var innerMapping = WebInspector.domAgent._idToDOMNode;
    for (var nodeId in innerMapping) {
        var node = innerMapping[nodeId];
        if (node.getAttribute("id") === idValue)
            return node;
    }
    return null;
};

InspectorTest.selectElementAndRun = function(idValue, callback)
{
    var node = InspectorTest.nodeForId(idValue);
    if (!node) {
        InspectorTest.addResult("Debugger was enabled.");
        InspectorTest.completeTest();
        return;
    }

    WebInspector.updateFocusedNode(node.id);
    InspectorTest.runAfterPendingDispatches(callback);
};

InspectorTest.dumpSelectedElementStyles = function()
{
    InspectorTest.addResults(InspectorTest.getSelectedElementStyles(false));
};

InspectorTest.getSelectedElementStyles = function(excludeComputed, excludeMatched)
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
            InspectorTest.dumpStyleTreeOutline(section.propertiesTreeOutline, result);
            result.push("");
        }
        result.push("");
    }
    return result;
};

// FIXME: this returns the first tree item found (may fail for same-named properties in a style).
InspectorTest.getElementStylePropertyTreeItem = function(propertyName)
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
};

InspectorTest.dumpStyleTreeOutline = function(treeItem, result)
{
    var children = treeItem.children;
    for (var i = 0; i < children.length; ++i)
        InspectorTest.dumpStyleTreeItem(children[i], result, "");
};

InspectorTest.dumpStyleTreeItem = function(treeItem, result, prefix)
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
        InspectorTest.dumpStyleTreeItem(children[i], result, prefix + "    ");
};

};
