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
    var allViews = [{ title: WebInspector.UIString("DOM"), name: "dom" }];
    if (views)
        allViews = allViews.concat(views);

    WebInspector.SourcePanel.call(this, resource, allViews);

    var panel = this;
    var domView = this.views.dom;
    domView.hide = function() { InspectorController.hideDOMNodeHighlight() };
    domView.show = function() {
        InspectorController.highlightDOMNode(panel.focusedDOMNode);
        panel.updateBreadcrumb();
        panel.updateTreeSelection();
    };

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
    domView.sidebarResizeElement.addEventListener("mousedown", this.rightSidebarResizerDragStart.bind(this), false);

    domView.contentElement.appendChild(domView.sideContentElement);
    domView.contentElement.appendChild(domView.sidebarElement);
    domView.contentElement.appendChild(domView.sidebarResizeElement);

    this.rootDOMNode = this.resource.documentNode;
}

WebInspector.DocumentPanel.prototype = {
    resize: function()
    {
        this.updateTreeSelection();
        this.updateBreadcrumbSizes();
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
        if (!this.visible)
            return;

        var crumbs = this.views.dom.innerCrumbsElement;

        var handled = false;
        var foundRoot = false;
        var crumb = crumbs.firstChild;
        while (crumb) {
            if (crumb.representedObject === this.rootDOMNode)
                foundRoot = true;

            if (foundRoot)
                crumb.addStyleClass("dimmed");
            else
                crumb.removeStyleClass("dimmed");

            if (crumb.representedObject === this.focusedDOMNode) {
                crumb.addStyleClass("selected");
                handled = true;
            } else {
                crumb.removeStyleClass("selected");
            }

            crumb = crumb.nextSibling;
        }

        if (handled) {
            // We don't need to rebuild the crumbs, but we need to adjust sizes
            // to reflect the new focused or root node.
            this.updateBreadcrumbSizes();
            return;
        }

        crumbs.removeChildren();

        var panel = this;
        var selectCrumbFunction = function(event) {
            var crumb = event.currentTarget;
            if (crumb.hasStyleClass("collapsed")) {
                // Clicking a collapsed crumb will expose the hidden crumbs.
                if (crumb === panel.views.dom.innerCrumbsElement.firstChild) {
                    // If the focused crumb is the first child, pick the farthest crumb
                    // that is still hidden. This allows the user to expose every crumb.
                    var currentCrumb = crumb;
                    while (currentCrumb) {
                        var hidden = currentCrumb.hasStyleClass("hidden");
                        var collapsed = currentCrumb.hasStyleClass("collapsed");
                        if (!hidden && !collapsed)
                            break;
                        crumb = currentCrumb;
                        currentCrumb = currentCrumb.nextSibling;
                    }
                }

                panel.updateBreadcrumbSizes(crumb);
            } else {
                // Clicking a dimmed crumb or double clicking (event.detail >= 2)
                // will change the root node in addition to the focused node.
                if (event.detail >= 2 || crumb.hasStyleClass("dimmed"))
                    panel.rootDOMNode = crumb.representedObject.parentNode;
                panel.focusedDOMNode = crumb.representedObject;
            }

            event.preventDefault();
        };

        var mouseOverCrumbFunction = function(event) {
            panel.mouseOverCrumb = true;

            if ("mouseOutTimeout" in panel) {
                clearTimeout(panel.mouseOutTimeout);
                delete panel.mouseOutTimeout;
            }
        };

        var mouseOutCrumbFunction = function(event) {
            delete panel.mouseOverCrumb;

            if ("mouseOutTimeout" in panel) {
                clearTimeout(panel.mouseOutTimeout);
                delete panel.mouseOutTimeout;
            }

            var timeoutFunction = function() {
                if (!panel.mouseOverCrumb)
                    panel.updateBreadcrumbSizes();
            };

            panel.mouseOutTimeout = setTimeout(timeoutFunction, 500);
        };

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
            crumb.addEventListener("mouseover", mouseOverCrumbFunction, false);
            crumb.addEventListener("mouseout", mouseOutCrumbFunction, false);

            var crumbTitle;
            switch (current.nodeType) {
                case Node.ELEMENT_NODE:
                    crumbTitle = current.nodeName.toLowerCase();

                    var nameElement = document.createElement("span");
                    nameElement.textContent = crumbTitle;
                    crumb.appendChild(nameElement);

                    var idAttribute = current.getAttribute("id");
                    if (idAttribute) {
                        var idElement = document.createElement("span");
                        crumb.appendChild(idElement);

                        var part = "#" + idAttribute;
                        crumbTitle += part;
                        idElement.appendChild(document.createTextNode(part));

                        // Mark the name as extra, since the ID is more important.
                        nameElement.className = "extra";
                    }

                    var classAttribute = current.getAttribute("class");
                    if (classAttribute) {
                        var classes = classAttribute.split(/\s+/);
                        var foundClasses = {};

                        if (classes.length) {
                            var classesElement = document.createElement("span");
                            classesElement.className = "extra";
                            crumb.appendChild(classesElement);

                            for (var i = 0; i < classes.length; ++i) {
                                var className = classes[i];
                                if (className && !(className in foundClasses)) {
                                    var part = "." + className;
                                    crumbTitle += part;
                                    classesElement.appendChild(document.createTextNode(part));
                                    foundClasses[className] = true;
                                }
                            }
                        }
                    }

                    break;

                case Node.TEXT_NODE:
                    if (isNodeWhitespace.call(current))
                        crumbTitle = WebInspector.UIString("(whitespace)");
                    else
                        crumbTitle = WebInspector.UIString("(text)");
                    break

                case Node.COMMENT_NODE:
                    crumbTitle = "<!-->";
                    break;

                default:
                    crumbTitle = current.nodeName.toLowerCase();
            }

            if (!crumb.childNodes.length) {
                var nameElement = document.createElement("span");
                nameElement.textContent = crumbTitle;
                crumb.appendChild(nameElement);
            }

            crumb.title = crumbTitle;

            if (foundRoot)
                crumb.addStyleClass("dimmed");
            if (current === this.focusedDOMNode)
                crumb.addStyleClass("selected");
            if (!crumbs.childNodes.length)
                crumb.addStyleClass("end");
            if (current.parentNode.nodeType === Node.DOCUMENT_NODE)
                crumb.addStyleClass("start");

            crumbs.appendChild(crumb);
            current = current.parentNode;
        }

        this.updateBreadcrumbSizes();
    },

    updateBreadcrumbSizes: function(focusedCrumb)
    {
        if (!this.visible)
            return;

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet, so we need to update later.
            setTimeout(this.updateBreadcrumbSizes.bind(this), 0);
            return;
        }

        var crumbs = this.views.dom.innerCrumbsElement;
        if (!crumbs.childNodes.length)
            return; // No crumbs, do nothing.

        var crumbsContainer = this.views.dom.crumbsElement;
        if (crumbsContainer.offsetWidth <= 0 || crumbs.offsetWidth <= 0)
            return;

        // A Zero index is the right most child crumb in the breadcrumb.
        var selectedIndex = 0;
        var focusedIndex = 0;
        var selectedCrumb;

        var i = 0;
        var crumb = crumbs.firstChild;
        while (crumb) {
            // Find the selected crumb and index. 
            if (!selectedCrumb && crumb.hasStyleClass("selected")) {
                selectedCrumb = crumb;
                selectedIndex = i;
            }

            // Find the focused crumb index. 
            if (crumb === focusedCrumb)
                focusedIndex = i;

            // Remove any styles that affect size before
            // deciding to shorten any crumbs.
            if (crumb !== crumbs.lastChild)
                crumb.removeStyleClass("start");
            if (crumb !== crumbs.firstChild)
                crumb.removeStyleClass("end");

            crumb.removeStyleClass("compact");
            crumb.removeStyleClass("collapsed");
            crumb.removeStyleClass("hidden");

            crumb = crumb.nextSibling;
            ++i;
        }

        // Restore the start and end crumb classes in case they got removed in coalesceCollapsedCrumbs().
        // The order of the crumbs in the document is opposite of the visual order.
        crumbs.firstChild.addStyleClass("end");
        crumbs.lastChild.addStyleClass("start");

        function crumbsAreSmallerThanContainer()
        {
            // There is some fixed extra space that is not returned in the crumbs' offsetWidth.
            // This padding is added to the crumbs' offsetWidth when comparing to the crumbsContainer.
            var rightPadding = 9;
            return ((crumbs.offsetWidth + rightPadding) < crumbsContainer.offsetWidth);
        }

        if (crumbsAreSmallerThanContainer())
            return; // No need to compact the crumbs, they all fit at full size.

        var BothSides = 0;
        var AncestorSide = -1;
        var ChildSide = 1;

        function makeCrumbsSmaller(shrinkingFunction, direction, significantCrumb)
        {
            if (!significantCrumb)
                significantCrumb = (focusedCrumb || selectedCrumb);

            if (significantCrumb === selectedCrumb)
                var significantIndex = selectedIndex;
            else if (significantCrumb === focusedCrumb)
                var significantIndex = focusedIndex;
            else {
                var significantIndex = 0;
                for (var i = 0; i < crumbs.childNodes.length; ++i) {
                    if (crumbs.childNodes[i] === significantCrumb) {
                        significantIndex = i;
                        break;
                    }
                }
            }

            function shrinkCrumbAtIndex(index)
            {
                var shrinkCrumb = crumbs.childNodes[index];
                if (shrinkCrumb && shrinkCrumb !== significantCrumb)
                    shrinkingFunction(shrinkCrumb);
                if (crumbsAreSmallerThanContainer())
                    return true; // No need to compact the crumbs more.
                return false;
            }

            // Shrink crumbs one at a time by applying the shrinkingFunction until the crumbs
            // fit in the crumbsContainer or we run out of crumbs to shrink.
            if (direction) {
                // Crumbs are shrunk on only one side (based on direction) of the signifcant crumb.
                var index = (direction > 0 ? 0 : crumbs.childNodes.length - 1);
                while (index !== significantIndex) {
                    if (shrinkCrumbAtIndex(index))
                        return true;
                    index += (direction > 0 ? 1 : -1);
                }
            } else {
                // Crumbs are shrunk in order of descending distance from the signifcant crumb,
                // with a tie going to child crumbs.
                var startIndex = 0;
                var endIndex = crumbs.childNodes.length - 1;
                while (startIndex != significantIndex || endIndex != significantIndex) {
                    var startDistance = significantIndex - startIndex;
                    var endDistance = endIndex - significantIndex;
                    if (startDistance >= endDistance)
                        var index = startIndex++;
                    else
                        var index = endIndex--;
                    if (shrinkCrumbAtIndex(index))
                        return true;
                }
            }

            // We are not small enough yet, return false so the caller knows.
            return false;
        }

        function coalesceCollapsedCrumbs()
        {
            var crumb = crumbs.firstChild;
            var collapsedRun = false;
            var newStartNeeded = false;
            var newEndNeeded = false;
            while (crumb) {
                var hidden = crumb.hasStyleClass("hidden");
                if (!hidden) {
                    var collapsed = crumb.hasStyleClass("collapsed"); 
                    if (collapsedRun && collapsed) {
                        crumb.addStyleClass("hidden");
                        crumb.removeStyleClass("compact");
                        crumb.removeStyleClass("collapsed");

                        if (crumb.hasStyleClass("start")) {
                            crumb.removeStyleClass("start");
                            newStartNeeded = true;
                        }

                        if (crumb.hasStyleClass("end")) {
                            crumb.removeStyleClass("end");
                            newEndNeeded = true;
                        }

                        continue;
                    }

                    collapsedRun = collapsed;

                    if (newEndNeeded) {
                        newEndNeeded = false;
                        crumb.addStyleClass("end");
                    }
                } else
                    collapsedRun = true;
                crumb = crumb.nextSibling;
            }

            if (newStartNeeded) {
                crumb = crumbs.lastChild;
                while (crumb) {
                    if (!crumb.hasStyleClass("hidden")) {
                        crumb.addStyleClass("start");
                        break;
                    }
                    crumb = crumb.previousSibling;
                }
            }
        }

        function compact(crumb)
        {
            if (crumb.hasStyleClass("hidden"))
                return;
            crumb.addStyleClass("compact");
        }

        function collapse(crumb, dontCoalesce)
        {
            if (crumb.hasStyleClass("hidden"))
                return;
            crumb.addStyleClass("collapsed");
            crumb.removeStyleClass("compact");
            if (!dontCoalesce)
                coalesceCollapsedCrumbs();
        }

        function compactDimmed(crumb)
        {
            if (crumb.hasStyleClass("dimmed"))
                compact(crumb);
        }

        function collapseDimmed(crumb)
        {
            if (crumb.hasStyleClass("dimmed"))
                collapse(crumb);
        }

        if (!focusedCrumb) {
            // When not focused on a crumb we can be biased and collapse less important
            // crumbs that the user might not care much about.

            // Compact child crumbs.
            if (makeCrumbsSmaller(compact, ChildSide))
                return;

            // Collapse child crumbs.
            if (makeCrumbsSmaller(collapse, ChildSide))
                return;

            // Compact dimmed ancestor crumbs.
            if (makeCrumbsSmaller(compactDimmed, AncestorSide))
                return;

            // Collapse dimmed ancestor crumbs.
            if (makeCrumbsSmaller(collapseDimmed, AncestorSide))
                return;
        }

        // Compact ancestor crumbs, or from both sides if focused.
        if (makeCrumbsSmaller(compact, (focusedCrumb ? BothSides : AncestorSide)))
            return;

        // Collapse ancestor crumbs, or from both sides if focused.
        if (makeCrumbsSmaller(collapse, (focusedCrumb ? BothSides : AncestorSide)))
            return;

        if (!selectedCrumb)
            return;

        // Compact the selected crumb.
        compact(selectedCrumb);
        if (crumbsAreSmallerThanContainer())
            return;

        // Collapse the selected crumb as a last resort. Pass true to prevent coalescing.
        collapse(selectedCrumb, true);
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

    handleCopyEvent: function(event)
    {
        if (this.currentView !== this.views.dom)
            return;

        // Don't prevent the normal copy if the user has a selection.
        if (!window.getSelection().isCollapsed)
            return;

        switch (this.focusedDOMNode.nodeType) {
            case Node.ELEMENT_NODE:
                var data = this.focusedDOMNode.outerHTML;
                break;

            case Node.COMMENT_NODE:
                var data = "<!--" + this.focusedDOMNode.nodeValue + "-->";
                break;

            default:
            case Node.TEXT_NODE:
                var data = this.focusedDOMNode.nodeValue;
        }

        event.clipboardData.clearData();
        event.preventDefault();

        if (data)
            event.clipboardData.setData("text/plain", data);
    },

    rightSidebarResizerDragStart: function(event)
    {
        if (this.sidebarDragEventListener || this.sidebarDragEndEventListener)
            return this.rightSidebarResizerDragEnd(event);

        this.sidebarDragEventListener = this.rightSidebarResizerDrag.bind(this);
        this.sidebarDragEndEventListener = this.rightSidebarResizerDragEnd.bind(this);
        WebInspector.elementDragStart(this.views.dom.sidebarElement, this.sidebarDragEventListener, this.sidebarDragEndEventListener, event, "col-resize");
    },

    rightSidebarResizerDragEnd: function(event)
    {
        WebInspector.elementDragEnd(this.views.dom.sidebarElement, this.sidebarDragEventListener, this.sidebarDragEndEventListener, event);
        delete this.sidebarDragEventListener;
        delete this.sidebarDragEndEventListener;
    },

    rightSidebarResizerDrag: function(event)
    {
        var x = event.pageX;

        var leftSidebarWidth = document.getElementById("sidebar").offsetWidth;
        var newWidth = Number.constrain(window.innerWidth - x, 100, window.innerWidth - leftSidebarWidth - 100);

        this.views.dom.sidebarElement.style.width = newWidth + "px";
        this.views.dom.sideContentElement.style.right = newWidth + "px";
        this.views.dom.sidebarResizeElement.style.right = (newWidth - 3) + "px";

        this.updateTreeSelection();
        this.updateBreadcrumbSizes();

        event.preventDefault();
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

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet, so we need to update later.
            setTimeout(this.updateSelection.bind(this), 0);
            return;
        }

        if (!this.selectionElement) {
            this.selectionElement = document.createElement("div");
            this.selectionElement.className = "selection selected";
            listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
        }

        this.selectionElement.style.height = listItemElement.offsetHeight + "px";
    },

    onattach: function()
    {
        this.listItemElement.addEventListener("mousedown", this.onmousedown.bind(this), false);
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
        this.updateSelection();
    },

    onmousedown: function(event)
    {
        // Prevent selecting the nearest word on double click.
        if (event.detail >= 2)
            event.preventDefault();
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
