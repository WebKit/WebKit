var initialize_ElementTest = function() {

InspectorTest.findNode = function(matchFunction, callback)
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
            if (matchFunction(childNode)) {
                result = childNode;
                callback(result);
                return;
            }
            pendingRequests++;
            childNode.getChildNodes(processChildren.bind(null, false));
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
        doc.getChildNodes(processChildren.bind(this, true));
    }
};

InspectorTest.nodeWithId = function(idValue, callback)
{
    function nodeIdMatches(node)
    {
        return node.getAttribute("id") === idValue;
    }
    InspectorTest.findNode(nodeIdMatches, callback);
}

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
    function onNodeFound(node)
    {
        if (node)
            WebInspector.updateFocusedNode(node.id);
        callback(node);
    }
    InspectorTest.nodeWithId(idValue, onNodeFound);
}

InspectorTest.waitForStyles = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);

    (function sniff(node)
    {
        if (node && node.getAttribute("id") === idValue) {
            callback();
            return;
        }
        InspectorTest.addSniffer(WebInspector.StylesSidebarPane.prototype, "_nodeStylesUpdatedForTest", sniff);
    })(null);
}

InspectorTest.selectNodeAndWaitForStyles = function(idValue, callback)
{
    WebInspector.showPanel("elements");

    callback = InspectorTest.safeWrap(callback);

    var targetNode;

    InspectorTest.waitForStyles(idValue, stylesUpdated);
    InspectorTest.selectNodeWithId(idValue, nodeSelected);

    function nodeSelected(node)
    {
        targetNode = node;
    }

    function stylesUpdated()
    {
        callback(targetNode);
    }
}

InspectorTest.dumpSelectedElementStyles = function(excludeComputed, excludeMatched, omitLonghands)
{
    function extractText(element)
    {
        var text = element.textContent;
        if (text)
            return text;
        element = element.querySelector("[data-uncopyable]");
        return element ? element.getAttribute("data-uncopyable") : "";
    }

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
            if (section.element.previousSibling && section.element.previousSibling.className === "sidebar-separator")
                InspectorTest.addResult("======== " + section.element.previousSibling.textContent + " ========");
            InspectorTest.addResult(section.expanded ? "[expanded] " : "[collapsed] ");
            var chainEntries = section.titleElement.children;
            for (var j = 0; j < chainEntries.length; ++j) {
                var chainEntry = chainEntries[j];
                var entryLine = chainEntry.children[1].textContent;
                if (chainEntry.children[2])
                    entryLine += " " + chainEntry.children[2].textContent;
                entryLine += " (" + extractText(chainEntry.children[0]) + ")";
                InspectorTest.addResult(entryLine);
            }
            section.expand();
            InspectorTest.dumpStyleTreeOutline(section.propertiesTreeOutline, omitLonghands ? 1 : 2);
            InspectorTest.addResult("");
        }
        InspectorTest.addResult("");
    }
};

InspectorTest.expandAndDumpSelectedElementEventListeners = function(callback)
{
    InspectorTest.expandSelectedElementEventListeners(function() {
        InspectorTest.dumpSelectedElementEventListeners(callback);
    });
}

InspectorTest.expandSelectedElementEventListeners = function(callback)
{
    var sidebarPane = WebInspector.panels.elements.sidebarPanes.eventListeners;
    sidebarPane.expand();

    InspectorTest.runAfterPendingDispatches(function() {
        InspectorTest.expandSelectedElementEventListenersSubsections(callback);
    });
}

InspectorTest.expandSelectedElementEventListenersSubsections = function(callback)
{
    var eventListenerSections = WebInspector.panels.elements.sidebarPanes.eventListeners.sections;
    for (var i = 0; i < eventListenerSections.length; ++i)
        eventListenerSections[i].expand();

    // Multiple sections may expand.
    InspectorTest.runAfterPendingDispatches(function() {
        InspectorTest.expandSelectedElementEventListenersEventBars(callback);
    });
}

InspectorTest.expandSelectedElementEventListenersEventBars = function(callback)
{
    var eventListenerSections = WebInspector.panels.elements.sidebarPanes.eventListeners.sections;
    for (var i = 0; i < eventListenerSections.length; ++i) {
        var eventBarChildren = eventListenerSections[i].eventBars.children;
        for (var j = 0; j < eventBarChildren.length; ++j)
            eventBarChildren[j]._section.expand();
    }

    // Multiple sections may expand.
    InspectorTest.runAfterPendingDispatches(callback);
}

