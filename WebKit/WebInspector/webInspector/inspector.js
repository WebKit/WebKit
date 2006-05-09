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

function loaded()
{
    treeScrollbar = new AppleVerticalScrollbar(document.getElementById("treeScrollbar"));

    treeScrollbar.setTrackStart("Images/scrollTrackTop.png", 18);
    treeScrollbar.setTrackMiddle("Images/scrollTrackMiddle.png");
    treeScrollbar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    treeScrollbar.setThumbStart("Images/scrollThumbTop.png", 9);
    treeScrollbar.setThumbMiddle("Images/scrollThumbMiddle.png");
    treeScrollbar.setThumbEnd("Images/scrollThumbBottom.png", 9);

    nodeContentsScrollbar = new AppleVerticalScrollbar(document.getElementById("nodeContentsScrollbar"));
    nodeContentsScrollArea = new AppleScrollArea(document.getElementById("nodeContentsScrollview"), nodeContentsScrollbar);

    nodeContentsScrollbar.setTrackStart("Images/scrollTrackTop.png", 18);
    nodeContentsScrollbar.setTrackMiddle("Images/scrollTrackMiddle.png");
    nodeContentsScrollbar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    nodeContentsScrollbar.setThumbStart("Images/scrollThumbTop.png", 9);
    nodeContentsScrollbar.setThumbMiddle("Images/scrollThumbMiddle.png");
    nodeContentsScrollbar.setThumbEnd("Images/scrollThumbBottom.png", 9);

    elementAttributesScrollbar = new AppleVerticalScrollbar(document.getElementById("elementAttributesScrollbar"));
    elementAttributesScrollArea = new AppleScrollArea(document.getElementById("elementAttributesScrollview"), elementAttributesScrollbar);
    
    elementAttributesScrollbar.setTrackStart("Images/scrollTrackTop.png", 18);
    elementAttributesScrollbar.setTrackMiddle("Images/scrollTrackMiddle.png");
    elementAttributesScrollbar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    elementAttributesScrollbar.setThumbStart("Images/scrollThumbTop.png", 9);
    elementAttributesScrollbar.setThumbMiddle("Images/scrollThumbMiddle.png");
    elementAttributesScrollbar.setThumbEnd("Images/scrollThumbBottom.png", 9);
    
    styleRulesScrollbar = new AppleVerticalScrollbar(document.getElementById("styleRulesScrollbar"));
    styleRulesScrollArea = new AppleScrollArea(document.getElementById("styleRulesScrollview"), styleRulesScrollbar);

    styleRulesScrollbar.setTrackStart("Images/scrollTrackTop.png", 18);
    styleRulesScrollbar.setTrackMiddle("Images/scrollTrackMiddle.png");
    styleRulesScrollbar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    styleRulesScrollbar.setThumbStart("Images/scrollThumbTop.png", 9);
    styleRulesScrollbar.setThumbMiddle("Images/scrollThumbMiddle.png");
    styleRulesScrollbar.setThumbEnd("Images/scrollThumbBottom.png", 9);

    stylePropertiesScrollbar = new AppleVerticalScrollbar(document.getElementById("stylePropertiesScrollbar"));
    stylePropertiesScrollArea = new AppleScrollArea(document.getElementById("stylePropertiesScrollview"), stylePropertiesScrollbar);

    stylePropertiesScrollbar.setTrackStart("Images/scrollTrackTop.png", 18);
    stylePropertiesScrollbar.setTrackMiddle("Images/scrollTrackMiddle.png");
    stylePropertiesScrollbar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    stylePropertiesScrollbar.setThumbStart("Images/scrollThumbTop.png", 9);
    stylePropertiesScrollbar.setThumbMiddle("Images/scrollThumbMiddle.png");
    stylePropertiesScrollbar.setThumbEnd("Images/scrollThumbBottom.png", 9);

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

    // Change the standard show/hide to include the parentNode.
    // This lets out content reflow when hidden.
    AppleScrollbar.prototype.hide = function() {
        this._track.style.display = "none";
        this.scrollbar.style.display = "none";
        this.hidden = true;
    }

    AppleScrollbar.prototype.show = function() {
        this._track.style.display = "block";
        this.scrollbar.style.display = null;
        if (this.hidden) {
            this.hidden = false;
            this.refresh();
        }
    }

    window.addEventListener("resize", refreshScrollbars, false);
}

