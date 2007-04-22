/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

var Inspector;
var rootDOMNode = null;
var focusedDOMNode = null;
var ignoreWhitespace = true;
var showUserAgentStyles = true;

// Property values to omit in the computed style list.
// If a property has this value, it will be omitted.
// Note we do not provide a value for "display", "height", or "width", for example,
// since we always want to display those.
var typicalStylePropertyValue = {
    "-webkit-appearance": "none",
    "-webkit-background-clip": "border",
    "-webkit-background-composite": "source-over",
    "-webkit-background-origin": "padding",
    "-webkit-background-size": "auto auto",
    "-webkit-border-fit": "border",
    "-webkit-border-horizontal-spacing": "0px",
    "-webkit-border-vertical-spacing": "0px",
    "-webkit-box-align": "stretch",
    "-webkit-box-direction": "normal",
    "-webkit-box-flex": "0",
    "-webkit-box-flex-group": "1",
    "-webkit-box-lines": "single",
    "-webkit-box-ordinal-group": "1",
    "-webkit-box-orient": "horizontal",
    "-webkit-box-pack": "start",
    "-webkit-box-shadow": "none",
    "-webkit-column-break-after" : "auto",
    "-webkit-column-break-before" : "auto",
    "-webkit-column-break-inside" : "auto",
    "-webkit-column-count" : "auto",
    "-webkit-column-gap" : "normal",
    "-webkit-column-rule-color" : "rgb(0, 0, 0)",
    "-webkit-column-rule-style" : "none",
    "-webkit-column-rule-width" : "0px",
    "-webkit-column-width" : "auto",
    "-webkit-highlight": "none",
    "-webkit-line-break": "normal",
    "-webkit-line-clamp": "none",
    "-webkit-margin-bottom-collapse": "collapse",
    "-webkit-margin-top-collapse": "collapse",
    "-webkit-marquee-direction": "auto",
    "-webkit-marquee-increment": "6px",
    "-webkit-marquee-repetition": "infinite",
    "-webkit-marquee-style": "scroll",
    "-webkit-nbsp-mode": "normal",
    "-webkit-rtl-ordering": "logical",
    "-webkit-text-decorations-in-effect": "none",
    "-webkit-text-fill-color": "rgb(0, 0, 0)",
    "-webkit-text-security": "none",
    "-webkit-text-stroke-color": "rgb(0, 0, 0)",
    "-webkit-text-stroke-width": "0",
    "-webkit-user-drag": "auto",
    "-webkit-user-modify": "read-only",
    "-webkit-user-select": "auto",
    "background-attachment": "scroll",
    "background-color": "rgba(0, 0, 0, 0)",
    "background-image": "none",
    "background-position-x": "auto",
    "background-position-y": "auto",
    "background-repeat": "repeat",
    "border-bottom-color": "rgb(0, 0, 0)",
    "border-bottom-style": "none",
    "border-bottom-width": "0px",
    "border-collapse": "separate",
    "border-left-color": "rgb(0, 0, 0)",
    "border-left-style": "none",
    "border-left-width": "0px",
    "border-right-color": "rgb(0, 0, 0)",
    "border-right-style": "none",
    "border-right-width": "0px",
    "border-top-color": "rgb(0, 0, 0)",
    "border-top-style": "none",
    "border-top-width": "0px",
    "bottom": "auto",
    "box-sizing": "content-box",
    "caption-side": "top",
    "clear": "none",
    "color": "rgb(0, 0, 0)",
    "cursor": "auto",
    "direction": "ltr",
    "empty-cells": "show",
    "float": "none",
    "font-style": "normal",
    "font-variant": "normal",
    "font-weight": "normal",
    "left": "auto",
    "letter-spacing": "normal",
    "line-height": "normal",
    "list-style-image": "none",
    "list-style-position": "outside",
    "list-style-type": "disc",
    "margin-bottom": "0px",
    "margin-left": "0px",
    "margin-right": "0px",
    "margin-top": "0px",
    "max-height": "none",
    "max-width": "none",
    "min-height": "0px",
    "min-width": "0px",
    "opacity": "1",
    "orphans": "2",
    "outline-color": "rgb(0, 0, 0)",
    "outline-style": "none",
    "outline-width": "0px",
    "overflow": "visible",
    "overflow-x": "visible",
    "overflow-y": "visible",
    "padding-bottom": "0px",
    "padding-left": "0px",
    "padding-right": "0px",
    "padding-top": "0px",
    "page-break-after": "auto",
    "page-break-before": "auto",
    "page-break-inside": "auto",
    "position": "static",
    "resize": "none",
    "right": "auto",
    "table-layout": "auto",
    "text-align": "auto",
    "text-decoration": "none",
    "text-indent": "0px",
    "text-shadow": "none",
    "text-transform": "none",
    "top": "auto",
    "unicode-bidi": "normal",
    "vertical-align": "baseline",
    "visibility": "visible",
    "white-space": "normal",
    "widows": "2",
    "word-spacing": "0px",
    "word-wrap": "normal",
    "z-index": "normal",
};

