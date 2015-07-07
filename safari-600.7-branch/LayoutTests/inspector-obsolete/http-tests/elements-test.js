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
            WebInspector._updateFocusedNode(node.id);
        callback(node);
    }
    InspectorTest.nodeWithId(idValue, onNodeFound);
}

InspectorTest.waitForStyles = function(idValue, callback, requireRebuild)
{
    callback = InspectorTest.safeWrap(callback);

    (function sniff(node, rebuild)
    {
        if ((rebuild || !requireRebuild) && node && node.getAttribute("id") === idValue) {
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

    InspectorTest.waitForStyles(idValue, stylesUpdated, true);
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

InspectorTest.selectNodeAndWaitForStylesWithComputed = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);

    function stylesCallback(targetNode)
    {
        InspectorTest.addSniffer(WebInspector.SidebarPane.prototype, "onContentReady", callback);
        WebInspector.panels.elements.sidebarPanes.computedStyle.expand();
    }

    InspectorTest.selectNodeAndWaitForStyles(idValue, stylesCallback);
}

InspectorTest.dumpSelectedElementStyles = function(excludeComputed, excludeMatched, omitLonghands, includeSelectorGroupMarks)
{
    function extractText(element)
    {
        var text = element.textContent;
        if (text)
            return text;
        element = element.querySelector("[data-uncopyable]");
        return element ? element.getAttribute("data-uncopyable") : "";
    }

    function buildMarkedSelectors(element)
    {
        var result = "";
        for (var node = element.firstChild; node; node = node.nextSibling) {
            if (node.nodeType === Node.ELEMENT_NODE && node.className === "selector-matches")
                result += "[$" + node.textContent + "$]";
            else
                result += node.textContent;
        }
        return result;
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
            InspectorTest.addResult((section.expanded ? "[expanded] " : "[collapsed] ") + (section.noAffect ? "[no-affect] " : ""));
            var chainEntries = section.titleElement.children;
            for (var j = 0; j < chainEntries.length; ++j) {
                var chainEntry = chainEntries[j];
                var entryLine = includeSelectorGroupMarks ? buildMarkedSelectors(chainEntry.children[1]) : chainEntry.children[1].textContent;
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
}

InspectorTest.toggleStyleProperty = function(propertyName, checked)
{
    var treeItem = InspectorTest.getElementStylePropertyTreeItem(propertyName);
    treeItem.toggleEnabled({ target: { checked: checked }, consume: function() { } });
}

InspectorTest.toggleMatchedStyleProperty = function(propertyName, checked)
{
    var treeItem = InspectorTest.getMatchedStylePropertyTreeItem(propertyName);
    treeItem.toggleEnabled({ target: { checked: checked }, consume: function() { } });
}

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
        var eventBarChildren = eventListenerSections[i]._eventBars.children;
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
        var eventBarChildren = section._eventBars.children;
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

InspectorTest.dumpObjectPropertySectionDeep = function(section)
{
    function domNodeToString(node) {
        if (node)
            return "'" + node.textContent + "'";
        else
            return "null";
    }
    function dumpTreeElementRecursively(treeElement, prefix) {
        if ("nameElement" in treeElement)
            InspectorTest.addResult(prefix + domNodeToString(treeElement.nameElement) + " => " + domNodeToString(treeElement.valueElement));
        else
            InspectorTest.addResult(prefix + treeElement.title);
        for (var i = 0; i < treeElement.children.length; i++)
            dumpTreeElementRecursively(treeElement.children[i], prefix + "    ");
    }

    var childNodes = section.propertiesTreeOutline.children;
    for (var i = 0; i < childNodes.length; i++) {
        dumpTreeElementRecursively(childNodes[i], "");
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
    if (treeItem.listItemElement.hasStyleClass("overloaded") || treeItem.listItemElement.hasStyleClass("inactive") || treeItem.listItemElement.hasStyleClass("not-parsed-ok"))
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

InspectorTest.dumpElementsTree = function(rootNode, depth)
{
    function beautify(element)
    {
        return element.innerText.replace(/\u200b/g, "").replace(/\n/g, "\\n").trim();
    }

    function dumpMap(name, map)
    {
        var result = [];
        for (var id in map)
            result.push(id + "=" + map[id]);
        if (!result.length)
            return "";
        return name + ":[" + result.join(",") + "]";
    }

    function userPropertyDataDump(treeItem)
    {
        if (treeItem._elementCloseTag)
            return "";

        var userProperties = "";
        var node = treeItem.representedObject;
        if (node) {
            userProperties += dumpMap("userProperties", node._userProperties);
            var dump = dumpMap("descendantUserAttributeCounters", node._descendantUserPropertyCounters);
            if (dump) {
                if (userProperties)
                    userProperties += ", ";
                userProperties += dump;
            }
            if (userProperties)
                userProperties = " [" + userProperties + "]";
        }
        return userProperties;
    }

    function print(treeItem, prefix, depth)
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

            var userProperties = userPropertyDataDump(treeItem);
            InspectorTest.addResult(prefix + expander + beautify(treeItem.listItemElement) + userProperties);
        }

        if (!treeItem.expanded)
            return;

        var children = treeItem.children;
        for (var i = 0; depth && children && i < children.length - 1; ++i)
            print(children[i], prefix + "    ", depth - 1);

        // Closing tag.
        if (children && children.length)
            print(children[children.length - 1], prefix, depth);
    }

    WebInspector.panels.elements.treeOutline._updateModifiedNodes();
    var treeOutline = WebInspector.panels.elements.treeOutline;
    print(rootNode ? treeOutline.findTreeElement(rootNode) : treeOutline, "", depth || 10000);
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
    WebInspector.showPanel("elements");
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
    return "[" + range.startLine + ":" + range.startColumn + "-" + range.endLine + ":" + range.endColumn + "]";
}

InspectorTest.generateUndoTest = function(testBody)
{
    function result(next)
    {
        var testNode = InspectorTest.expandedNodeWithId(/function\s([^(]*)/.exec(testBody)[1]);
        InspectorTest.addResult("Initial:");
        InspectorTest.dumpElementsTree(testNode);

        testBody(undo);

        function undo()
        {
            InspectorTest.addResult("Post-action:");
            InspectorTest.dumpElementsTree(testNode);
            WebInspector.domAgent.undo(redo);
        }

        function redo()
        {
            InspectorTest.addResult("Post-undo (initial):");
            InspectorTest.dumpElementsTree(testNode);
            WebInspector.domAgent.redo(done);
        }

        function done()
        {
            InspectorTest.addResult("Post-redo (action):");
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

const indent = "    ";

InspectorTest.dumpRulesArray = function(rules, currentIndent)
{
    if (!rules)
        return;
    currentIndent = currentIndent || "";
    for (var i = 0; i < rules.length; ++i)
        InspectorTest.dumpRule(rules[i], currentIndent);
}

InspectorTest.dumpRuleMatchesArray = function(matches, currentIndent)
{
    if (!matches)
        return;
    currentIndent = currentIndent || "";
    for (var i = 0; i < matches.length; ++i)
        InspectorTest.dumpRule(matches[i].rule, currentIndent);
}

InspectorTest.dumpRule = function(rule, currentIndent)
{
    function selectorRange()
    {
        if (!rule.selectorList || !rule.selectorList.range)
            return "";
        var range = rule.selectorList.range;
        return ", " + range.startLine + ":" + range.startColumn + "-" + range.endLine + ":" + range.endColumn;
    }

    currentIndent = currentIndent || "";

    if (!rule.type || rule.type === "style") {
        InspectorTest.addResult(currentIndent + rule.selectorList.text + ": [" + rule.origin + selectorRange() + "] {");
        InspectorTest.dumpStyle(rule.style, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "media") {
        InspectorTest.addResult(currentIndent + "@media " + rule.mediaText + " {");
        InspectorTest.dumpRulesArray(rule.childRules, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "import") {
        InspectorTest.addResult(currentIndent + "@import: header=" + InspectorTest.rangeText(rule.headerRange) + ", body=" + InspectorTest.rangeText(rule.bodyRange));
        return;
    }

    if (rule.type === "page" || rule.type === "font-face") {
        if (rule.type === "page")
            InspectorTest.addResult(currentIndent + rule.selectorList.text + " {");
        else
            InspectorTest.addResult(currentIndent + "@" + rule.type + " " + (rule.selectorList.text ? rule.selectorList.text + " " : "")  + "{");
        InspectorTest.dumpStyle(rule.style, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "charset") {
        InspectorTest.addResult("@charset");
        return;
    }

    InspectorTest.addResult(currentIndent + "[UNKNOWN RULE]: header=" + InspectorTest.rangeText(rule.headerRange) + ", body=" + InspectorTest.rangeText(rule.bodyRange));
}

InspectorTest.dumpStyle = function(style, currentIndent)
{
    currentIndent = currentIndent || "";
    if (!style) {
        InspectorTest.addResult(currentIndent + "[NO STYLE]");
        return;
    }
    for (var i = 0; i < style.cssProperties.length; ++i) {
        var property = style.cssProperties[i];
        if (property.status !== "disabled")
            InspectorTest.addResult(currentIndent + "['" + property.name + "':'" + property.value + "'" + (property.priority === "important" ? " is-important" : "") + (("parsedOk" in property) ? " non-parsed" : "") +"] @" + InspectorTest.rangeText(property.range) + " " + (property.status || "style"));
        else
            InspectorTest.addResult(currentIndent + "[text='" + property.text + "'] " + property.status);
    }
}

};

function dumpInspectorHighlightRects()
{
    var rectNames = ["margin", "border", "padding", "content"];
    var rects = window.internals.inspectorHighlightRects();
    for (var i = 0; i < rects.length; i++)
    {
        var rectName = (i < rectNames.length ? rectNames[i] : "untitled");
        var rect = rects.item(i);
        output(rectName + " rect is " + rect.width + " x " + rect.height + " at (" + rect.left + ", " + rect.top + ")");
    }
}

function dumpInspectorHighlightedRegions()
{
    var highlight = window.internals.inspectorHighlightObject();
    if (!highlight.length) {
        output("No highlighted node.");
        return;
    }
    // Reformat the string using JSON.stringify.
    var json = JSON.parse(highlight);
    if (!json.elementInfo || !json.elementInfo.regionFlowInfo) {
        output("No highlighted regions.");
        return;
    }
    output(JSON.stringify(json.elementInfo.regionFlowInfo, null, 4));
}

function dumpInspectorHighlightedNode()
{
    var highlight = window.internals.inspectorHighlightObject();
    if (!highlight.length) {
        output("No highlighted node.");
        return;
    }
    // Reformat the string using JSON.stringify.
    var json = JSON.parse(highlight);
    if (!json) {
        output("No highlighted node.");
        return;
    }
    output(JSON.stringify(json, null, 4));
}