function refreshScrollbars()
{
    if (currentPane == "node") {
        nodeContentsScrollArea.refresh();
        elementAttributesScrollArea.refresh();
    } else if (currentPane == "style") {
        styleRulesScrollArea.refresh();
        stylePropertiesScrollArea.refresh();
    }
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
        treePopup.style.display = null;
        searchCount.style.display = null;
        searchField.style.width = null;
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
var paneUpdateState = new Array();
var noSelection = false;

function toggleNoSelection(state)
{
    noSelection = state;
    if (noSelection) {
        for (var i = 0; i < tabNames.length; i++)
            document.getElementById(tabNames[i] + "Pane").style.display = "none";
        document.getElementById("noSelection").style.display = null;
    } else {
        document.getElementById("noSelection").style.display = "none";
        switchPane(currentPane);
    }
}

function switchPane(pane)
{
    currentPane = pane;
    for (var i = 0; i < tabNames.length; i++) {
        if (pane == tabNames[i]) {
            if (!noSelection)
                document.getElementById(tabNames[i] + "Pane").style.display = null;
            document.getElementById(tabNames[i] + "Button").className = "square selected";
        } else {
            document.getElementById(tabNames[i] + "Pane").style.display = "none";
            document.getElementById(tabNames[i] + "Button").className = "square";
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
        span.className = "value";
        span.textContent = attr.value;
        span.title = span.textContent;
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

function updateNodePane() {
    var focusedNode = Inspector.focusedDOMNode();

    if (focusedNode.nodeType == Node.TEXT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.display = null;

        document.getElementById("nodeContentsScrollview").textContent = focusedNode.nodeValue;
        nodeContentsScrollArea.refresh();
    } else if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("elementAttributes").style.display = null;
        document.getElementById("nodeContents").style.display = null;
        
        updateElementAttributes();
        
        if (focusedNode.namespaceURI.length > 0) {
            document.getElementById("nodeNamespace").textContent = focusedNode.namespaceURI;
            document.getElementById("nodeNamespace").title = focusedNode.namespaceURI;
            document.getElementById("nodeNamespaceRow").style.display = null;
        } else {
            document.getElementById("nodeNamespaceRow").style.display = "none";
        }

        document.getElementById("nodeContentsScrollview").innerHTML = "<span class=\"disabled\">Loading...</span>";
        nodeContentsScrollArea.refresh();

        clearTimeout(nodeUpdateTimeout);
        nodeUpdateTimeout = setTimeout("delayedNodePaneUpdate()", 250);
    } else if (focusedNode.nodeType == Node.DOCUMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.display = "none";
    }

    document.getElementById("nodeType").textContent = nodeTypeName(focusedNode);
    document.getElementById("nodeName").textContent = focusedNode.nodeName;
}

var nodeUpdateTimeout = null;
function delayedNodePaneUpdate()
{
    var focusedNode = Inspector.focusedDOMNode();
    var serializer = new XMLSerializer();
    document.getElementById("nodeContentsScrollview").textContent = serializer.serializeToString(focusedNode);
    nodeContentsScrollArea.refresh();
}

var styleRules = null;
var selectedStyleRuleIndex = 0;
var styleProperties = null;
var expandedStyleShorthands = new Array();

function updateStylePane()
{
    var focusedNode = Inspector.focusedDOMNode();
    var rulesArea = document.getElementById("styleRulesScrollview");
    var propertiesArea = document.getElementById("stylePropertiesTree");

    rulesArea.innerHTML = "";
    propertiesArea.innerHTML = "";
    styleRules = new Array();
    styleProperties = new Array();

    if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("styleRules").style.display = null;
        document.getElementById("styleProperties").style.display = null;
        document.getElementById("noStyle").style.display = "none";

        var propertyCount = new Array();

        var computedStyle = focusedNode.ownerDocument.defaultView.getComputedStyle(focusedNode, "");
        if (computedStyle) {
            var computedObj = new Object();
            computedObj.computedStyle = true;
            computedObj.selectorText = "Computed Style";
            computedObj.style = computedStyle;
            computedObj.subtitle = "";
            styleRules.push(computedObj);
        }

        var focusedNodeName = focusedNode.nodeName.toLowerCase();
        for (var i = 0; i < focusedNode.attributes.length; i++) {
            var attr = focusedNode.attributes[i];
            if (attr.style) {
                var attrStyle = new Object();
                attrStyle.attr = attr.name;
                attrStyle.selectorText = focusedNodeName + "[" + attr.name;
                if (attr.value.length)
                    attrStyle.selectorText += "=" + attr.value;
                attrStyle.selectorText += "]";
                attrStyle.style = attr.style;
                attrStyle.subtitle = "element's \"" + attr.name + "\" attribute";
                styleRules.push(attrStyle);
            }
        }

        var matchedStyleRules = focusedNode.ownerDocument.defaultView.getMatchedCSSRules(focusedNode, "");
        if (matchedStyleRules) {
            for (var i = 0; i < matchedStyleRules.length; i++) {
                styleRules.push(matchedStyleRules[i]);
            }
        }

        if (focusedNode.style.length) {
            var inlineStyle = new Object();
            inlineStyle.selectorText = "Inline Style Attribute";
            inlineStyle.style = focusedNode.style;
            inlineStyle.subtitle = "element's \"style\" attribute";
            styleRules.push(inlineStyle);
        }

        if (styleRules.length && selectedStyleRuleIndex >= styleRules.length)
            selectedStyleRuleIndex = (styleRules.length - 1);

        var priorityUsed = false;
        for (var i = (styleRules.length - 1); i >= 0; --i) {
            styleProperties[i] = new Array();

            var row = document.createElement("div");
            row.className = "row";
            if (i == selectedStyleRuleIndex)
                row.className += " focused";
            if (styleRules[i].computedStyle)
                row.className += " computedStyle";

            var cell = document.createElement("div");
            cell.className = "cell selector";
            cell.title = styleRules[i].selectorText;
            cell.textContent = cell.title;
            row.appendChild(cell);

            cell = document.createElement("div");
            cell.className = "cell stylesheet";
            if (styleRules[i].subtitle != null)
                cell.title = styleRules[i].subtitle;
            else if (styleRules[i].parentStyleSheet && styleRules[i].parentStyleSheet.href)
                cell.title = styleRules[i].parentStyleSheet.href;
            else
                cell.title = "inline stylesheet";
            cell.textContent = cell.title;
            row.appendChild(cell);

            row.styleRuleIndex = i;
            row.addEventListener("click", styleRuleSelect, true);

            var style = styleRules[i].style;
            var styleShorthandLookup = new Array();
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
                    prop = new Object();
                    prop.style = style;
                    prop.subProperties = new Array(name);
                    prop.unusedProperties = new Array();
                    prop.name = (shorthand ? shorthand : name);
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

                if (styleRules[i].computedStyle)
                    continue;

                if (!propertyCount[name]) {
                    propertyCount[name] = 1;
                } else {
                    prop.unusedProperties[name] = true;
                    propertyCount[name]++;
                }
            }

            if (styleRules[i].computedStyle && styleRules.length > 1) {
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
            var priorityCount = new Array();
            for (var i = 0; i < styleRules.length; i++) {
                var style = styleRules[i].style;
                if (style.computedStyle)
                    continue;
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
        noStyle.style.display = null;
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
    for (var i = 0; i < properties.length; i++) {
        var prop = properties[i];
        var mainli = document.createElement("li");
        if (prop.subProperties.length > 1) {
            mainli.className = "hasChildren";
            if (expandedStyleShorthands[prop.name])
                mainli.className += " expanded";
            mainli.shorthand = prop.name;
            var button = document.createElement("button");
            button.addEventListener("click", toggleStyleShorthand, false);
            mainli.appendChild(button);
        }

        var span = document.createElement("span");
        span.className = "property";
        span.textContent = prop.name;
        mainli.appendChild(span);

        span = document.createElement("span");
        span.className = "value";
        span.title = prop.style.getPropertyValue(prop.name);
        if (prop.style.getPropertyPriority(prop.name).length)
            span.title += " !" + prop.style.getPropertyPriority(prop.name);
        span.textContent = span.title;
        mainli.appendChild(span);

        propertiesTree.appendChild(mainli);

        var overloadCount = 0;
        if (prop.subProperties && prop.subProperties.length > 1) {
            var subTree = document.createElement("ul");
            if (!expandedStyleShorthands[prop.name])
                subTree.style.display = "none";

            for (var j = 0; j < prop.subProperties.length; j++) {
                var name = prop.subProperties[j];
                var li = document.createElement("li");
                if (prop.style.isPropertyImplicit(name) || prop.style.getPropertyValue(name) == "initial")
                    li.className = "implicit";

                if (prop.unusedProperties[name] || prop.unusedProperties[prop.name]) {
                    li.className += " overloaded";
                    overloadCount++;
                }

                var span = document.createElement("span");
                span.className = "property";
                span.textContent = name;
                li.appendChild(span);

                span = document.createElement("span");
                span.className = "value";
                span.title = prop.style.getPropertyValue(name);
                if (prop.style.getPropertyPriority(name).length)
                    span.title += " !" + prop.style.getPropertyPriority(name);
                span.textContent = span.title;
                li.appendChild(span);

                subTree.appendChild(li);
            }

            propertiesTree.appendChild(subTree);
        }

        if (prop.unusedProperties[prop.name] || overloadCount == prop.subProperties.length)
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
        li.nextSibling.style.display = null;
        expandedStyleShorthands[li.shorthand] = true;
    }

    stylePropertiesScrollArea.refresh();
}

function selectMappedStyleRule(attrName)
{
    if (!paneUpdateState["style"])
        updateStylePane();

    for (var i = 0; i < styleRules.length; i++)
        if (styleRules[i].attr == attrName)
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

function updateMetricsPane()
{
}

function updatePropertiesPane()
{
}