// "Nicknames" for some common values that are easier to read.
var valueNickname = {
    "rgb(0, 0, 0)": "black",
    "rgb(255, 255, 255)": "white",
    "rgba(0, 0, 0, 0)": "transparent",
};

// Display types for which margin is ignored.
var noMarginDisplayType = {
    "table-cell": "no",
    "table-column": "no",
    "table-column-group": "no",
    "table-footer-group": "no",
    "table-header-group": "no",
    "table-row": "no",
    "table-row-group": "no",
};

// Display types for which padding is ignored.
var noPaddingDisplayType = {
    "table-column": "no",
    "table-column-group": "no",
    "table-footer-group": "no",
    "table-header-group": "no",
    "table-row": "no",
    "table-row-group": "no",
};

function setUpScrollbar(id)
{
    var bar = new VerticalScrollbar(document.getElementById(id));

    bar.setTrackStart("Images/scrollTrackTop.png", 18);
    bar.setTrackMiddle("Images/scrollTrackMiddle.png");
    bar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    bar.setThumbStart("Images/scrollThumbTop.png", 9);
    bar.setThumbMiddle("Images/scrollThumbMiddle.png");
    bar.setThumbEnd("Images/scrollThumbBottom.png", 9);

    return bar;
}

function loaded()
{
    treeOutlineScrollArea = new ScrollArea(document.getElementById("treeOutlineScrollView"),
        setUpScrollbar("treeOutlineScrollbar"));

    treeOutline = new TreeOutline(document.getElementById("treeOutline"));

    nodeContentsScrollArea = new ScrollArea(document.getElementById("nodeContentsScrollview"),
        setUpScrollbar("nodeContentsScrollbar"));
    elementAttributesScrollArea = new ScrollArea(document.getElementById("elementAttributesScrollview"),
        setUpScrollbar("elementAttributesScrollbar"));
    
    styleRulesScrollArea = new ScrollArea(document.getElementById("styleRulesScrollview"),
        setUpScrollbar("styleRulesScrollbar"));
    stylePropertiesScrollArea = new ScrollArea(document.getElementById("stylePropertiesScrollview"),
        setUpScrollbar("stylePropertiesScrollbar"));

    jsPropertiesScrollArea = new ScrollArea(document.getElementById("jsPropertiesScrollview"),
        setUpScrollbar("jsPropertiesScrollbar"));

    window.addEventListener("resize", refreshScrollbars, true);
    document.addEventListener("mousedown", changeFocus, true);
    document.addEventListener("focus", changeFocus, true);
    document.addEventListener("keypress", documentKeypress, true);
    document.getElementById("splitter").addEventListener("mousedown", topAreaResizeDragStart, true);

    currentFocusElement = document.getElementById("tree");

    toggleNoSelection(false);
    switchPane("node");
}

var currentFocusElement;

function changeFocus(event)
{
    var nextFocusElement;

    var current = event.target;
    while (current) {
        if (current.nodeName.toLowerCase() === "input")
            nextFocusElement = current;
        current = current.parentNode;
    }

    if (!nextFocusElement)
        nextFocusElement = event.target.firstParentWithClass("focusable");

    if (!nextFocusElement || (currentFocusElement && currentFocusElement === nextFocusElement))
        return;

    if (currentFocusElement) {
        currentFocusElement.removeStyleClass("focused");
        currentFocusElement.addStyleClass("blured");
    }

    currentFocusElement = nextFocusElement;

    if (currentFocusElement) {
        currentFocusElement.addStyleClass("focused");
        currentFocusElement.removeStyleClass("blured");
    }
}

function documentKeypress(event)
{
    if (!currentFocusElement || !currentFocusElement.id || !currentFocusElement.id.length)
        return;
    if (window[currentFocusElement.id + "Keypress"])
        eval(currentFocusElement.id + "Keypress(event)");
}

function refreshScrollbars()
{
    treeOutlineScrollArea.refresh();
    elementAttributesScrollArea.refresh();
    jsPropertiesScrollArea.refresh();
    nodeContentsScrollArea.refresh();
    stylePropertiesScrollArea.refresh();
    styleRulesScrollArea.refresh();
}

var searchActive = false;
var searchQuery = "";
var searchResults = [];

function performSearch(query)
{
    var treePopup = document.getElementById("treePopup");
    var searchField = document.getElementById("search");
    var searchCount = document.getElementById("searchCount");

    if (query.length && !searchActive) {
        treePopup.style.display = "none";
        searchCount.style.display = "block";
        searchField.style.width = "150px";
        searchActive = true;
    } else if (!query.length && searchActive) {
        treePopup.style.removeProperty("display");
        searchCount.style.removeProperty("display");
        searchField.style.removeProperty("width");
        searchActive = false;
    }

    searchQuery = query;
    refreshSearch();
}