InspectorTest.dumpSelectedElementEventListeners = function(callback)
{
    function formatSourceNameProperty(value)
    {
        return "[clipped-for-test]/" + value.replace(/(.*?\/)LayoutTests/, "LayoutTests");
    }

    var eventListenerSections = WebInspector.panels.elements.sidebarPanes.eventListeners.sections;
    for (var i = 0; i < eventListenerSections.length; ++i) {
        var section = eventListenerSections[i];
        var eventType = section._title;
        InspectorTest.addResult("");
        InspectorTest.addResult("======== " + eventType + " ========");
        var eventBarChildren = section.eventBars.children;
        for (var j = 0; j < eventBarChildren.length; ++j) {
            var objectPropertiesSection = eventBarChildren[j]._section;
            InspectorTest.dumpObjectPropertySection(objectPropertiesSection, {
                sourceName: formatSourceNameProperty
            });
        }
    }

    callback();
}

InspectorTest.dumpObjectPropertySection = function(section, formatters)
{
    var expandedSubstring = section.expanded ? "[expanded]" : "[collapsed]";
    InspectorTest.addResult(expandedSubstring + " " + section.titleElement.textContent + " " + section.subtitleAsTextForTest);
    if (!section.propertiesForTest)
        return;

    for (var i = 0; i < section.propertiesForTest.length; ++i) {
        var property = section.propertiesForTest[i];
        var key = property.name;
        var value = property.value._description;
        if (key in formatters)
            value = formatters[key](value);
        InspectorTest.addResult("    " + key + ": " + value);
    }
}

// FIXME: this returns the first tree item found (may fail for same-named properties in a style).
InspectorTest.getElementStylePropertyTreeItem = function(propertyName)
{
    var styleSections = WebInspector.panels.elements.sidebarPanes.styles.sections[0];
    var elementStyleSection = styleSections[1];
    return InspectorTest.getFirstPropertyTreeItemForSection(elementStyleSection, propertyName);
};

// FIXME: this returns the first tree item found (may fail for same-named properties in a style).
InspectorTest.getMatchedStylePropertyTreeItem = function(propertyName)
{
    var styleSections = WebInspector.panels.elements.sidebarPanes.styles.sections[0];
    for (var i = 1; i < styleSections.length; ++i) {
        var treeItem = InspectorTest.getFirstPropertyTreeItemForSection(styleSections[i], propertyName);
        if (treeItem)
            return treeItem;
    }
    return null;
};

InspectorTest.getFirstPropertyTreeItemForSection = function(section, propertyName)
{
    var outline = section.propertiesTreeOutline;
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

    WebInspector.panels.elements.treeOutline._updateModifiedNodes();
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

    function onAllNodesAvailable()
    {
        WebInspector.panels.elements.treeOutline._updateModifiedNodes();
        expand(WebInspector.panels.elements.treeOutline);
        callback();
    }
    WebInspector.inspectorView.setCurrentPanel(WebInspector.panels.elements);
    InspectorTest.findNode(function() { return false; }, onAllNodesAvailable);
};

InspectorTest.dumpDOMAgentTree = function()
{
    if (!WebInspector.domAgent._document)
        return;

    function dump(node, prefix, startIndex)
    {
        InspectorTest.addResult(prefix + node.nodeName() + "[" + (node.id - startIndex)+ "]");
        var children = node.children;
        for (var i = 0; children && i < children.length; ++i)
            dump(children[i], prefix + "    ", startIndex);
    }
    dump(WebInspector.domAgent._document, "", WebInspector.domAgent._document.id - 1);
};

InspectorTest.rangeText = function(range)
{
    if (!range)
        return "[undefined-undefined]";
    return "[" + range.start + "-" + range.end + "]";
}

InspectorTest.generateUndoTest = function(testBody)
{
    function result(next)
    {
        var testNode = InspectorTest.expandedNodeWithId(/function\s([^(]*)/.exec(testBody)[1]);
        InspectorTest.addResult("Initial:");
        InspectorTest.dumpElementsTree(testNode);

        testBody(step1);

        function step1()
        {
            InspectorTest.addResult("Post-action:");
            InspectorTest.dumpElementsTree(testNode);
            DOMAgent.undo(step2);
        }

        function step2()
        {
            InspectorTest.addResult("Post-undo (initial):");
            InspectorTest.dumpElementsTree(testNode);
            next();
        }
    }
    result.toString = function()
    {
        return testBody.toString();
    }
    return result;
}

};
