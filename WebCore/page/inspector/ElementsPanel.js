/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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

WebInspector.ElementsPanel = function()
{
    WebInspector.Panel.call(this);

    this.element.addStyleClass("elements");

    this.contentElement = document.createElement("div");
    this.contentElement.id = "elements-content";
    this.contentElement.className = "outline-disclosure";

    this.treeListElement = document.createElement("ol");
    this.treeListElement.addEventListener("mousedown", this._onmousedown.bind(this), false);
    this.treeListElement.addEventListener("dblclick", this._ondblclick.bind(this), false);
    this.treeListElement.addEventListener("mousemove", this._onmousemove.bind(this), false);
    this.treeListElement.addEventListener("mouseout", this._onmouseout.bind(this), false);

    this.treeOutline = new TreeOutline(this.treeListElement);
    this.treeOutline.panel = this;

    this.contentElement.appendChild(this.treeListElement);

    this.crumbsElement = document.createElement("div");
    this.crumbsElement.className = "crumbs";

    this.sidebarPanes = {};
    this.sidebarPanes.styles = new WebInspector.StylesSidebarPane();
    this.sidebarPanes.metrics = new WebInspector.MetricsSidebarPane();
    this.sidebarPanes.properties = new WebInspector.PropertiesSidebarPane();

    this.sidebarPanes.styles.onexpand = this.updateStyles.bind(this);
    this.sidebarPanes.metrics.onexpand = this.updateMetrics.bind(this);
    this.sidebarPanes.properties.onexpand = this.updateProperties.bind(this);

    this.sidebarPanes.styles.expanded = true;

    this.sidebarElement = document.createElement("div");
    this.sidebarElement.id = "elements-sidebar";

    this.sidebarElement.appendChild(this.sidebarPanes.styles.element);
    this.sidebarElement.appendChild(this.sidebarPanes.metrics.element);
    this.sidebarElement.appendChild(this.sidebarPanes.properties.element);

    this.sidebarResizeElement = document.createElement("div");
    this.sidebarResizeElement.className = "sidebar-resizer-vertical";
    this.sidebarResizeElement.addEventListener("mousedown", this.rightSidebarResizerDragStart.bind(this), false);

    this.element.appendChild(this.contentElement);
    this.element.appendChild(this.sidebarElement);
    this.element.appendChild(this.sidebarResizeElement);

    this.reset();
}

