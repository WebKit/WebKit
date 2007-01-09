/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

var Inspector = null;
var showUserAgentStyles = true;

// Property values to omit in the computed style list.
// If a property has this value, it will be omitted.
// Note we do not provide a value for "display", "height", or "width", for example,
// since we always want to display those.
var typicalStylePropertyValue = {
    "-webkit-background-clip": "border",
    "-webkit-background-composite": "source-over",
    "-webkit-background-origin": "padding",
    "-webkit-background-size": "auto auto",
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
    "-webkit-marquee-direction": "auto",
    "-webkit-marquee-increment": "6px",
    "-webkit-marquee-repetition": "infinite",
    "-webkit-marquee-style": "scroll",
    "-webkit-nbsp-mode": "normal",
    "-webkit-text-decorations-in-effect": "none",
    "-webkit-text-fill-color": "rgb(0, 0, 0)",
    "-webkit-text-security": "none",
    "-webkit-text-stroke-color": "rgb(0, 0, 0)",
    "-webkit-text-stroke-width": "0",
    "-webkit-user-modify": "read-only",
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
    var bar = new AppleVerticalScrollbar(document.getElementById(id));

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
    treeScrollbar = setUpScrollbar("treeScrollbar");

    nodeContentsScrollArea = new AppleScrollArea(document.getElementById("nodeContentsScrollview"),
        setUpScrollbar("nodeContentsScrollbar"));
    elementAttributesScrollArea = new AppleScrollArea(document.getElementById("elementAttributesScrollview"),
        setUpScrollbar("elementAttributesScrollbar"));
    
    styleRulesScrollArea = new AppleScrollArea(document.getElementById("styleRulesScrollview"),
        setUpScrollbar("styleRulesScrollbar"));
    stylePropertiesScrollArea = new AppleScrollArea(document.getElementById("stylePropertiesScrollview"),
        setUpScrollbar("stylePropertiesScrollbar"));

    jsPropertiesScrollArea = new AppleScrollArea(document.getElementById("jsPropertiesScrollview"),
        setUpScrollbar("jsPropertiesScrollbar"));

    treeScrollbar._getViewToContentRatio = function() {
        var contentHeight = Inspector.treeViewScrollHeight();
        var height = document.getElementById("treeScrollArea").offsetHeight;
        if (contentHeight > height)
            return height / contentHeight;
        return 1.0;
    }

    treeScrollbar._computeTrackOffset = function() { return Inspector.treeViewOffsetTop(); }
    treeScrollbar._getContentLength = function() { return Inspector.treeViewScrollHeight(); }
    treeScrollbar._getViewLength = function() { return document.getElementById("treeScrollArea").offsetHeight; }
    treeScrollbar._canScroll = function() { return true; }

    treeScrollbar.scrollTo = function(pos) {
        Inspector.treeViewScrollTo(pos);
        this.verticalHasScrolled();
    }

    treeScrollbar.verticalHasScrolled = function() {
        var new_thumb_pos = this._thumbPositionForContentPosition(Inspector.treeViewOffsetTop());
        this._thumbStart = new_thumb_pos;
        this._thumb.style.top = new_thumb_pos + "px";
    }

    // much better AppleScrollArea reveal
    AppleScrollArea.prototype.reveal = function(node) {
        var offsetY = 0;
        var obj = node;
        do {
            offsetY += obj.offsetTop;
            obj = obj.offsetParent;
        } while (obj && obj != this.content);

        var offsetX = 0;
        obj = node;
        do {
            offsetX += obj.offsetLeft;
            obj = obj.offsetParent;
        } while (obj && obj != this.content);

        var top = this.content.scrollTop;
        var height = this.viewHeight;
        if ((top + height) < (offsetY + node.clientHeight)) 
            this.verticalScrollTo(offsetY - height + node.clientHeight);
        else if (top > offsetY)
            this.verticalScrollTo(offsetY);

        var left = this.content.scrollLeft;
        var width = this.viewWidth;
        if ((left + width) < (offsetX + node.clientWidth)) 
            this.horizontalScrollTo(offsetX - width + node.clientWidth);
        else if (left > offsetX)
            this.horizontalScrollTo(offsetX);
    }

    // Change the standard show/hide to include the entire scrollbar.
    // This lets the content reflow to use the additional space when the scrollbar is hidden.
    AppleScrollbar.prototype.hide = function() {
        this._track.style.display = "none";
        this.scrollbar.style.display = "none";
        this.hidden = true;
    }
    AppleScrollbar.prototype.show = function() {
        this._track.style.display = "block";
        this.scrollbar.style.removeProperty("display");
        if (this.hidden) {
            this.hidden = false;
            this.refresh();
        }
    }

    window.addEventListener("resize", refreshScrollbars, false);

    toggleNoSelection(false);
    switchPane("node");
}

