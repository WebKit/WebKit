/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

WebInspector.DocumentPanel = function(resource, views)
{
    var allViews = [{ title: "DOM" }];
    if (views)
        allViews = allViews.concat(views);

    WebInspector.SourcePanel.call(this, resource, allViews);

    var domView = this.views.dom;
    domView.show = function() { InspectorController.highlightDOMNode(panel.focusedDOMNode) };
    domView.hide = function() { InspectorController.hideDOMNodeHighlight() };

    domView.sideContentElement = document.createElement("div");
    domView.sideContentElement.className = "content side";

    domView.treeContentElement = document.createElement("div");
    domView.treeContentElement.className = "content tree outline-disclosure";

    domView.treeListElement = document.createElement("ol");
    domView.treeOutline = new TreeOutline(domView.treeListElement);
    domView.treeOutline.panel = this;

    domView.crumbsElement = document.createElement("div");
    domView.crumbsElement.className = "crumbs";

    domView.innerCrumbsElement = document.createElement("div");
    domView.crumbsElement.appendChild(domView.innerCrumbsElement);

    domView.sidebarPanes = {};
    domView.sidebarPanes.styles = new WebInspector.StylesSidebarPane();
    domView.sidebarPanes.metrics = new WebInspector.MetricsSidebarPane();
    domView.sidebarPanes.properties = new WebInspector.PropertiesSidebarPane();

    var panel = this;
    domView.sidebarPanes.styles.onexpand = function() { panel.updateStyles() };
    domView.sidebarPanes.metrics.onexpand = function() { panel.updateMetrics() };
    domView.sidebarPanes.properties.onexpand = function() { panel.updateProperties() };

    domView.sidebarPanes.styles.expanded = true;

    domView.sidebarElement = document.createElement("div");
    domView.sidebarElement.className = "sidebar";

    domView.sidebarElement.appendChild(domView.sidebarPanes.styles.element);
    domView.sidebarElement.appendChild(domView.sidebarPanes.metrics.element);
    domView.sidebarElement.appendChild(domView.sidebarPanes.properties.element);

    domView.sideContentElement.appendChild(domView.treeContentElement);
    domView.sideContentElement.appendChild(domView.crumbsElement);
    domView.treeContentElement.appendChild(domView.treeListElement);

    domView.sidebarResizeElement = document.createElement("div");
    domView.sidebarResizeElement.className = "sidebar-resizer-vertical sidebar-resizer-vertical-right";
    domView.sidebarResizeElement.addEventListener("mousedown", function(event) { panel.rightSidebarResizerDragStart(event) }, false);

    domView.contentElement.appendChild(domView.sideContentElement);
    domView.contentElement.appendChild(domView.sidebarElement);
    domView.contentElement.appendChild(domView.sidebarResizeElement);

    this.rootDOMNode = this.resource.documentNode;
}

