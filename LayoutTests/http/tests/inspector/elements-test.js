var initialize_ElementTest = function() {


InspectorTest.nodeWithId = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var result = null;
    var topLevelChildrenRequested = false;
    var pendingRequests = 0;
    function processChildren(topLevel, children)
    {
        pendingRequests--;
        if (result)
            return;

        for (var i = 0; children && i < children.length; ++i) {
            var childNode = children[i];
            if (childNode.getAttribute("id") === idValue) {
                result = childNode;
                callback(result);
                return;
            }
            pendingRequests++;
            WebInspector.domAgent.getChildNodesAsync(childNode, processChildren.bind(null, false));
        }

        if (topLevel)
            topLevelChildrenRequested = true;
        if (topLevelChildrenRequested && !result && !pendingRequests)
            callback(null);
    }
    pendingRequests++;

    WebInspector.domAgent.requestDocument(documentRequested.bind(this));
    function documentRequested(doc)
    {
        WebInspector.domAgent.getChildNodesAsync(doc, processChildren.bind(this, true));
    }
};

InspectorTest.expandedNodeWithId = function(idValue)
{
    var result;
    function callback(node)
    {
        result = node;
    }
    InspectorTest.nodeWithId(idValue, callback);
    return result;
}

InspectorTest.selectNodeWithId = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);
    function mycallback(node)
    {
        if (node)
            WebInspector.updateFocusedNode(node.id);
        InspectorTest.runAfterPendingDispatches(callback.bind(null, node));
    }
    InspectorTest.nodeWithId(idValue, mycallback);
};

InspectorTest.dumpSelectedElementStyles = function(excludeComputed, excludeMatched, omitLonghands)
{
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
                InspectorTest.addResult("======== " + section.element.previousSibling.textContent + " ========");
            InspectorTest.addResult((section.expanded ? "[expanded] " : "[collapsed] ") + section.titleElement.textContent + " (" + section.subtitleAsTextForTest + ")");
            section.expand();
            InspectorTest.dumpStyleTreeOutline(section.propertiesTreeOutline, omitLonghands ? 1 : 2);
            InspectorTest.addResult("");
        }
        InspectorTest.addResult("");
    }
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

InspectorTest.dumpStyleTreeOutline = function(treeItem, depth)
{
    var children = treeItem.children;
    for (var i = 0; i < children.length; ++i)
        InspectorTest.dumpStyleTreeItem(children[i], "", depth || 2);
};

InspectorTest.dumpStyleTreeItem = function(treeItem, prefix, depth)
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
    InspectorTest.addResult(prefix + typePrefix + textContent);
    if (--depth) {
        treeItem.expand();
        var children = treeItem.children;
        for (var i = 0; children && i < children.length; ++i)
            InspectorTest.dumpStyleTreeItem(children[i], prefix + "    ", depth);
    }
};

InspectorTest.dumpElementsTree = function(rootNode)
{
    function beautify(element)
    {
        return element.textContent.replace(/\u200b/g, "").replace(/\n/g, "").trim();
    }

    function print(treeItem, prefix)
    {
        if (treeItem.listItemElement) {
            var expander;
            if (treeItem.hasChildren) {
                if (treeItem.expanded)
                    expander = "- ";
                else
                    expander = "+ ";
            } else
                expander = "  ";

            InspectorTest.addResult(prefix + expander + beautify(treeItem.listItemElement));
        }
        

        if (!treeItem.expanded)
            return;

        var children = treeItem.children;
        for (var i = 0; children && i < children.length - 1; ++i)
            print(children[i], prefix + "    ");

        // Closing tag.
        if (children && children.length)
            print(children[children.length - 1], prefix);
    }

    WebInspector.panels.elements.updateModifiedNodes();
    var treeOutline = WebInspector.panels.elements.treeOutline;
    print(rootNode ? treeOutline.findTreeElement(rootNode) : treeOutline, "");
};

InspectorTest.expandElementsTree = function(callback)
{
    callback = InspectorTest.safeWrap(callback);

    function expand(treeItem)
    {
        var children = treeItem.children;
        for (var i = 0; children && i < children.length; ++i) {
            children[i].expand();
            expand(children[i]);
        }
    }

    function mycallback()
    {
        WebInspector.panels.elements.updateModifiedNodes();
        expand(WebInspector.panels.elements.treeOutline);
        callback();
    }
    InspectorTest.nodeWithId(/nonstring/, mycallback);
};

InspectorTest.dumpDOMAgentTree = function()
{
    if (!WebInspector.domAgent._document)
        return;

    function dump(node, prefix, startIndex)
    {
        InspectorTest.addResult(prefix + node.nodeName + "[" + (node.id - startIndex)+ "]");
        var children = node.children;
        for (var i = 0; children && i < children.length; ++i)
            dump(children[i], prefix + "    ", startIndex);
    }
    dump(WebInspector.domAgent._document, "", WebInspector.domAgent._document.id - 1);
};

};