WebInspector.ElementsPanel.prototype = {
    toolbarItemClass: "elements",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Elements");
    },

    get statusBarItems()
    {
        return [this.crumbsElement];
    },

    updateStatusBarItems: function()
    {
        this.updateBreadcrumbSizes();
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        this.sidebarResizeElement.style.right = (this.sidebarElement.offsetWidth - 3) + "px";
        this.updateBreadcrumb();
        this.updateTreeSelection();
    },

    hide: function()
    {
        WebInspector.Panel.prototype.hide.call(this);
        this.altKeyDown = false;
        this.hoveredDOMNode = null;
        this.forceHoverHighlight = false;
    },

    resize: function()
    {
        this.updateTreeSelection();
        this.updateBreadcrumbSizes();
    },

    reset: function()
    {
        this.rootDOMNode = null;
        this.focusedDOMNode = null;

        this.altKeyDown = false;
        this.hoveredDOMNode = null;
        this.forceHoverHighlight = false;

        var inspectedWindow = InspectorController.inspectedWindow();
        if (!inspectedWindow || !inspectedWindow.document)
            return;

        if (!inspectedWindow.document.firstChild) {
            function contentLoaded()
            {
                inspectedWindow.document.removeEventListener("DOMContentLoaded", contentLoadedCallback, false);

                this.reset();
            }

            var contentLoadedCallback = InspectorController.wrapCallback(contentLoaded.bind(this));

            inspectedWindow.document.addEventListener("DOMContentLoaded", contentLoadedCallback, false);
            return;
        }

        var inspectedRootDocument = inspectedWindow.document;
        this.rootDOMNode = inspectedRootDocument;

        var canidateFocusNode = inspectedRootDocument.body || inspectedRootDocument.documentElement;
        if (canidateFocusNode) {
            this.focusedDOMNode = canidateFocusNode;
            if (this.treeOutline.selectedTreeElement)
                this.treeOutline.selectedTreeElement.expand();
        }
    },

    updateTreeSelection: function()
    {
        if (!this.treeOutline || !this.treeOutline.selectedTreeElement)
            return;
        var element = this.treeOutline.selectedTreeElement;
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

        this._focusedNodeChanged();

        var nodeItem = this.revealNode(x);
        if (nodeItem)
            nodeItem.select();
    },

    get hoveredDOMNode()
    {
        return this._hoveredDOMNode;
    },

    set hoveredDOMNode(x)
    {
        if (this._hoveredDOMNode === x)
            return;

        this._hoveredDOMNode = x;

        if (this._hoveredDOMNode)
            this._updateHoverHighlightSoon();
        else
            this._updateHoverHighlight();
    },

    get forceHoverHighlight()
    {
        return this._forceHoverHighlight;
    },

    set forceHoverHighlight(x)
    {
        if (this._forceHoverHighlight === x)
            return;

        this._forceHoverHighlight = x;

        if (this._forceHoverHighlight)
            this._updateHoverHighlightSoon();
        else
            this._updateHoverHighlight();
    },

    get altKeyDown()
    {
        return this._altKeyDown;
    },

    set altKeyDown(x)
    {
        if (this._altKeyDown === x)
            return;

        this._altKeyDown = x;

        if (this._altKeyDown)
            this._updateHoverHighlightSoon();
        else
            this._updateHoverHighlight();
    },

    _updateHoverHighlightSoon: function()
    {
        if ("_updateHoverHighlightTimeout" in this)
            return;
        this._updateHoverHighlightTimeout = setTimeout(this._updateHoverHighlight.bind(this), 0);
    },

    _updateHoverHighlight: function()
    {
        if ("_updateHoverHighlightTimeout" in this) {
            clearTimeout(this._updateHoverHighlightTimeout);
            delete this._updateHoverHighlightTimeout;
        }

        if (this._hoveredDOMNode && this.visible && (this._altKeyDown || this.forceHoverHighlight))
            InspectorController.highlightDOMNode(this._hoveredDOMNode);
        else
            InspectorController.hideDOMNodeHighlight();
    },

    _focusedNodeChanged: function(forceUpdate)
    {
        this.updateBreadcrumb(forceUpdate);

        for (var pane in this.sidebarPanes)
            this.sidebarPanes[pane].needsUpdate = true;

        this.updateStyles(true);
        this.updateMetrics();
        this.updateProperties();
    },

    revealNode: function(node)
    {
        var nodeItem = this.treeOutline.findTreeElement(node, this._isAncestorIncludingParentFrames.bind(this), this._parentNodeOrFrameElement.bind(this));
        if (!nodeItem)
            return;

        nodeItem.reveal();
        return nodeItem;
    },

    updateTreeOutline: function()
    {
        this.treeOutline.removeChildrenRecursive();

        if (!this.rootDOMNode)
            return;

        // FIXME: this could use findTreeElement to reuse a tree element if it already exists
        var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(this.rootDOMNode) : this.rootDOMNode.firstChild);
        while (node) {
            this.treeOutline.appendChild(new WebInspector.DOMNodeTreeElement(node));
            node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
        }

        this.updateTreeSelection();
    },

    updateBreadcrumb: function(forceUpdate)
    {
        if (!this.visible)
            return;

        var crumbs = this.crumbsElement;

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

        if (handled && !forceUpdate) {
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
                if (crumb === panel.crumbsElement.firstChild) {
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

            WebInspector.currentFocusElement = document.getElementById("main-panels");

            event.preventDefault();
        };

        var mouseOverCrumbFunction = function(event) {
            panel.mouseOverCrumb = true;

            panel.altKeyDown = event.altKey;
            panel.hoveredDOMNode = this.representedObject;
            panel.forceHoverHighlight = this.hasStyleClass("selected");

            if ("mouseOutTimeout" in panel) {
                clearTimeout(panel.mouseOutTimeout);
                delete panel.mouseOutTimeout;
            }
        };

        var mouseOutCrumbFunction = function(event) {
            delete panel.mouseOverCrumb;

            if (event.target === this) {
                panel.altKeyDown = false;
                panel.hoveredDOMNode = null;
                panel.forceHoverHighlight = false;
            }

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
        for (var current = this.focusedDOMNode; current; current = this._parentNodeOrFrameElement(current)) {
            if (current.nodeType === Node.DOCUMENT_NODE)
                continue;

            if (current === this.rootDOMNode)
                foundRoot = true;

            var crumb = document.createElement("span");
            crumb.className = "crumb";
            crumb.representedObject = current;
            crumb.addEventListener("mousedown", selectCrumbFunction, false);
            crumb.addEventListener("mouseover", mouseOverCrumbFunction.bind(crumb), false);
            crumb.addEventListener("mouseout", mouseOutCrumbFunction.bind(crumb), false);

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

                case Node.DOCUMENT_TYPE_NODE:
                    crumbTitle = "<!DOCTYPE>";
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

            crumbs.appendChild(crumb);
        }

        if (crumbs.hasChildNodes())
            crumbs.lastChild.addStyleClass("start");

        this.updateBreadcrumbSizes();
    },

    updateBreadcrumbSizes: function(focusedCrumb)
    {
        if (!this.visible)
            return;

        if (document.body.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return;
        }

        var crumbs = this.crumbsElement;
        if (!crumbs.childNodes.length || crumbs.offsetWidth <= 0)
            return; // No crumbs, do nothing.

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
            var rightPadding = 20;
            var errorWarningElement = document.getElementById("error-warning-count");
            if (!WebInspector.console.visible && errorWarningElement)
                rightPadding += errorWarningElement.offsetWidth;
            return ((crumbs.totalOffsetLeft + crumbs.offsetWidth + rightPadding) < window.innerWidth);
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
            // fit in the container or we run out of crumbs to shrink.
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

    updateStyles: function(forceUpdate)
    {
        var stylesSidebarPane = this.sidebarPanes.styles;
        if (!stylesSidebarPane.expanded || !stylesSidebarPane.needsUpdate)
            return;

        stylesSidebarPane.update(this.focusedDOMNode, null, forceUpdate);
        stylesSidebarPane.needsUpdate = false;
    },

    updateMetrics: function()
    {
        var metricsSidebarPane = this.sidebarPanes.metrics;
        if (!metricsSidebarPane.expanded || !metricsSidebarPane.needsUpdate)
            return;

        metricsSidebarPane.update(this.focusedDOMNode);
        metricsSidebarPane.needsUpdate = false;
    },

    updateProperties: function()
    {
        var propertiesSidebarPane = this.sidebarPanes.properties;
        if (!propertiesSidebarPane.expanded || !propertiesSidebarPane.needsUpdate)
            return;

        propertiesSidebarPane.update(this.focusedDOMNode);
        propertiesSidebarPane.needsUpdate = false;
    },

    handleKeyEvent: function(event)
    {
        if (event.keyIdentifier === "Alt")
            this.altKeyDown = true;
        this.treeOutline.handleKeyEvent(event);
    },

    handleKeyUpEvent: function(event)
    {
        if (event.keyIdentifier === "Alt")
            this.altKeyDown = false;
    },

    handleCopyEvent: function(event)
    {
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
        WebInspector.elementDragStart(this.sidebarElement, this.rightSidebarResizerDrag.bind(this), this.rightSidebarResizerDragEnd.bind(this), event, "col-resize");
    },

    rightSidebarResizerDragEnd: function(event)
    {
        WebInspector.elementDragEnd(event);
    },

    rightSidebarResizerDrag: function(event)
    {
        var x = event.pageX;
        var newWidth = Number.constrain(window.innerWidth - x, Preferences.minElementsSidebarWidth, window.innerWidth * 0.66);

        this.sidebarElement.style.width = newWidth + "px";
        this.contentElement.style.right = newWidth + "px";
        this.sidebarResizeElement.style.right = (newWidth - 3) + "px";

        this.updateTreeSelection();

        event.preventDefault();
    },

    _getDocumentForNode: function(node)
    {
        return node.nodeType == Node.DOCUMENT_NODE ? node : node.ownerDocument;
    },

    _parentNodeOrFrameElement: function(node)
    {
        var parent = node.parentNode;
        if (parent)
            return parent;

        var document = this._getDocumentForNode(node);
        return document.defaultView.frameElement;
    },

    _isAncestorIncludingParentFrames: function(a, b)
    {
        for (var node = b; node; node = this._getDocumentForNode(node).defaultView.frameElement)
            if (isAncestorNode.call(a, node))
                return true;
        return false;
    },

    _treeElementFromEvent: function(event)
    {
        var outline = this.treeOutline;

        var root = this.treeListElement;

        // We choose this X coordinate based on the knowledge that our list
        // items extend nearly to the right edge of the outer <ol>.
        var x = root.totalOffsetLeft + root.offsetWidth - 20;

        var y = event.pageY;

        // Our list items have 1-pixel cracks between them vertically. We avoid
        // the cracks by checking slightly above and slightly below the mouse
        // and seeing if we hit the same element each time.
        var elementUnderMouse = outline.treeElementFromPoint(x, y);
        var elementAboveMouse = outline.treeElementFromPoint(x, y - 2);
        var element;
        if (elementUnderMouse === elementAboveMouse)
            element = elementUnderMouse;
        else
            element = outline.treeElementFromPoint(x, y + 2);

        return element;
    },

    _ondblclick: function(event)
    {
        var element = this._treeElementFromEvent(event);

        if (!element)
            return;

        element.ondblclick();
    },

    _onmousedown: function(event)
    {
        var element = this._treeElementFromEvent(event);

        if (!element || element.isEventWithinDisclosureTriangle(event))
            return;

        element.select();
    },

    _onmousemove: function(event)
    {
        var element = this._treeElementFromEvent(event);
        if (!element)
            return;

        this.altKeyDown = event.altKey;
        this.hoveredDOMNode = element.representedObject;
        this.forceHoverHighlight = element.selected;
    },

    _onmouseout: function(event)
    {
        if (event.target !== this.treeListElement)
            return;
        this.altKeyDown = false;
        this.hoveredDOMNode = null;
        this.forceHoverHighlight = false;
    }
}

WebInspector.ElementsPanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.DOMNodeTreeElement = function(node)
{
    var hasChildren = node.contentDocument || (Preferences.ignoreWhitespace ? (firstChildSkippingWhitespace.call(node) ? true : false) : node.hasChildNodes());
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
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
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

        this._makeURLsActivateOnModifiedClick();
    },

    _makeURLsActivateOnModifiedClick: function()
    {
        var links = this.listItemElement.querySelectorAll("li > .webkit-html-tag > .webkit-html-attribute > .webkit-html-external-link, li > .webkit-html-tag > .webkit-html-attribute > .webkit-html-resource-link");
        if (!links)
            return;

        var isMac = InspectorController.platform().indexOf("mac") == 0;

        for (var i = 0; i < links.length; ++i) {
            var link = links[i];
            var isExternal = link.hasStyleClass("webkit-html-external-link");
            var href = link.getAttribute("href");
            var title;
            if (isMac) {
                if (isExternal)
                    title = WebInspector.UIString("Option-click to visit %s.", href);
                else
                    title = WebInspector.UIString("Option-click to show %s.", href);
            } else {
                if (isExternal)
                    title = WebInspector.UIString("Alt-click to visit %s.", href);
                else
                    title = WebInspector.UIString("Alt-click to show %s.", href);
            }
            link.setAttribute("title", title);
            link.followOnAltClick = true;
        }
    },

    onpopulate: function()
    {
        if (this.children.length || this.whitespaceIgnored !== Preferences.ignoreWhitespace)
            return;

        this.removeChildren();
        this.whitespaceIgnored = Preferences.ignoreWhitespace;

        var treeElement = this;
        function appendChildrenOfNode(node)
        {
            var child = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(node) : node.firstChild);
            while (child) {
                treeElement.appendChild(new WebInspector.DOMNodeTreeElement(child));
                child = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(child) : child.nextSibling;
            }
        }

        if (this.representedObject.contentDocument)
            appendChildrenOfNode(this.representedObject.contentDocument);

        appendChildrenOfNode(this.representedObject);

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
        if (this.listItemElement)
            this.listItemElement.scrollIntoViewIfNeeded(false);
    },

    onselect: function()
    {
        this._selectedByCurrentMouseDown = true;
        this.treeOutline.panel.focusedDOMNode = this.representedObject;
        this.updateSelection();
    },

    onmousedown: function(event)
    {
        if (this._editing)
            return;

        if (this._selectedByCurrentMouseDown)
            delete this._selectedByCurrentMouseDown;
        else if (this._startEditing(event)) {
            event.preventDefault();
            return;
        }

        // Prevent selecting the nearest word on double click.
        if (event.detail >= 2)
            event.preventDefault();
    },

    ondblclick: function(treeElement, event)
    {
        if (this._editing)
            return;

        var panel = this.treeOutline.panel;
        panel.rootDOMNode = this.parent.representedObject;
        panel.focusedDOMNode = this.representedObject;

        if (this.hasChildren && !this.expanded)
            this.expand();
    },

    _startEditing: function(event)
    {
        if (this.treeOutline.panel.focusedDOMNode != this.representedObject)
            return;

        if (this.representedObject.nodeType != Node.ELEMENT_NODE && this.representedObject.nodeType != Node.TEXT_NODE)
            return false;

        var textNode = event.target.enclosingNodeOrSelfWithClass("webkit-html-text-node");
        if (textNode)
            return this._startEditingTextNode(textNode);

        var attribute = event.target.enclosingNodeOrSelfWithClass("webkit-html-attribute");
        if (attribute)
            return this._startEditingAttribute(attribute, event);

        return false;
    },

    _startEditingAttribute: function(attribute, event)
    {
        if (WebInspector.isBeingEdited(attribute))
            return true;

        var attributeNameElement = attribute.getElementsByClassName("webkit-html-attribute-name")[0];
        if (!attributeNameElement)
            return false;

        var isURL = event.target.enclosingNodeOrSelfWithClass("webkit-html-external-link") || event.target.enclosingNodeOrSelfWithClass("webkit-html-resource-link");
        if (isURL && event.altKey)
            return false;

        var attributeName = attributeNameElement.innerText;

        function removeZeroWidthSpaceRecursive(node)
        {
            if (node.nodeType === Node.TEXT_NODE) {
                node.nodeValue = node.nodeValue.replace(/\u200B/g, "");
                return;
            }

            if (node.nodeType !== Node.ELEMENT_NODE)
                return;

            for (var child = node.firstChild; child; child = child.nextSibling)
                removeZeroWidthSpaceRecursive(child);
        }

        // Remove zero-width spaces that were added by nodeTitleInfo.
        removeZeroWidthSpaceRecursive(attribute);

        this._editing = true;

        WebInspector.startEditing(attribute, this._attributeEditingCommitted.bind(this), this._editingCancelled.bind(this), attributeName);
        window.getSelection().setBaseAndExtent(event.target, 0, event.target, 1);

        return true;
    },

    _startEditingTextNode: function(textNode)
    {
        if (WebInspector.isBeingEdited(textNode))
            return true;

        this._editing = true;

        WebInspector.startEditing(textNode, this._textNodeEditingCommitted.bind(this), this._editingCancelled.bind(this));
        window.getSelection().setBaseAndExtent(textNode, 0, textNode, 1);

        return true;
    },

    _attributeEditingCommitted: function(element, newText, oldText, attributeName)
    {
        delete this._editing;

        var parseContainerElement = document.createElement("span");
        parseContainerElement.innerHTML = "<span " + newText + "></span>";
        var parseElement = parseContainerElement.firstChild;
        if (!parseElement || !parseElement.hasAttributes()) {
            editingCancelled(element, context);
            return;
        }

        var foundOriginalAttribute = false;
        for (var i = 0; i < parseElement.attributes.length; ++i) {
            var attr = parseElement.attributes[i];
            foundOriginalAttribute = foundOriginalAttribute || attr.name === attributeName;
            InspectorController.inspectedWindow().Element.prototype.setAttribute.call(this.representedObject, attr.name, attr.value);
        }

        if (!foundOriginalAttribute)
            InspectorController.inspectedWindow().Element.prototype.removeAttribute.call(this.representedObject, attributeName);

        this._updateTitle();
        this.treeOutline.panel._focusedNodeChanged(true);
    },

    _textNodeEditingCommitted: function(element, newText)
    {
        delete this._editing;

        var textNode;
        if (this.representedObject.nodeType == Node.ELEMENT_NODE) {
            // We only show text nodes inline in elements if the element only
            // has a single child, and that child is a text node.
            textNode = this.representedObject.firstChild;
        } else if (this.representedObject.nodeType == Node.TEXT_NODE)
            textNode = this.representedObject;

        textNode.nodeValue = newText;
        this._updateTitle();
    },


    _editingCancelled: function(element, context)
    {
        delete this._editing;

        this._updateTitle();
    },

    _updateTitle: function()
    {
        this.title = nodeTitleInfo.call(this.representedObject, this.hasChildren, WebInspector.linkifyURL).title;
        delete this.selectionElement;
        this.updateSelection();
        this._makeURLsActivateOnModifiedClick();
    },
}

WebInspector.DOMNodeTreeElement.prototype.__proto__ = TreeElement.prototype;