WebInspector.DocumentPanel.prototype = {
    show: function()
    {
        WebInspector.SourcePanel.prototype.show.call(this);
        this.updateTreeSelection();
    },

    resize: function()
    {
        this.updateTreeSelection();
    },

    updateTreeSelection: function()
    {
        if (!this.views.dom.treeOutline || !this.views.dom.treeOutline.selectedTreeElement)
            return;
        var element = this.views.dom.treeOutline.selectedTreeElement;
        element.updateSelection();
    },

    get rootDOMNode()
    {
        return this._rootDOMNode;
    },

    set rootDOMNode(x)
    {
        if (this._rootDOMNode === x)
            return;

        this._rootDOMNode = x;

        this.updateBreadcrumb();
        this.updateTreeOutline();
    },

    get focusedDOMNode()
    {
        return this._focusedDOMNode;
    },

    set focusedDOMNode(x)
    {
        if (this.resource.category !== WebInspector.resourceCategories.documents) {
            InspectorController.log("Called set focusedDOMNode on a non-document resource " + this.resource.displayName + " which is not a document");
            return;
        }

        if (this._focusedDOMNode === x) {
            var nodeItem = this.revealNode(x);
            if (nodeItem)
                nodeItem.select();
            return;
        }

        this._focusedDOMNode = x;

        this.updateBreadcrumb();

        for (var pane in this.views.dom.sidebarPanes)
            this.views.dom.sidebarPanes[pane].needsUpdate = true;

        this.updateStyles();
        this.updateMetrics();
        this.updateProperties();

        InspectorController.highlightDOMNode(x);

        var nodeItem = this.revealNode(x);
        if (nodeItem)
            nodeItem.select();
    },

    revealNode: function(node)
    {
        var nodeItem = this.views.dom.treeOutline.findTreeElement(node, function(a, b) { return isAncestorNode.call(a, b); }, function(a) { return a.parentNode; });
        if (!nodeItem)
            return;

        nodeItem.reveal();
        return nodeItem;
    },

    updateTreeOutline: function()
    {
        this.views.dom.treeOutline.removeChildrenRecursive();

        if (!this.rootDOMNode)
            return;

        // FIXME: this could use findTreeElement to reuse a tree element if it already exists
        var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(this.rootDOMNode) : this.rootDOMNode.firstChild);
        while (node) {
            this.views.dom.treeOutline.appendChild(new WebInspector.DOMNodeTreeElement(node));
            node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
        }

        this.updateTreeSelection();
    },

    updateBreadcrumb: function()
    {
        if (!this.views || !this.views.dom.contentElement)
            return;
        var crumbs = this.views.dom.innerCrumbsElement;
        if (!crumbs)
            return;

        var handled = false;
        var foundRoot = false;
        var crumb = crumbs.firstChild;
        while (crumb) {
            if (crumb.representedObject === this.rootDOMNode)
                foundRoot = true;

            if (foundRoot)
                crumb.addStyleClass("hidden");
            else
                crumb.removeStyleClass("hidden");

            if (crumb.representedObject === this.focusedDOMNode) {
                crumb.addStyleClass("selected");
                handled = true;
            } else {
                crumb.removeStyleClass("selected");
            }

            crumb = crumb.nextSibling;
        }

        if (handled)
            return;

        crumbs.removeChildren();

        var panel = this;
        var selectCrumbFunction = function(event)
        {
            if (event.currentTarget.hasStyleClass("hidden"))
                panel.rootDOMNode = event.currentTarget.representedObject.parentNode;
            panel.focusedDOMNode = event.currentTarget.representedObject;
            event.preventDefault();
            event.stopPropagation();
        }

        var selectCrumbRootFunction = function(event)
        {
            panel.rootDOMNode = event.currentTarget.representedObject.parentNode;
            panel.focusedDOMNode = event.currentTarget.representedObject;
            event.preventDefault();
            event.stopPropagation();
        }

        foundRoot = false;
        var current = this.focusedDOMNode;
        while (current) {
            if (current.nodeType === Node.DOCUMENT_NODE)
                break;

            if (current === this.rootDOMNode)
                foundRoot = true;

            var crumb = document.createElement("span");
            crumb.className = "crumb";
            crumb.representedObject = current;
            crumb.addEventListener("mousedown", selectCrumbFunction, false);
            crumb.addEventListener("dblclick", selectCrumbRootFunction, false);

            var crumbTitle;
            switch (current.nodeType) {
                case Node.ELEMENT_NODE:
                    crumbTitle = current.nodeName.toLowerCase();
    
                    var value = current.getAttribute("id");
                    if (value && value.length)
                        crumbTitle += "#" + value;

                    value = current.getAttribute("class");
                    if (value && value.length) {
                        var classes = value.split(/\s+/);
                        var classesLength = classes.length;
                        for (var i = 0; i < classesLength; ++i) {
                            value = classes[i];
                            if (value && value.length)
                                crumbTitle += "." + value;
                        }
                    }

                    break;

                case Node.TEXT_NODE:
                    if (isNodeWhitespace.call(current))
                        crumbTitle = "(whitespace)";
                    else
                        crumbTitle = "(text)";
                    break

                case Node.COMMENT_NODE:
                    crumbTitle = "<!-->";
                    break;

                default:
                    crumbTitle = current.nodeName.toLowerCase();
            }

            crumb.textContent = crumbTitle;

            if (foundRoot)
                crumb.addStyleClass("hidden");
            if (current === this.focusedDOMNode)
                crumb.addStyleClass("selected");
            if (!crumbs.childNodes.length)
                crumb.addStyleClass("end");
            if (current.parentNode.nodeType === Node.DOCUMENT_NODE)
                crumb.addStyleClass("start");

            crumbs.appendChild(crumb);
            current = current.parentNode;
        }
    },

    updateStyles: function()
    {
        var stylesSidebarPane = this.views.dom.sidebarPanes.styles;
        if (!stylesSidebarPane.expanded || !stylesSidebarPane.needsUpdate)
            return;

        stylesSidebarPane.update(this.focusedDOMNode);
        stylesSidebarPane.needsUpdate = false;
    },

    updateMetrics: function()
    {
        var metricsSidebarPane = this.views.dom.sidebarPanes.metrics;
        if (!metricsSidebarPane.expanded || !metricsSidebarPane.needsUpdate)
            return;

        metricsSidebarPane.update(this.focusedDOMNode);
        metricsSidebarPane.needsUpdate = false;
    },

    updateProperties: function()
    {
        var propertiesSidebarPane = this.views.dom.sidebarPanes.properties;
        if (!propertiesSidebarPane.expanded || !propertiesSidebarPane.needsUpdate)
            return;

        propertiesSidebarPane.update(this.focusedDOMNode);
        propertiesSidebarPane.needsUpdate = false;
    },

    handleKeyEvent: function(event)
    {
        if (this.views.dom.treeOutline && this.currentView && this.currentView === this.views.dom)
            this.views.dom.treeOutline.handleKeyEvent(event);
    },

    rightSidebarResizerDragStart: function(event)
    {
        var panel = this; 
        WebInspector.dividerDragStart(this.views.dom.sidebarElement, function(event) { panel.rightSidebarResizerDrag(event) }, function(event) { panel.rightSidebarResizerDragEnd(event) }, event, "col-resize");
    },

    rightSidebarResizerDragEnd: function(event)
    {
        var panel = this;
        WebInspector.dividerDragEnd(this.views.dom.sidebarElement, function(event) { panel.rightSidebarResizerDrag(event) }, function(event) { panel.rightSidebarResizerDragEnd(event) }, event);
    },

    rightSidebarResizerDrag: function(event)
    {
        var rightSidebar = this.views.dom.sidebarElement;
        if (rightSidebar.dragging == true) {
            var x = event.clientX + window.scrollX;

            var leftSidebarWidth = window.getComputedStyle(document.getElementById("sidebar")).getPropertyCSSValue("width").getFloatValue(CSSPrimitiveValue.CSS_PX);
            var newWidth = Number.constrain(window.innerWidth - x, 100, window.innerWidth - leftSidebarWidth - 100);

            if (x == newWidth)
                rightSidebar.dragLastX = x;

            rightSidebar.style.width = newWidth + "px";
            this.views.dom.sideContentElement.style.right = newWidth + "px";
            this.views.dom.sidebarResizeElement.style.right = (newWidth - 3) + "px";

            this.updateTreeSelection();

            event.preventDefault();
        }
    }
}