function refreshScrollbars()
{
    elementAttributesScrollArea.refresh();
    jsPropertiesScrollArea.refresh();
    nodeContentsScrollArea.refresh();
    stylePropertiesScrollArea.refresh();
    styleRulesScrollArea.refresh();
}

var searchActive = false;

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

    Inspector.searchPerformed(query);
}

function resultsWithXpathQuery(query)
{
    var nodeList = null;
    try {
        var focusedNode = Inspector.focusedDOMNode();
        nodeList = focusedNode.document.evaluate(query, focusedNode.document, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
    } catch(err) {
        // ignore any exceptions. the query might be malformed, but we allow that 
    }
    return nodeList;
}

var tabNames = ["node","metrics","style","properties"];
var currentPane = "node";
var paneUpdateState = [];
var noSelection = false;

function toggleNoSelection(state)
{
    noSelection = state;
    if (noSelection) {
        for (var i = 0; i < tabNames.length; i++)
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
    for (var i = 0; i < tabNames.length; i++) {
        var paneElement = document.getElementById(tabNames[i] + "Pane");
        var button = document.getElementById(tabNames[i] + "Button");
        if (!button.originalClassName)
            button.originalClassName = button.className;
        if (pane == tabNames[i]) {
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
        eval("update" + pane.charAt(0).toUpperCase() + pane.substr(1) + "Pane()");
        paneUpdateState[pane] = true;
    } else {
        refreshScrollbars();
    }
}

function nodeTypeName(node)
{
    switch (node.nodeType) {
        case Node.ELEMENT_NODE: return "Element";
        case Node.ATTRIBUTE_NODE: return "Attribute";
        case Node.TEXT_NODE: return "Text";
        case Node.CDATA_SECTION_NODE: return "Character Data";
        case Node.ENTITY_REFERENCE_NODE: return "Entity Reference";
        case Node.ENTITY_NODE: return "Entity";
        case Node.PROCESSING_INSTRUCTION_NODE: return "Processing Instruction";
        case Node.COMMENT_NODE: return "Comment";
        case Node.DOCUMENT_NODE: return "Document";
        case Node.DOCUMENT_TYPE_NODE: return "Document Type";
        case Node.DOCUMENT_FRAGMENT_NODE: return "Document Fragment";
        case Node.NOTATION_NODE: return "Notation";
    }
    return "(unknown)";
}

function updatePanes()
{
    for (var i = 0; i < tabNames.length; i++)
        paneUpdateState[tabNames[i]] = false;
    if (noSelection)
        return;
    eval("update" + currentPane.charAt(0).toUpperCase() + currentPane.substr(1) + "Pane()");    
    paneUpdateState[currentPane] = true;
}

function updateElementAttributes()
{
    var focusedNode = Inspector.focusedDOMNode();
    var attributesList = document.getElementById("elementAttributesList")

    attributesList.innerHTML = "";

    if (!focusedNode.attributes.length)
        attributesList.innerHTML = "<span class=\"disabled\">(none)</span>";

    for (i = 0; i < focusedNode.attributes.length; i++) {
        var attr = focusedNode.attributes[i];
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
    if (!Inspector)
        return;
    var focusedNode = Inspector.focusedDOMNode();

    if (focusedNode.nodeType == Node.TEXT_NODE || focusedNode.nodeType == Node.COMMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.removeProperty("display");

        document.getElementById("nodeContentsScrollview").textContent = focusedNode.nodeValue;
        nodeContentsScrollArea.refresh();
    } else if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("elementAttributes").style.removeProperty("display");
        document.getElementById("nodeContents").style.display = "none";

        updateElementAttributes();
        
        if (focusedNode.namespaceURI.length > 0) {
            document.getElementById("nodeNamespace").textContent = focusedNode.namespaceURI;
            document.getElementById("nodeNamespace").title = focusedNode.namespaceURI;
            document.getElementById("nodeNamespaceRow").style.removeProperty("display");
        } else {
            document.getElementById("nodeNamespaceRow").style.display = "none";
        }
    } else if (focusedNode.nodeType == Node.DOCUMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.display = "none";
    }

    document.getElementById("nodeType").textContent = nodeTypeName(focusedNode);
    document.getElementById("nodeName").textContent = focusedNode.nodeName;

    refreshScrollbars();
}

var styleRules = null;
var selectedStyleRuleIndex = 0;
var styleProperties = null;
var expandedStyleShorthands = [];

function updateStylePane()
{
    var focusedNode = Inspector.focusedDOMNode();
    if (focusedNode.nodeType == Node.TEXT_NODE && focusedNode.parentNode && focusedNode.parentNode.nodeType == Node.ELEMENT_NODE)
        focusedNode = focusedNode.parentNode;
    var rulesArea = document.getElementById("styleRulesScrollview");
    var propertiesArea = document.getElementById("stylePropertiesTree");

    rulesArea.innerHTML = "";
    propertiesArea.innerHTML = "";
    styleRules = [];
    styleProperties = [];

    if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("styleRules").style.removeProperty("display");
        document.getElementById("styleProperties").style.removeProperty("display");
        document.getElementById("noStyle").style.display = "none";

        var propertyCount = [];

        var computedStyle = focusedNode.ownerDocument.defaultView.getComputedStyle(focusedNode);
        if (computedStyle && computedStyle.length) {
            var computedObj = {
                isComputedStyle: true,
                selectorText: "Computed Style",
                style: computedStyle,
                subtitle: "",
            };
            styleRules.push(computedObj);
        }

        var focusedNodeName = focusedNode.nodeName.toLowerCase();
        for (var i = 0; i < focusedNode.attributes.length; i++) {
            var attr = focusedNode.attributes[i];
            if (attr.style) {
                var attrStyle = {
                    attrName: attr.name,
                    style: attr.style,
                    subtitle: "element\u2019s \u201C" + attr.name + "\u201D attribute",
                };
                attrStyle.selectorText = focusedNodeName + "[" + attr.name;
                if (attr.value.length)
                    attrStyle.selectorText += "=" + attr.value;
                attrStyle.selectorText += "]";
                styleRules.push(attrStyle);
            }
        }

        var matchedStyleRules = focusedNode.ownerDocument.defaultView.getMatchedCSSRules(focusedNode, "", !showUserAgentStyles);
        if (matchedStyleRules) {
            for (var i = 0; i < matchedStyleRules.length; i++) {
                styleRules.push(matchedStyleRules[i]);
            }
        }

        if (focusedNode.style.length) {
            var inlineStyle = {
                selectorText: "Inline Style Attribute",
                style: focusedNode.style,
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
            if (i == selectedStyleRuleIndex)
                row.className += " focused";
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
            if (styleRules[i].subtitle != null)
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
            row.addEventListener("click", styleRuleSelect, true);

            var style = styleRules[i].style;
            var styleShorthandLookup = [];
            for (var j = 0; j < style.length; j++) {
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
            for (var i = 0; i < styleRules.length; i++) {
                if (styleRules[i].isComputedStyle)
                    continue;
                var style = styleRules[i].style;
                for (var j = 0; j < styleProperties[i].length; j++) {
                    var prop = styleProperties[i][j];
                    for (var k = 0; k < prop.subProperties.length; k++) {
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
        noStyle.textContent = "Can't style " + nodeTypeName(focusedNode) + " nodes.";
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
        if (row.nodeName == "DIV")
            row.className = "row";
        row = row.nextSibling;
    }

    row = event.currentTarget;
    row.className = "row focused";

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
        for (var k = 0; k < colors.length; k++) {
            var swatch = document.createElement("span");
            swatch.className = "colorSwatch";
            swatch.style.backgroundColor = colors[k];
            li.appendChild(swatch);
        }
    }
}

function updateStyleProperties()
{
    var focusedNode = Inspector.focusedDOMNode();
    var propertiesTree = document.getElementById("stylePropertiesTree");
    propertiesTree.innerHTML = "";

    if (selectedStyleRuleIndex >= styleProperties.length) {
        stylePropertiesScrollArea.refresh();
        return;
    }

    var properties = styleProperties[selectedStyleRuleIndex];
    var omitTypicalValues = styleRules[selectedStyleRuleIndex].isComputedStyle;
    for (var i = 0; i < properties.length; i++) {
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

            for (var j = 0; j < prop.subProperties.length; j++) {
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

        if (prop.unusedProperties[name] || overloadCount == prop.subProperties.length)
            mainli.className += " overloaded";
    }

    stylePropertiesScrollArea.refresh();
}

function toggleStyleShorthand(event)
{
    var li = event.currentTarget.parentNode;
    if (li.className.indexOf("expanded") != -1) {
        li.className = li.className.replace(/ expanded/, "");
        li.nextSibling.style.display = "none";
        expandedStyleShorthands[li.shorthand] = false;
    } else {
        li.className += " expanded";
        li.nextSibling.style.removeProperty("display");
        expandedStyleShorthands[li.shorthand] = true;
    }

    stylePropertiesScrollArea.refresh();
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

    for (var i = 0; i < styleRules.length; i++)
        if (styleRules[i].attrName == attrName)
            break;

    selectedStyleRuleIndex = i;

    var row = document.getElementById("styleRulesScrollview").firstChild;
    while (row) {
        if (row.nodeName == "DIV") {
            if (row.styleRuleIndex == selectedStyleRuleIndex)
                row.className = "row focused";
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
    var focusedNode = Inspector.focusedDOMNode();
    if (focusedNode.nodeType == Node.ELEMENT_NODE)
        style = focusedNode.ownerDocument.defaultView.getComputedStyle(focusedNode);
    if (!style || style.length == 0) {
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

    if (noMarginDisplayType[style.display] == "no")
        document.getElementById("marginBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("marginBoxTable").removeAttribute("hide");

    if (noPaddingDisplayType[style.display] == "no")
        document.getElementById("paddingBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("paddingBoxTable").removeAttribute("hide");
}

function updatePropertiesPane()
{
    // FIXME: Like the style pane, this should have a top item that's "all properties"
    // and separate items for each item in the prototype chain. For now, we implement
    // only the "all properties" part, and only for enumerable properties.

    var focusedNode = Inspector.focusedDOMNode();
    var list = document.getElementById("jsPropertiesList");
    list.innerHTML = "";

    for (var name in focusedNode) {
        var li = document.createElement("li");

        var span = document.createElement("span");
        span.className = "property";
        span.textContent = name + ": ";
        li.appendChild(span);

        var value = focusedNode[name];

        span = document.createElement("span");
        span.className = "value";
        span.textContent = value;
        span.title = value;
        li.appendChild(span);

        list.appendChild(li);
    }

    jsPropertiesScrollArea.refresh();
}

// This is a workaround for rdar://4901491 - Dashboard AppleClasses try to set a NaN value and break the scrollbar.
AppleVerticalScrollbar.prototype._setObjectLength = function(object, length)
{
    if (!isNaN(length))
        object.style.height = length + "px";
}