function refreshSearch()
{
    searchResults = [];

    if (searchActive) {
        // perform the search
        treeOutline.removeChildrenRecursive();
        treeOutlineScrollArea.refresh();
        toggleNoSelection(true);

        var count = 0;
        var xpathQuery = searchQuery;
        if (searchQuery.indexOf("/") === -1) {
            var escapedQuery = searchQuery.escapeCharacters("'");
            xpathQuery = "//*[contains(name(),'" + escapedQuery + "') or contains(@*,'" + escapedQuery + "')] | //text()[contains(.,'" + escapedQuery + "')] | //comment()[contains(.,'" + escapedQuery + "')]";
        }

        try {
            var focusedNodeDocument = focusedDOMNode.ownerDocument;
            var nodeList = focusedNodeDocument.evaluate(xpathQuery, focusedNodeDocument, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
            for (var i = 0; i < nodeList.snapshotLength; ++i) {
                searchResults.push(nodeList.snapshotItem(i));
                count++;
            }
        } catch(err) {
            // ignore any exceptions. the query might be malformed, but we allow that 
        }

        for (var i = 0; i < searchResults.length; ++i) {
            var item = new DOMNodeTreeElement(searchResults[i]);
            treeOutline.appendChild(item);
            item.collapse();
            if (!i)
                item.select();
        }

        var searchCountElement = document.getElementById("searchCount");
        if (count === 1)
            searchCountElement.textContent = "1 node";
        else
            searchCountElement.textContent = count + " nodes";
    } else {
        // switch back to the DOM tree and reveal the focused node
        updateTreeOutline();
    }
}

var tabNames = ["node","metrics","style","properties"];
var currentPane = "node";
var paneUpdateState = [];
var noSelection = false;

function toggleNoSelection(state)
{
    noSelection = state;
    if (noSelection) {
        for (var i = 0; i < tabNames.length; ++i)
            document.getElementById(tabNames[i] + "Pane").style.display = "none";
        document.getElementById("noSelection").style.removeProperty("display");
    } else {
        document.getElementById("noSelection").style.display = "none";
        switchPane(currentPane);
    }
}

function switchPane(pane)
{
    currentPane = pane;

    for (var i = 0; i < tabNames.length; ++i) {
        var paneElement = document.getElementById(tabNames[i] + "Pane");
        var button = document.getElementById(tabNames[i] + "Button");
        if (!button.originalClassName)
            button.originalClassName = button.className;
        if (pane === tabNames[i]) {
            if (!noSelection)
                paneElement.style.removeProperty("display");
            button.className = button.originalClassName + " selected";
        } else {
            paneElement.style.display = "none";
            button.className = button.originalClassName;
        }
    }

    if (noSelection)
        return;

    if (!paneUpdateState[pane]) {
        var functionName = "update" + pane.charAt(0).toUpperCase() + pane.substr(1) + "Pane";
        if (window[functionName])
            eval(functionName + "()");
        paneUpdateState[pane] = true;
    } else {
        refreshScrollbars();
    }
}

function treeElementPopulate(element)
{
    if (element.children.length || element.whitespaceIgnored !== ignoreWhitespace)
        return;

    element.removeChildren();
    element.whitespaceIgnored = ignoreWhitespace;

    var node = (ignoreWhitespace ? firstChildSkippingWhitespace.call(element.representedObject) : element.representedObject.firstChild);
    while (node) {
        var item = new DOMNodeTreeElement(node);
        element.appendChild(item);
        node = ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
    }
}

function treeElementExpanded(element)
{
    treeOutlineScrollArea.refresh();
}

function treeElementCollapsed(element)
{
    treeOutlineScrollArea.refresh();
}

function treeElementRevealed(element)
{
    if (element._listItemNode)
        treeOutlineScrollArea.reveal(element._listItemNode);
}

function treeElementSelected(element)
{
    updateFocusedNode(element.representedObject);

    // update the disabled state of the traverse buttons
    var traverseUp = document.getElementById("traverseUp");
    if (searchActive)
        traverseUp.disabled = !treeOutline.selectedTreeElement.traversePreviousTreeElement(false);
    else
        traverseUp.disabled = (!element.representedObject.previousSibling && !element.representedObject.parentNode);

    var traverseDown = document.getElementById("traverseDown");
    if (searchActive)
        traverseDown.disabled = !treeOutline.selectedTreeElement.traverseNextTreeElement(false);
    else
        traverseDown.disabled = !traverseNextNode.call(element.representedObject, ignoreWhitespace);
}

function treeElementDoubleClicked(element)
{
    if (element.hasChildren)
        updateRootNode(element.representedObject);
}

function DOMNodeTreeElement(node)
{
    var title = nodeDisplayName.call(node).escapeHTML();
    var hasChildren = (ignoreWhitespace ? (firstChildSkippingWhitespace.call(node) ? true : false) : node.hasChildNodes());

    if (hasChildren) 
        title += " <span class=\"content\">" + nodeContentPreview.call(node) + "</span>";

    var item = new TreeElement(title, node, hasChildren);
    item.onpopulate = treeElementPopulate;
    item.onexpand = treeElementExpanded;
    item.oncollapse = treeElementCollapsed;
    item.onselect = treeElementSelected;
    item.onreveal = treeElementRevealed;
    item.ondblclick = treeElementDoubleClicked;
    if (hasChildren) 
        item.whitespaceIgnored = ignoreWhitespace;
    return item;
}

function revealNodeInTreeOutline(node)
{
    var item = treeOutline.findTreeElement(node);
    if (item) {
        item.reveal();
        return item;
    }

    var found = false;
    for (var i = 0; i < treeOutline.children.length; ++i) {
        item = treeOutline.children[i];
        if (item.representedObject === node || isAncestorNode.call(item.representedObject, node)) {
            found = true;
            break;
        }
    }

    if (!found)
        return null;

    var ancestors = []; 
    var currentNode = node; 
    while (currentNode) { 
        ancestors.unshift(currentNode); 
        if (currentNode === item.representedObject)
            break;
        currentNode = currentNode.parentNode; 
    }

    for (var i = 0; i < ancestors.length; ++i) {
        item = treeOutline.findTreeElement(ancestors[i]);
        if (ancestors[i] !== node)
            item.expand();
    }

    return item;
}

function updateRootNode(node)
{
    if (!node || !rootDOMNode || rootDOMNode !== node) {
        rootDOMNode = node;
        updateTreeOutline();
    }
}

function updateFocusedNode(node)
{
    if (!node || !focusedDOMNode || focusedDOMNode !== node) {
        var oldFocusNode = focusedDOMNode;
        focusedDOMNode = node;

        updatePanes();

        if (focusedDOMNode) {
            if (rootDOMNode !== focusedDOMNode && !isAncestorNode.call(rootDOMNode, focusedDOMNode)) {
                var newRoot = firstCommonNodeAncestor.call(oldFocusNode, focusedDOMNode);
                if (!newRoot && focusedDOMNode.parentNode)
                    newRoot = focusedDOMNode.parentNode;
                else if(!newRoot)
                    newRoot = focusedDOMNode;
                updateRootNode(newRoot);
            }

            var item = treeOutline.findTreeElement(focusedDOMNode);
            if (!item)
                item = revealNodeInTreeOutline(focusedDOMNode);
            if (item)
                item.select();

            Inspector.highlightDOMNode(focusedDOMNode);
        }
    }
}

function updateTreeOutline(dontRevealSelectedItem)
{
    if (searchActive)
        return;

    var item = treeOutline.findTreeElement(rootDOMNode);

    if (item && item.whitespaceIgnored !== ignoreWhitespace)
        item = null;
    if (item && item.parent)
        item.parent.removeChild(item);

    treeOutline.removeChildrenRecursive();

    if (!rootDOMNode) {
        treeOutlineScrollArea.refresh();
        return;
    }

    if (!item)
        item = new DOMNodeTreeElement(rootDOMNode);
    treeOutline.appendChild(item);
    item.expand();

    if (dontRevealSelectedItem)
        item = treeOutline.findTreeElement(focusedDOMNode);
    else
        item = revealNodeInTreeOutline(focusedDOMNode);

    if (item)
        item.select();

    treeOutlineScrollArea.refresh();

    var rootPopup = document.getElementById("treePopup");

    var resetPopup = true;
    for (var i = 0; i < rootPopup.options.length; ++i) {
        if (rootPopup.options[i].representedNode === rootDOMNode) {
            rootPopup.options[i].selected = true;
            resetPopup = false;
            break;
        }
    }

    if (!resetPopup)
        return;

    rootPopup.removeChildren();

    var currentNode = rootDOMNode;
    while (currentNode) {
        var option = document.createElement("option");
        option.representedNode = currentNode;
        option.textContent = nodeDisplayName.call(currentNode);
        if (currentNode === rootDOMNode)
            option.selected = true;
        rootPopup.insertBefore(option, rootPopup.firstChild);
        currentNode = currentNode.parentNode;
    }
}

function selectNewRoot(event)
{
    var rootPopup = document.getElementById("treePopup");
    var option = rootPopup.options[rootPopup.selectedIndex];
    updateRootNode(option.representedNode);
}

function treeKeypress(event)
{
    treeOutline.handleKeyEvent(event);
}

function traverseTreeBackward(event)
{
    var item;

    // traverse backward, holding the opton key will traverse only to the previous sibling
    if (event.altKey)
        item = treeOutline.selectedTreeElement.previousSibling;
    else
        item = treeOutline.selectedTreeElement.traversePreviousTreeElement(false);

    if (item) {
        item.reveal();
        item.select();
    }
}

function traverseTreeForward(event)
{
    var item;

    // traverse forward, holding the opton key will traverse only to the next sibling
    if (event.altKey)
        item = treeOutline.selectedTreeElement.nextSibling;
    else
        item = treeOutline.selectedTreeElement.traverseNextTreeElement(false);

    if (item) {
        item.reveal();
        item.select();
    }
}

function updatePanes()
{
    for (var i = 0; i < tabNames.length; ++i)
        paneUpdateState[tabNames[i]] = false;

    toggleNoSelection(!focusedDOMNode);
    if (noSelection)
        return;

    var functionName = "update" + currentPane.charAt(0).toUpperCase() + currentPane.substr(1) + "Pane";
    if (window[functionName])
        eval(functionName + "()");    
    paneUpdateState[currentPane] = true;
}

function updateElementAttributes()
{
    var attributesList = document.getElementById("elementAttributesList")

    attributesList.removeChildren();

    if (!focusedDOMNode.attributes.length)
        attributesList.innerHTML = "<span class=\"disabled\">(none)</span>";

    for (var i = 0; i < focusedDOMNode.attributes.length; ++i) {
        var attr = focusedDOMNode.attributes[i];
        var li = document.createElement("li");

        var span = document.createElement("span");
        span.className = "property";
        if (attr.namespaceURI)
            span.title = attr.namespaceURI;
        span.textContent = attr.name;
        li.appendChild(span);

        span = document.createElement("span");
        span.className = "relation";
        span.textContent = "=";
        li.appendChild(span);

        span = document.createElement("span");
        span.className = "value";
        span.textContent = "\"" + attr.value + "\"";
        span.title = attr.value;
        li.appendChild(span);

        if (attr.style) {
            span = document.createElement("span");
            span.className = "mapped";
            span.innerHTML = "(<a href=\"javascript:selectMappedStyleRule('" + attr.name + "')\">mapped style</a>)";
            li.appendChild(span);
        }

        attributesList.appendChild(li);
    }

    elementAttributesScrollArea.refresh();
}

function updateNodePane()
{
    if (!focusedDOMNode)
        return;

    if (focusedDOMNode.nodeType === Node.TEXT_NODE || focusedDOMNode.nodeType === Node.COMMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.removeProperty("display");

        document.getElementById("nodeContentsScrollview").textContent = focusedDOMNode.nodeValue;
        nodeContentsScrollArea.refresh();
    } else if (focusedDOMNode.nodeType === Node.ELEMENT_NODE) {
        document.getElementById("elementAttributes").style.removeProperty("display");
        document.getElementById("nodeContents").style.display = "none";

        updateElementAttributes();

        if (focusedDOMNode.namespaceURI && focusedDOMNode.namespaceURI.length > 0) {
            document.getElementById("nodeNamespace").textContent = focusedDOMNode.namespaceURI;
            document.getElementById("nodeNamespace").title = focusedDOMNode.namespaceURI;
            document.getElementById("nodeNamespaceRow").style.removeProperty("display");
        } else {
            document.getElementById("nodeNamespaceRow").style.display = "none";
        }
    } else if (focusedDOMNode.nodeType === Node.DOCUMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.display = "none";
    }

    document.getElementById("nodeType").textContent = nodeTypeName.call(focusedDOMNode);
    document.getElementById("nodeName").textContent = focusedDOMNode.nodeName;

    refreshScrollbars();
}

var styleRules = [];
var selectedStyleRuleIndex = 0;
var styleProperties = [];
var expandedStyleShorthands = [];

function updateStylePane()
{
    var styleNode = focusedDOMNode;
    if (styleNode.nodeType === Node.TEXT_NODE && styleNode.parentNode && styleNode.parentNode.nodeType === Node.ELEMENT_NODE)
        styleNode = styleNode.parentNode;
    var rulesArea = document.getElementById("styleRulesScrollview");
    var propertiesArea = document.getElementById("stylePropertiesTree");

    rulesArea.removeChildren();
    propertiesArea.removeChildren();
    styleRules = [];
    styleProperties = [];

    if (styleNode.nodeType === Node.ELEMENT_NODE) {
        document.getElementById("styleRules").style.removeProperty("display");
        document.getElementById("styleProperties").style.removeProperty("display");
        document.getElementById("noStyle").style.display = "none";

        var propertyCount = [];

        var computedStyle = styleNode.ownerDocument.defaultView.getComputedStyle(styleNode);
        if (computedStyle && computedStyle.length) {
            var computedObj = {
                isComputedStyle: true,
                selectorText: "Computed Style",
                style: computedStyle,
                subtitle: "",
            };
            styleRules.push(computedObj);
        }

        var styleNodeName = styleNode.nodeName.toLowerCase();
        for (var i = 0; i < styleNode.attributes.length; ++i) {
            var attr = styleNode.attributes[i];
            if (attr.style) {
                var attrStyle = {
                    attrName: attr.name,
                    style: attr.style,
                    subtitle: "element\u2019s \u201C" + attr.name + "\u201D attribute",
                };
                attrStyle.selectorText = styleNodeName + "[" + attr.name;
                if (attr.value.length)
                    attrStyle.selectorText += "=" + attr.value;
                attrStyle.selectorText += "]";
                styleRules.push(attrStyle);
            }
        }

        var matchedStyleRules = styleNode.ownerDocument.defaultView.getMatchedCSSRules(styleNode, "", !showUserAgentStyles);
        if (matchedStyleRules) {
            for (var i = 0; i < matchedStyleRules.length; ++i) {
                styleRules.push(matchedStyleRules[i]);
            }
        }

        if (styleNode.style && styleNode.style.length) {
            var inlineStyle = {
                selectorText: "Inline Style Attribute",
                style: styleNode.style,
                subtitle: "element\u2019s \u201Cstyle\u201D attribute",
            };
            styleRules.push(inlineStyle);
        }

        if (styleRules.length && selectedStyleRuleIndex >= styleRules.length)
            selectedStyleRuleIndex = (styleRules.length - 1);

        var priorityUsed = false;
        for (var i = (styleRules.length - 1); i >= 0; --i) {
            styleProperties[i] = [];

            var row = document.createElement("div");
            row.className = "row";
            if (i === selectedStyleRuleIndex)
                row.className += " selected";
            if (styleRules[i].isComputedStyle)
                row.className += " computedStyle";

            var cell = document.createElement("div");
            cell.className = "cell selector";
            var text = styleRules[i].selectorText;
            cell.textContent = text;
            cell.title = text;
            row.appendChild(cell);

            cell = document.createElement("div");
            cell.className = "cell stylesheet";
            var sheet;
            if (styleRules[i].subtitle)
                sheet = styleRules[i].subtitle;
            else if (styleRules[i].parentStyleSheet && styleRules[i].parentStyleSheet.href)
                sheet = styleRules[i].parentStyleSheet.href;
            else if (styleRules[i].parentStyleSheet && !styleRules[i].parentStyleSheet.ownerNode)
                sheet = "user agent stylesheet";
            else
                sheet = "inline stylesheet";
            cell.textContent = sheet;
            cell.title = sheet;
            row.appendChild(cell);

            row.styleRuleIndex = i;
            row.addEventListener("mousedown", styleRuleSelect, false);

            var style = styleRules[i].style;
            var styleShorthandLookup = [];
            for (var j = 0; j < style.length; ++j) {
                var prop = null;
                var name = style[j];
                var shorthand = style.getPropertyShorthand(name);
                if (shorthand)
                    prop = styleShorthandLookup[shorthand];

                if (!priorityUsed && style.getPropertyPriority(name).length)
                    priorityUsed = true;

                if (prop) {
                    prop.subProperties.push(name);
                } else {
                    prop = {
                        style: style,
                        subProperties: [name],
                        unusedProperties: [],
                        name: (shorthand ? shorthand : name),
                    };
                    styleProperties[i].push(prop);
                    if (shorthand) {
                        styleShorthandLookup[shorthand] = prop;
                        if (!propertyCount[shorthand]) {
                            propertyCount[shorthand] = 1;
                        } else {
                            prop.unusedProperties[shorthand] = true;
                            propertyCount[shorthand]++;
                        }
                    }
                }

                if (styleRules[i].isComputedStyle)
                    continue;

                if (!propertyCount[name]) {
                    propertyCount[name] = 1;
                } else {
                    prop.unusedProperties[name] = true;
                    propertyCount[name]++;
                }
            }

            if (styleRules[i].isComputedStyle && styleRules.length > 1) {
                var divider = document.createElement("hr");
                divider.className = "divider";
                rulesArea.insertBefore(divider, rulesArea.firstChild);
            }

            if (rulesArea.firstChild)
                rulesArea.insertBefore(row, rulesArea.firstChild);
            else
                rulesArea.appendChild(row);
        }

        if (priorityUsed) {
            // walk the properties again and account for !important
            var priorityCount = [];
            for (var i = 0; i < styleRules.length; ++i) {
                if (styleRules[i].isComputedStyle)
                    continue;
                var style = styleRules[i].style;
                for (var j = 0; j < styleProperties[i].length; ++j) {
                    var prop = styleProperties[i][j];
                    for (var k = 0; k < prop.subProperties.length; ++k) {
                        var name = prop.subProperties[k];
                        if (style.getPropertyPriority(name).length) {
                            if (!priorityCount[name]) {
                                if (prop.unusedProperties[name])
                                    prop.unusedProperties[name] = false;
                                priorityCount[name] = 1;
                            } else {
                                priorityCount[name]++;
                            }
                        } else if (priorityCount[name]) {
                            prop.unusedProperties[name] = true;
                        }
                    }
                }
            }
        }

        updateStyleProperties();
    } else {
        var noStyle = document.getElementById("noStyle");
        noStyle.textContent = "Can't style " + nodeTypeName.call(styleNode) + " nodes.";
        document.getElementById("styleRules").style.display = "none";
        document.getElementById("styleProperties").style.display = "none";
        noStyle.style.removeProperty("display");
    }

    styleRulesScrollArea.refresh();
}

function styleRuleSelect(event)
{
    var row = document.getElementById("styleRulesScrollview").firstChild;
    while (row) {
        if (row.nodeName.toLowerCase() === "div")
            row.className = "row";
        row = row.nextSibling;
    }

    row = event.currentTarget;
    row.className = "row selected";

    selectedStyleRuleIndex = row.styleRuleIndex;
    updateStyleProperties();
}

function populateStyleListItem(li, prop, name)
{
    var span = document.createElement("span");
    span.className = "property";
    span.textContent = name + ": ";
    li.appendChild(span);

    var value = prop.style.getPropertyValue(name);
    if (!value)
        return;

    span = document.createElement("span");
    span.className = "value";
    var textValue = valueNickname[value] ? valueNickname[value] : value;
    var priority = prop.style.getPropertyPriority(name);
    if (priority.length)
        textValue += " !" + priority;
    span.textContent = textValue + ";";
    span.title = textValue;
    li.appendChild(span);

    var colors = value.match(/(rgb\([0-9]+, [0-9]+, [0-9]+\))|(rgba\([0-9]+, [0-9]+, [0-9]+, [0-9]+\))/g);
    if (colors) {
        for (var k = 0; k < colors.length; ++k) {
            var swatch = document.createElement("span");
            swatch.className = "colorSwatch";
            swatch.style.backgroundColor = colors[k];
            li.appendChild(swatch);
        }
    }
}

function updateStyleProperties()
{
    var propertiesTree = document.getElementById("stylePropertiesTree");
    propertiesTree.removeChildren();

    if (selectedStyleRuleIndex >= styleProperties.length) {
        stylePropertiesScrollArea.refresh();
        return;
    }

    var properties = styleProperties[selectedStyleRuleIndex];
    var omitTypicalValues = styleRules[selectedStyleRuleIndex].isComputedStyle;
    for (var i = 0; i < properties.length; ++i) {
        var prop = properties[i];
        var name = prop.name;

        if (omitTypicalValues && typicalStylePropertyValue[name] == prop.style.getPropertyValue(name))
            continue;

        var mainli = document.createElement("li");
        if (prop.subProperties.length > 1) {
            mainli.className = "hasChildren";
            if (expandedStyleShorthands[name])
                mainli.className += " expanded";
            mainli.shorthand = name;
            var button = document.createElement("button");
            button.addEventListener("click", toggleStyleShorthand, false);
            mainli.appendChild(button);
        }

        populateStyleListItem(mainli, prop, name);
        propertiesTree.appendChild(mainli);

        var overloadCount = 0;
        if (prop.subProperties && prop.subProperties.length > 1) {
            var subTree = document.createElement("ul");
            if (!expandedStyleShorthands[name])
                subTree.style.display = "none";

            for (var j = 0; j < prop.subProperties.length; ++j) {
                var name = prop.subProperties[j];
                var li = document.createElement("li");
                if (prop.style.isPropertyImplicit(name) || prop.style.getPropertyValue(name) == "initial")
                    li.className = "implicit";

                if (prop.unusedProperties[name] || prop.unusedProperties[name]) {
                    li.className += " overloaded";
                    overloadCount++;
                }

                populateStyleListItem(li, prop, name);
                subTree.appendChild(li);
            }

            propertiesTree.appendChild(subTree);
        }

        if (prop.unusedProperties[name] || overloadCount === prop.subProperties.length)
            mainli.className += " overloaded";
    }

    stylePropertiesScrollArea.refresh();
}

function toggleStyleShorthand(event)
{
    var li = event.currentTarget.parentNode;
    if (li.hasStyleClass("expanded")) {
        li.removeStyleClass("expanded");
        li.nextSibling.style.display = "none";
        expandedStyleShorthands[li.shorthand] = false;
    } else {
        li.addStyleClass("expanded");
        li.nextSibling.style.removeProperty("display");
        expandedStyleShorthands[li.shorthand] = true;
    }

    stylePropertiesScrollArea.refresh();
}

function toggleIgnoreWhitespace()
{
    ignoreWhitespace = !ignoreWhitespace;
    updateTreeOutline(true);
}

function toggleShowUserAgentStyles()
{
    showUserAgentStyles = !showUserAgentStyles;
    updateStylePane();
}

function selectMappedStyleRule(attrName)
{
    if (!paneUpdateState["style"])
        updateStylePane();

    var attrName = attrName.toLowerCase();
    for (var i = 0; i < styleRules.length; ++i)
        if (styleRules[i].attrName.toLowerCase() === attrName)
            break;

    selectedStyleRuleIndex = i;

    var row = document.getElementById("styleRulesScrollview").firstChild;
    while (row) {
        if (row.nodeName.toLowerCase() === "div") {
            if (row.styleRuleIndex === selectedStyleRuleIndex)
                row.className = "row selected";
            else
                row.className = "row";
        }

        row = row.nextSibling;
    }

    styleRulesScrollArea.refresh();

    updateStyleProperties();
    switchPane("style");
}

function setMetric(style, name, suffix)
{
    var value = style.getPropertyValue(name + suffix);
    if (value == "" || value == "0px")
        value = "\u2012";
    else
        value = value.replace(/px$/, "");
    document.getElementById(name).textContent = value;
}

function setBoxMetrics(style, box, suffix)
{
    setMetric(style, box + "-left", suffix);
    setMetric(style, box + "-right", suffix);
    setMetric(style, box + "-top", suffix);
    setMetric(style, box + "-bottom", suffix);
}

function updateMetricsPane()
{
    var style;
    if (focusedDOMNode.nodeType === Node.ELEMENT_NODE)
        style = focusedDOMNode.ownerDocument.defaultView.getComputedStyle(focusedDOMNode);
    if (!style || !style.length) {
        document.getElementById("noMetrics").style.removeProperty("display");
        document.getElementById("marginBoxTable").style.display = "none";
        return;
    }

    document.getElementById("noMetrics").style.display = "none";
    document.getElementById("marginBoxTable").style.removeProperty("display");

    setBoxMetrics(style, "margin", "");
    setBoxMetrics(style, "border", "-width");
    setBoxMetrics(style, "padding", "");

    var size = style.getPropertyValue("width").replace(/px$/, "")
        + " \u00D7 "
        + style.getPropertyValue("height").replace(/px$/, "");
    document.getElementById("content").textContent = size;

    if (noMarginDisplayType[style.display] === "no")
        document.getElementById("marginBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("marginBoxTable").removeAttribute("hide");

    if (noPaddingDisplayType[style.display] === "no")
        document.getElementById("paddingBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("paddingBoxTable").removeAttribute("hide");
}

function updatePropertiesPane()
{
    // FIXME: Like the style pane, this should have a top item that's "all properties"
    // and separate items for each item in the prototype chain. For now, we implement
    // only the "all properties" part, and only for enumerable properties.

    var list = document.getElementById("jsPropertiesList");
    list.removeChildren();

    for (var name in focusedDOMNode) {
        var li = document.createElement("li");

        var span = document.createElement("span");
        span.className = "property";
        span.textContent = name + ": ";
        li.appendChild(span);

        var value = focusedDOMNode[name];

        span = document.createElement("span");
        span.className = "value";
        span.textContent = value;
        span.title = value;
        li.appendChild(span);

        list.appendChild(li);
    }

    jsPropertiesScrollArea.refresh();
}

function dividerDragStart(element, dividerDrag, dividerDragEnd, event, cursor) 
{
    element.dragging = true;
    element.dragLastY = event.clientY + window.scrollY;
    element.dragLastX = event.clientX + window.scrollX;
    document.addEventListener("mousemove", dividerDrag, true);
    document.addEventListener("mouseup", dividerDragEnd, true);
    document.body.style.cursor = cursor;
    event.preventDefault();
}

function dividerDragEnd(element, dividerDrag, dividerDragEnd, event) 
{
    element.dragging = false;
    document.removeEventListener("mousemove", dividerDrag, true);
    document.removeEventListener("mouseup", dividerDragEnd, true);
    document.body.style.removeProperty("cursor");
}

function topAreaResizeDragStart(event) 
{
    dividerDragStart(document.getElementById("splitter"), topAreaResizeDrag, topAreaResizeDragEnd, event, "row-resize");
}

function topAreaResizeDrag(event) 
{
    var element = document.getElementById("splitter");
    if (element.dragging) {
        var y = event.clientY + window.scrollY;
        var delta = element.dragLastY - y;
        element.dragLastY = y;

        var top = document.getElementById("top");
        top.style.height = (top.clientHeight - delta) + "px";

        var splitter = document.getElementById("splitter");
        splitter.style.top = (splitter.offsetTop - delta) + "px";

        var bottom = document.getElementById("bottom");
        bottom.style.top = (bottom.offsetTop - delta) + "px";

        window.resizeBy(0, (delta * -1));

        treeOutlineScrollArea.refresh();

        event.preventDefault();
    }
}

function topAreaResizeDragEnd(event) 
{
    dividerDragEnd(document.getElementById("splitter"), topAreaResizeDrag, topAreaResizeDragEnd, event);
}