WebInspector.DocumentPanel.prototype.__proto__ = WebInspector.SourcePanel.prototype;

WebInspector.DOMNodeTreeElement = function(node)
{
    var hasChildren = (Preferences.ignoreWhitespace ? (firstChildSkippingWhitespace.call(node) ? true : false) : node.hasChildNodes());
    var titleInfo = nodeTitleInfo.call(node, hasChildren, WebInspector.linkifyURL);

    if (titleInfo.hasChildren) 
        this.whitespaceIgnored = Preferences.ignoreWhitespace;

    TreeElement.call(this, titleInfo.title, node, titleInfo.hasChildren);
}

WebInspector.DOMNodeTreeElement.prototype = {
    updateSelection: function()
    {
        var listItemElement = this.listItemElement;
        if (!listItemElement)
            return;

        if (!this.selectionElement) {
            this.selectionElement = document.createElement("div");
            this.selectionElement.className = "selection selected";
            listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
        }

        this.selectionElement.style.height = listItemElement.offsetHeight + "px";
    },

    onpopulate: function()
    {
        if (this.children.length || this.whitespaceIgnored !== Preferences.ignoreWhitespace)
            return;

        this.removeChildren();
        this.whitespaceIgnored = Preferences.ignoreWhitespace;

        var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(this.representedObject) : this.representedObject.firstChild);
        while (node) {
            this.appendChild(new WebInspector.DOMNodeTreeElement(node));
            node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
        }

        if (this.representedObject.nodeType == Node.ELEMENT_NODE) {
            var title = "<span class=\"webkit-html-tag close\">&lt;/" + this.representedObject.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
            var item = new TreeElement(title, this.representedObject, false);
            item.selectable = false;
            this.appendChild(item);
        }
    },

    onexpand: function()
    {
        this.treeOutline.panel.updateTreeSelection();
    },

    oncollapse: function()
    {
        this.treeOutline.panel.updateTreeSelection();
    },

    onreveal: function()
    {
        if (!this.listItemElement || !this.treeOutline)
            return;
        this.treeOutline.panel.views.dom.treeContentElement.scrollToElement(this.listItemElement);
    },

    onselect: function()
    {
        this.treeOutline.panel.focusedDOMNode = this.representedObject;

        // Call updateSelection twice to make sure the height is correct,
        // the first time might have a bad height because we are in a weird tree state
        this.updateSelection();

        var element = this;
        setTimeout(function() { element.updateSelection() }, 0);
    },

    ondblclick: function()
    {
        var panel = this.treeOutline.panel;
        panel.rootDOMNode = this.representedObject.parentNode;
        panel.focusedDOMNode = this.representedObject;

        if (this.hasChildren && !this.expanded)
            this.expand();
    }
}

WebInspector.DOMNodeTreeElement.prototype.__proto__ = TreeElement.prototype;
