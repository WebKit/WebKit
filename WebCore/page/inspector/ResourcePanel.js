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

WebInspector.ResourcePanel = function(resource)
{
    this.resource = resource;
    this.messages = [];

    WebInspector.Panel.call(this);
}

WebInspector.ResourcePanel.prototype = {
    show: function()
    {
        this.resource.listItem.select(true);
        this.resource.listItem.reveal();

        if (this.currentView && "onshow" in this.currentView)
            this.currentView.onshow();

        WebInspector.Panel.prototype.show.call(this);
    },

    hide: function()
    {
        this.resource.listItem.deselect(true);
        if (this.currentView && "onhide" in this.currentView)
            this.currentView.onhide();
        WebInspector.Panel.prototype.hide.call(this);
    },

    sourceRow: function(lineNumber)
    {
        this.refreshIfNeeded();
        var doc = this.sourceFrame.contentDocument;
        var rows = doc.getElementsByTagName("table")[0].rows;

        // Line numbers are a 1-based index, but the rows collection is 0-based.
        --lineNumber;
        if (lineNumber >= rows.length)
            lineNumber = rows.length - 1;

        return rows[lineNumber];
    },

    showSourceLine: function(lineNumber)
    {
        var row = this.sourceRow(lineNumber);
        if (!row)
            return;
        this.navigateToView("source");
        row.scrollIntoView(true);
    },

    addMessageToSource: function(msg)
    {
        this.messages.push(msg);
        if (this.sourceFrame)
            this._addMessageToSource(msg);
    },

    _addMessageToSource: function(msg)
    {
        var row = this.sourceRow(msg.line);
        if (!row)
            return;
        var cell = row.getElementsByTagName("td")[1];

        var errorDiv = cell.lastChild;
        if (!errorDiv || errorDiv.nodeName.toLowerCase() !== "div" || !errorDiv.hasStyleClass("webkit-html-message-bubble")) {
            errorDiv = document.createElement("div");
            errorDiv.className = "webkit-html-message-bubble";
            cell.appendChild(errorDiv);
        }
        var imageURL;
        switch (msg.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                errorDiv.addStyleClass("webkit-html-error-message");
                imageURL = "Images/errorIcon.png";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                errorDiv.addStyleClass("webkit-html-warning-message");
                imageURL = "Images/warningIcon.png";
                break;
        }

        var lineDiv = document.createElement("div");
        lineDiv.className = "webkit-html-message-line";
        errorDiv.appendChild(lineDiv);

        var image = document.createElement("img");
        image.src = imageURL;
        image.className = "webkit-html-message-icon";
        lineDiv.appendChild(image);

        lineDiv.appendChild(document.createTextNode(msg.message));
    },

    _addMessagesToSource: function()
    {
        for (var i = 0; i < this.messages.length; ++i)
            this._addMessageToSource(this.messages[i]);
    },

    navigateToView: function(view)
    {
        if (typeof view === "string" || view instanceof String) {
            if (this.needsRefresh)
                this.refresh();
            view = this.views[view];
        }

        WebInspector.navigateToView(view);
    },

    createViews: function()
    {
        this.currentView = null;

        var selectViewFunction = function(event)
        {
            WebInspector.navigateToView(event.currentTarget.view);
        }

        var views = [
            {name: "source", title: "Source", selected: true},
        ];

        if (this.resource.category === WebInspector.resourceCategories.documents) {
            var domView = {name: "dom", title: "DOM", selected: false};
            var panel = this;
            domView.onshow = function() { InspectorController.highlightDOMNode(panel.focusedDOMNode) };
            domView.onhide = function() { InspectorController.hideDOMNodeHighlight() };
            views.push(domView);
        }

        var oldViews = this.views || {};
        this.views = {};
        this.viewButtons = [];
        for (var i = 0; i < views.length; ++i) {
            var buttonElement = document.createElement("button");
            buttonElement.setAttribute("title", views[i].title);
            buttonElement.addEventListener("click", selectViewFunction, "true");
            buttonElement.appendChild(document.createElement("img"));

            var contentElement = document.createElement("div");
            contentElement.className = "content " + views[i].name;

            // We need to reuse the old view object if possible because it might
            // already be in the backForwardList
            var view = oldViews[views[i].name] || {};

            view.buttonElement = buttonElement;
            view.contentElement = contentElement;
            view.panel = this;
            view.buttonElement.view = view;
            if ("onshow" in views[i])
                view.onshow = views[i].onshow;
            if ("onhide" in views[i])
                view.onhide = views[i].onhide;

            this.views[views[i].name] = view;
            this.viewButtons.push(buttonElement);
            this.element.appendChild(contentElement);
        }

        this.currentView = this.views.source;

        if (WebInspector.currentPanel == this)
            WebInspector.updateViewButtons();
    },

    _createSourceFrame: function()
    {
        this.sourceFrame = document.createElement("iframe");
        this.sourceFrame.setAttribute("viewsource", "true");
        this.sourceFrame.name = "sourceFrame" + WebInspector.ResourcePanel.sourceFrameCounter++;
        this.views.source.contentElement.appendChild(this.sourceFrame);
        InspectorController.addSourceToFrame(this.resource.identifier, this.sourceFrame);
        WebInspector.addMainEventListeners(this.sourceFrame.contentDocument);
        this._addMessagesToSource();
    },

    updateTreeSelection: function()
    {
        if (!this.domTreeOutline || !this.domTreeOutline.selectedTreeElement)
            return;
        var element = this.domTreeOutline.selectedTreeElement;
        element.updateSelection(element);
    },

    refresh: function()
    {
        this.needsRefresh = false;

        this.element.removeChildren();

        this.createViews();

        if (!this.resource.finished)
            this.views.source.contentElement.addStyleClass("loading");
        else if (this.resource.failed)
            this.views.source.contentElement.removeStyleClass("failed");
        else if (this.resource.category === WebInspector.resourceCategories.documents) {
            this._createSourceFrame();

            this.views.dom.contentElement.sideContentElement = document.createElement("div");
            this.views.dom.contentElement.sideContentElement.className = "content side";

            this.views.dom.contentElement.treeContentElement = document.createElement("div");
            this.views.dom.contentElement.treeContentElement.className = "content tree outline-disclosure";

            this.views.dom.contentElement.treeListElement = document.createElement("ol");
            this.domTreeOutline = new TreeOutline(this.views.dom.contentElement.treeListElement);
            this.domTreeOutline.panel = this;

            var panel = this;
            window.addEventListener("resize", function() { panel.updateTreeSelection() }, false);

            this.views.dom.contentElement.crumbsElement = document.createElement("div");
            this.views.dom.contentElement.crumbsElement.className = "crumbs";

            this.views.dom.contentElement.innerCrumbsElement = document.createElement("div");
            this.views.dom.contentElement.crumbsElement.appendChild(this.views.dom.contentElement.innerCrumbsElement);

            this.views.dom.contentElement.sidebarPanes = {};
            this.views.dom.contentElement.sidebarPanes.styles = new WebInspector.SidebarPane("Styles");
            this.views.dom.contentElement.sidebarPanes.metrics = new WebInspector.SidebarPane("Metrics");
            this.views.dom.contentElement.sidebarPanes.properties = new WebInspector.SidebarPane("Properties");

            var panel = this;
            this.views.dom.contentElement.sidebarPanes.styles.onexpand = function() { panel.updateStyles() };
            this.views.dom.contentElement.sidebarPanes.metrics.onexpand = function() { panel.updateMetrics() };
            this.views.dom.contentElement.sidebarPanes.properties.onexpand = function() { panel.updateProperties() };

            this.views.dom.contentElement.sidebarPanes.styles.expanded = true;

            this.views.dom.contentElement.sidebarElement = document.createElement("div");
            this.views.dom.contentElement.sidebarElement.className = "sidebar";

            this.views.dom.contentElement.sidebarElement.appendChild(this.views.dom.contentElement.sidebarPanes.styles.element);
            this.views.dom.contentElement.sidebarElement.appendChild(this.views.dom.contentElement.sidebarPanes.metrics.element);
            this.views.dom.contentElement.sidebarElement.appendChild(this.views.dom.contentElement.sidebarPanes.properties.element);

            this.views.dom.contentElement.sideContentElement.appendChild(this.views.dom.contentElement.treeContentElement);
            this.views.dom.contentElement.sideContentElement.appendChild(this.views.dom.contentElement.crumbsElement);
            this.views.dom.contentElement.treeContentElement.appendChild(this.views.dom.contentElement.treeListElement);

            this.views.dom.contentElement.appendChild(this.views.dom.contentElement.sideContentElement);
            this.views.dom.contentElement.appendChild(this.views.dom.contentElement.sidebarElement);

            this.rootDOMNode = this.resource.documentNode;
        } else if (this.resource.category === WebInspector.resourceCategories.images) {
            this.views.source.contentElement.removeStyleClass("source");
            this.views.source.contentElement.addStyleClass("image");

            var container = document.createElement("div");
            container.className = "image";
            this.views.source.contentElement.appendChild(container);

            this.imagePreviewElement = document.createElement("img");
            this.imagePreviewElement.setAttribute("src", this.resource.url);

            container.appendChild(this.imagePreviewElement);

            container = document.createElement("div");
            container.className = "info";
            this.views.source.contentElement.appendChild(container);

            var imageNameElement = document.createElement("h1");
            imageNameElement.className = "title";
            imageNameElement.textContent = this.resource.displayName;
            container.appendChild(imageNameElement);

            var infoListElement = document.createElement("dl");
            infoListElement.className = "infoList";

            var imageProperties = [
                {name: "Dimensions", value: this.imagePreviewElement.naturalWidth + " \u00D7 " + this.imagePreviewElement.height},
                {name: "File size", value: (this.resource.contentLength / 1024).toPrecision(2) + "KB"},
                {name: "MIME type", value: this.resource.mimeType}
            ];

            var listHTML = '';
            for (var i = 0; i < imageProperties.length; ++i)
                listHTML += "<dt>" + imageProperties[i].name + "</dt><dd>" + imageProperties[i].value + "</dd>";

            infoListElement.innerHTML = listHTML;
            container.appendChild(infoListElement);
        } else if (this.resource.category === WebInspector.resourceCategories.stylesheets
                   || this.resource.category === WebInspector.resourceCategories.scripts) {
                this.views.source.contentElement.addStyleClass(this.resource.category.title.toLowerCase());
            this._createSourceFrame();
        } else {
            if (this.resource.category)
                this.views.source.contentElement.addStyleClass(this.resource.category.title.toLowerCase());
        }
    },

    get currentView()
    {
        return this._currentView;
    },

    set currentView(x)
    {
        if (this._currentView === x)
            return;

        if (this._currentView) {
            this._currentView.buttonElement.removeStyleClass("selected");
            this._currentView.contentElement.removeStyleClass("selected");
            if ("onhide" in this._currentView)
                this._currentView.onhide();
        }

        this._currentView = x;

        if (x) {
            x.buttonElement.addStyleClass("selected");
            x.contentElement.addStyleClass("selected");
            if ("onshow" in x)
                x.onshow();

            if (x.contentElement.parentNode !== this.element)
                InspectorController.log("Tried to set currentView to a view " + x.title + " whose contentElement is a non-child");
        }
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

        for (var pane in this.views.dom.contentElement.sidebarPanes)
            this.views.dom.contentElement.sidebarPanes[pane].needsUpdate = true;

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
        var nodeItem = this.domTreeOutline.findTreeElement(node, function(a, b) { return isAncestorNode.call(a, b); }, function(a) { return a.parentNode; });
        if (!nodeItem)
            return;

        nodeItem.reveal();
        return nodeItem;
    },

    updateTreeOutline: function()
    {
        this.domTreeOutline.removeChildrenRecursive();

        if (!this.rootDOMNode)
            return;

        // FIXME: this could use findTreeElement to reuse a tree element if it already exists
        var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(this.rootDOMNode) : this.rootDOMNode.firstChild);
        while (node) {
            var item = new WebInspector.DOMNodeTreeElement(node);
            this.domTreeOutline.appendChild(item);
            node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
        }

        this.updateTreeSelection();
    },

    updateBreadcrumb: function()
    {
        if (!this.views || !this.views.dom.contentElement)
            return;
        var crumbs = this.views.dom.contentElement.innerCrumbsElement;
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
        if (!this.views.dom.contentElement.sidebarPanes.styles.expanded)
            return;
        if (!this.views.dom.contentElement.sidebarPanes.styles.needsUpdate)
            return;

        this.views.dom.contentElement.sidebarPanes.styles.needsUpdate = false;

        var stylesBody = this.views.dom.contentElement.sidebarPanes.styles.bodyElement;

        stylesBody.removeChildren();
        this.views.dom.contentElement.sidebarPanes.styles.sections = [];

        if (!this.focusedDOMNode)
            return;

        var styleNode = this.focusedDOMNode;
        if (styleNode.nodeType === Node.TEXT_NODE && styleNode.parentNode)
            styleNode = styleNode.parentNode;

        var styleRules = [];
        var styleProperties = [];

        if (styleNode.nodeType === Node.ELEMENT_NODE) {
            var propertyCount = [];

            var computedStyle = styleNode.ownerDocument.defaultView.getComputedStyle(styleNode);
            if (computedStyle && computedStyle.length)
                styleRules.push({ isComputedStyle: true, selectorText: "Computed Style", style: computedStyle });

            var styleNodeName = styleNode.nodeName.toLowerCase();
            for (var i = 0; i < styleNode.attributes.length; ++i) {
                var attr = styleNode.attributes[i];
                if (attr.style) {
                    var attrStyle = { attrName: attr.name, style: attr.style };
                    attrStyle.subtitle = "element\u2019s \u201C" + attr.name + "\u201D attribute";
                    attrStyle.selectorText = styleNodeName + "[" + attr.name;
                    if (attr.value.length)
                        attrStyle.selectorText += "=" + attr.value;
                    attrStyle.selectorText += "]";
                    styleRules.push(attrStyle);
                }
            }

            if (styleNode.style && styleNode.style.length) {
                var inlineStyle = { selectorText: "Inline Style Attribute", style: styleNode.style };
                inlineStyle.subtitle = "element\u2019s \u201Cstyle\u201D attribute";
                styleRules.push(inlineStyle);
            }

            var matchedStyleRules = styleNode.ownerDocument.defaultView.getMatchedCSSRules(styleNode, "", !Preferences.showUserAgentStyles);
            if (matchedStyleRules) {
                for (var i = matchedStyleRules.length - 1; i >= 0; --i)
                    styleRules.push(matchedStyleRules[i]);
            }

            var priorityUsed = false;
            var usedProperties = {};
            var shorthandProperties = {};
            for (var i = 0; i < styleRules.length; ++i) {
                styleProperties[i] = [];

                var style = styleRules[i].style;
                var styleShorthandLookup = [];
                for (var j = 0; j < style.length; ++j) {
                    var prop = null;
                    var name = style[j];
                    var shorthand = style.getPropertyShorthand(name);
                    if (!shorthand && styleRules[i].isComputedStyle)
                        shorthand = shorthandProperties[name];

                    if (shorthand) {
                        prop = styleShorthandLookup[shorthand];
                        shorthandProperties[name] = shorthand;
                    }

                    if (!priorityUsed && style.getPropertyPriority(name).length)
                        priorityUsed = true;

                    if (prop) {
                        prop.subProperties.push(name);
                    } else {
                        prop = { style: style, subProperties: [name], unusedProperties: [], name: (shorthand ? shorthand : name) };
                        styleProperties[i].push(prop);

                        if (shorthand) {
                            styleShorthandLookup[shorthand] = prop;
                            if (!styleRules[i].isComputedStyle) {
                                if (!propertyCount[shorthand]) {
                                    propertyCount[shorthand] = 1;
                                } else {
                                    prop.unusedProperties[shorthand] = true;
                                    propertyCount[shorthand]++;
                                }
                            }
                        }
                    }

                    if (styleRules[i].isComputedStyle)
                        continue;

                    usedProperties[name] = true;
                    if (shorthand)
                        usedProperties[shorthand] = true;

                    if (!propertyCount[name]) {
                        propertyCount[name] = 1;
                    } else {
                        prop.unusedProperties[name] = true;
                        propertyCount[name]++;
                    }
                }
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

            var styleRulesLength = styleRules.length;
            for (var i = 0; i < styleRulesLength; ++i) {
                var selectorText = styleRules[i].selectorText;

                var section = new WebInspector.PropertiesSection(selectorText);
                section.expanded = true;

                if (styleRules[i].isComputedStyle) {
                    if (Preferences.showInheritedComputedStyleProperties)
                        section.element.addStyleClass("show-inherited");

                    var showInheritedLabel = document.createElement("label");
                    var showInheritedInput = document.createElement("input");
                    showInheritedInput.type = "checkbox";
                    showInheritedInput.checked = Preferences.showInheritedComputedStyleProperties;

                    var computedStyleSection = section;
                    var showInheritedToggleFunction = function(event) {
                        Preferences.showInheritedComputedStyleProperties = showInheritedInput.checked;
                        if (Preferences.showInheritedComputedStyleProperties)
                            computedStyleSection.element.addStyleClass("show-inherited");
                        else
                            computedStyleSection.element.removeStyleClass("show-inherited");
                        event.stopPropagation();
                    };

                    showInheritedLabel.addEventListener("click", showInheritedToggleFunction, false);

                    showInheritedLabel.appendChild(showInheritedInput);
                    showInheritedLabel.appendChild(document.createTextNode("Show implicit properties"));
                    section.subtitleElement.appendChild(showInheritedLabel);
                } else {
                    var sheet;
                    var file = false;
                    if (styleRules[i].subtitle)
                        sheet = styleRules[i].subtitle;
                    else if (styleRules[i].parentStyleSheet && styleRules[i].parentStyleSheet.href) {
                        var url = styleRules[i].parentStyleSheet.href;
                        sheet = WebInspector.linkifyURL(url, url.trimURL(WebInspector.mainResource.domain).escapeHTML());
                        file = true;
                    } else if (styleRules[i].parentStyleSheet && !styleRules[i].parentStyleSheet.ownerNode)
                        sheet = "user agent stylesheet";
                    else
                        sheet = "inline stylesheet";

                    if (file)
                        section.subtitleElement.addStyleClass("file");

                    section.subtitle = sheet;
                }

                stylesBody.appendChild(section.element);
                this.views.dom.contentElement.sidebarPanes.styles.sections.push(section);

                var properties = styleProperties[i];
                var isComputedStyle = styleRules[i].isComputedStyle;

                for (var j = 0; j < properties.length; ++j) {
                    var prop = properties[j];

                    var propTreeElement = new WebInspector.StylePropertyTreeElement(prop, isComputedStyle, usedProperties);
                    section.propertiesTreeOutline.appendChild(propTreeElement);
                }
            }
        } else {
            // can't style this node
        }
    },

    updateMetrics: function()
    {
        if (!this.views.dom.contentElement.sidebarPanes.metrics.expanded)
            return;
        if (!this.views.dom.contentElement.sidebarPanes.metrics.needsUpdate)
            return;

        this.views.dom.contentElement.sidebarPanes.metrics.needsUpdate = false;

        var metricsBody = this.views.dom.contentElement.sidebarPanes.metrics.bodyElement;

        metricsBody.removeChildren();

        if (!this.focusedDOMNode)
            return;

        var style;
        if (this.focusedDOMNode.nodeType === Node.ELEMENT_NODE)
            style = this.focusedDOMNode.ownerDocument.defaultView.getComputedStyle(this.focusedDOMNode);
        if (!style)
            return;

        var metricsElement = document.createElement("div");
        metricsElement.className = "metrics";

        function boxPartValue(style, name, suffix)
        {
            var value = style.getPropertyValue(name + suffix);
            if (value === "" || value === "0px")
                value = "\u2012";
            return value.replace(/px$/, "");
        }

        // Display types for which margin is ignored.
        var noMarginDisplayType = {
            "table-cell": true,
            "table-column": true,
            "table-column-group": true,
            "table-footer-group": true,
            "table-header-group": true,
            "table-row": true,
            "table-row-group": true
        };

        // Display types for which padding is ignored.
        var noPaddingDisplayType = {
            "table-column": true,
            "table-column-group": true,
            "table-footer-group": true,
            "table-header-group": true,
            "table-row": true,
            "table-row-group": true
        };

        var boxes = ["content", "padding", "border", "margin"];
        var previousBox;
        for (var i = 0; i < boxes.length; ++i) {
            var name = boxes[i];

            if (name === "margin" && noMarginDisplayType[style.display])
                continue;
            if (name === "padding" && noPaddingDisplayType[style.display])
                continue;

            var boxElement = document.createElement("div");
            boxElement.className = name;

            if (boxes[i] === "content") {
                var width = style.width.replace(/px$/, "");
                var height = style.height.replace(/px$/, "");
                boxElement.textContent = width + " \u00D7 " + height;
            } else {
                var suffix = boxes[i] === "border" ? "-width" : "";

                var labelElement = document.createElement("div");
                labelElement.className = "label";
                labelElement.textContent = name;
                boxElement.appendChild(labelElement);

                var topElement = document.createElement("div");
                topElement.className = "top";
                topElement.textContent = boxPartValue(style, name + "-top", suffix);
                boxElement.appendChild(topElement);

                var leftElement = document.createElement("div");
                leftElement.className = "left";
                leftElement.textContent = boxPartValue(style, name + "-left", suffix);
                boxElement.appendChild(leftElement);

                if (previousBox)
                    boxElement.appendChild(previousBox);

                var rightElement = document.createElement("div");
                rightElement.className = "right";
                rightElement.textContent = boxPartValue(style, name + "-right", suffix);
                boxElement.appendChild(rightElement);

                var bottomElement = document.createElement("div");
                bottomElement.className = "bottom";
                bottomElement.textContent = boxPartValue(style, name + "-bottom", suffix);
                boxElement.appendChild(bottomElement);
            }

            previousBox = boxElement;
        }

        metricsElement.appendChild(previousBox);
        metricsBody.appendChild(metricsElement);
    },

    updateProperties: function()
    {
        if (!this.views.dom.contentElement.sidebarPanes.properties.expanded)
            return;
        if (!this.views.dom.contentElement.sidebarPanes.properties.needsUpdate)
            return;

        this.views.dom.contentElement.sidebarPanes.properties.needsUpdate = false;

        var propertiesBody = this.views.dom.contentElement.sidebarPanes.properties.bodyElement;

        propertiesBody.removeChildren();
        this.views.dom.contentElement.sidebarPanes.properties.sections = [];

        if (!this.focusedDOMNode)
            return;

        for (var prototype = this.focusedDOMNode; prototype; prototype = prototype.__proto__) {
            var hasChildren = false;
            for (var prop in prototype) {
                if (prop === "__treeElementIdentifier")
                    continue;
                if (!prototype.hasOwnProperty(prop))
                    continue;

                hasChildren = true;
                break;
            }

            if (!hasChildren)
                continue;

            var title = Object.describe(prototype);
            var subtitle;
            if (title.match(/Prototype$/)) {
                title = title.replace(/Prototype$/, "");
                subtitle = "Prototype";
            }
            var section = new WebInspector.PropertiesSection(title, subtitle);
            section.onpopulate = WebInspector.DOMPropertiesSection.onpopulate(prototype);

            propertiesBody.appendChild(section.element);
            this.views.dom.contentElement.sidebarPanes.properties.sections.push(section);
        }
    },

    handleKeyEvent: function(event)
    {
        if (this.domTreeOutline && this.currentView && this.currentView === this.views.dom)
            this.domTreeOutline.handleKeyEvent(event);
    }
}

WebInspector.ResourcePanel.sourceFrameCounter = 0;

WebInspector.ResourcePanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.SidebarPane = function(title)
{
    this.element = document.createElement("div");
    this.element.className = "pane";

    this.titleElement = document.createElement("div");
    this.titleElement.className = "title";

    var pane = this;
    this.titleElement.addEventListener("click", function() { pane.expanded = !pane.expanded; }, false);

    this.bodyElement = document.createElement("div");
    this.bodyElement.className = "body";

    this.element.appendChild(this.titleElement);
    this.element.appendChild(this.bodyElement);

    this.title = title;
    this.growbarVisible = false;
    this.expanded = false;
    this.onexpand = null;
    this.oncollapse = null;
}

WebInspector.SidebarPane.prototype = {
    get title()
    {
        return this._title;
    },

    set title(x)
    {
        if (this._title === x)
            return;
        this._title = x;
        this.titleElement.textContent = x;
    },

    get growbarVisible()
    {
        return this._growbarVisible;
    },

    set growbarVisible(x)
    {
        if (this._growbarVisible === x)
            return;

        this._growbarVisible = x;

        if (x && !this._growbarElement) {
            this._growbarElement = document.createElement("div");
            this._growbarElement.className = "growbar";
            this.element.appendChild(this._growbarElement);
        } else if (!x && this._growbarElement) {
            if (this._growbarElement.parentNode)
                this._growbarElement.parentNode(this._growbarElement);
            delete this._growbarElement;
        }
    },

    get expanded()
    {
        return this._expanded;
    },

    set expanded(x)
    {
        if (x)
            this.expand();
        else
            this.collapse();
    },

    expand: function()
    {
        if (this._expanded)
            return;
        this._expanded = true;
        this.element.addStyleClass("expanded");
        if (this.onexpand)
            this.onexpand(this);
    },

    collapse: function()
    {
        if (!this._expanded)
            return;
        this._expanded = false;
        this.element.removeStyleClass("expanded");
        if (this.oncollapse)
            this.oncollapse(this);
    }
}

WebInspector.PropertiesSection = function(title, subtitle)
{
    this.element = document.createElement("div");
    this.element.className = "section";

    this.headerElement = document.createElement("div");
    this.headerElement.className = "header";

    this.titleElement = document.createElement("div");
    this.titleElement.className = "title";

    this.subtitleElement = document.createElement("div");
    this.subtitleElement.className = "subtitle";

    this.headerElement.appendChild(this.titleElement);
    this.headerElement.appendChild(this.subtitleElement);

    var section = this;
    this.headerElement.addEventListener("click", function() { section.expanded = !section.expanded; }, false);

    this.propertiesElement = document.createElement("ol");
    this.propertiesElement.className = "properties";
    this.propertiesTreeOutline = new TreeOutline(this.propertiesElement);

    this.element.appendChild(this.headerElement);
    this.element.appendChild(this.propertiesElement);

    this.title = title;
    this.subtitle = subtitle;
    this.expanded = false;
}

WebInspector.PropertiesSection.prototype = {
    get title()
    {
        return this._title;
    },

    set title(x)
    {
        if (this._title === x)
            return;
        this._title = x;
        this.titleElement.textContent = x;
    },

    get subtitle()
    {
        return this._subtitle;
    },

    set subtitle(x)
    {
        if (this._subtitle === x)
            return;
        this._subtitle = x;
        this.subtitleElement.innerHTML = x;
    },

    get expanded()
    {
        return this._expanded;
    },

    set expanded(x)
    {
        if (x)
            this.expand();
        else
            this.collapse();
    },

    expand: function()
    {
        if (this._expanded)
            return;
        this._expanded = true;

        if (!this._populated && this.onpopulate) {
            this.onpopulate(this);
            this._populated = true;
        }
        this.element.addStyleClass("expanded");
    },

    collapse: function()
    {
        if (!this._expanded)
            return;
        this._expanded = false;
        this.element.removeStyleClass("expanded");
    }
}

WebInspector.DOMPropertiesSection = function()
{
    // FIXME: Perhaps this should be a real subclass someday
}

WebInspector.DOMPropertiesSection.onpopulate = function(prototype)
{
    return function(section) {
        var properties = Object.sortedProperties(prototype);
        for (var i = 0; i < properties.length; ++i) {
            var name = properties[i];
            if (!prototype.hasOwnProperty(name))
                continue;
            if (name === "__treeElementIdentifier")
                continue;
            var item = new WebInspector.DOMPropertyTreeElement(name, prototype);
            section.propertiesTreeOutline.appendChild(item);
        }
    }
}

WebInspector.DOMNodeTreeElement = function(node)
{
    var hasChildren = (Preferences.ignoreWhitespace ? (firstChildSkippingWhitespace.call(node) ? true : false) : node.hasChildNodes());

    var titleInfo = nodeTitleInfo.call(node, hasChildren, WebInspector.linkifyURL);
    var title = titleInfo.title;
    hasChildren = titleInfo.hasChildren;

    var item = new TreeElement(title, node, hasChildren);
    item.updateSelection = WebInspector.DOMNodeTreeElement.updateSelection;
    item.onpopulate = WebInspector.DOMNodeTreeElement.populate;
    item.onexpand = WebInspector.DOMNodeTreeElement.expanded;
    item.oncollapse = WebInspector.DOMNodeTreeElement.collapsed;
    item.onselect = WebInspector.DOMNodeTreeElement.selected;
    item.onreveal = WebInspector.DOMNodeTreeElement.revealed;
    item.ondblclick = WebInspector.DOMNodeTreeElement.doubleClicked;
    if (hasChildren) 
        item.whitespaceIgnored = Preferences.ignoreWhitespace;
    return item;
}

WebInspector.DOMNodeTreeElement.updateSelection = function(element)
{
    if (!element || !element._listItemNode)
        return;

    if (!element.selectionElement) {
        element.selectionElement = document.createElement("div");
        element.selectionElement.className = "selection selected";
        element._listItemNode.insertBefore(element.selectionElement, element._listItemNode.firstChild);
    }

    element.selectionElement.style.height = element._listItemNode.offsetHeight + "px";
}

WebInspector.DOMNodeTreeElement.populate = function(element)
{
    if (element.children.length || element.whitespaceIgnored !== Preferences.ignoreWhitespace)
        return;

    element.removeChildren();
    element.whitespaceIgnored = Preferences.ignoreWhitespace;

    var node = (Preferences.ignoreWhitespace ? firstChildSkippingWhitespace.call(element.representedObject) : element.representedObject.firstChild);
    while (node) {
        var item = new WebInspector.DOMNodeTreeElement(node);
        element.appendChild(item);
        node = Preferences.ignoreWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
    }

    if (element.representedObject.nodeType == Node.ELEMENT_NODE) {
        var title = "<span class=\"webkit-html-tag close\">&lt;/" + element.representedObject.nodeName.toLowerCase().escapeHTML() + "&gt;</span>";
        var item = new TreeElement(title, element.representedObject, false);
        item.selectable = false;
        element.appendChild(item);
    }
}

WebInspector.DOMNodeTreeElement.expanded = function(element)
{
    element.treeOutline.panel.updateTreeSelection();
}

WebInspector.DOMNodeTreeElement.collapsed = function(element)
{
    element.treeOutline.panel.updateTreeSelection();
}

WebInspector.DOMNodeTreeElement.revealed = function(element)
{
    if (!element._listItemNode || !element.treeOutline || !element.treeOutline._childrenListNode)
        return;
    var parent = element.treeOutline.panel.views.dom.contentElement.treeContentElement;
    parent.scrollToElement(element._listItemNode);
}

WebInspector.DOMNodeTreeElement.selected = function(element)
{
    var panel = element.treeOutline.panel;
    panel.focusedDOMNode = element.representedObject;

    // Call updateSelection twice to make sure the height is correct,
    // the first time might have a bad height because we are in a weird tree state
    element.updateSelection(element);
    setTimeout(function() { element.updateSelection(element) }, 0);
}

WebInspector.DOMNodeTreeElement.doubleClicked = function(element)
{
    var panel = element.treeOutline.panel;
    panel.rootDOMNode = element.representedObject.parentNode;
    panel.focusedDOMNode = element.representedObject;
}

WebInspector.StylePropertyTreeElement = function(prop, computedStyle, usedProperties)
{
    var overloadCount = 0;
    var priority;
    if (prop.subProperties && prop.subProperties.length > 1) {
        for (var i = 0; i < prop.subProperties.length; ++i) {
            var name = prop.subProperties[i];
            if (!priority)
                priority = prop.style.getPropertyPriority(name);
            if (prop.unusedProperties[name])
                overloadCount++;
        }
    }

    if (!priority)
        priority = prop.style.getPropertyPriority(prop.name);

    var overloaded = (prop.unusedProperties[prop.name] || overloadCount === prop.subProperties.length);
    var title = WebInspector.StylePropertyTreeElement.createTitle(prop.name, prop.style, overloaded, priority, computedStyle, usedProperties);

    var item = new TreeElement(title, prop, (prop.subProperties && prop.subProperties.length > 1));
    item.computedStyle = computedStyle;
    item.onpopulate = WebInspector.StylePropertyTreeElement.populate;
    return item;
}

WebInspector.StylePropertyTreeElement.createTitle = function(name, style, overloaded, priority, computed, usedProperties)
{
    // "Nicknames" for some common values that are easier to read.
    var valueNickname = {
        "rgb(0, 0, 0)": "black",
        "rgb(255, 255, 255)": "white",
        "rgba(0, 0, 0, 0)": "transparent"
    };

    var alwaysShowComputedProperties = { "display": true, "height": true, "width": true };

    var value = style.getPropertyValue(name);

    var textValue = value;
    if (value) {
        var urls = value.match(/url\([^)]+\)/);
        if (urls) {
            for (var i = 0; i < urls.length; ++i) {
                var url = urls[i].substring(4, urls[i].length - 1);
                textValue = textValue.replace(urls[i], "url(" + WebInspector.linkifyURL(url) + ")");
            }
        } else {
            if (value in valueNickname)
                textValue = valueNickname[value];
            textValue = textValue.escapeHTML();
        }
    }

    var classes = [];
    if (!computed && (style.isPropertyImplicit(name) || value == "initial"))
        classes.push("implicit");
    if (computed && !usedProperties[name] && !alwaysShowComputedProperties[name])
        classes.push("inherited");
    if (overloaded)
        classes.push("overloaded");

    var title = "";
    if (classes.length)
        title += "<span class=\"" + classes.join(" ") + "\">";

    title += "<span class=\"name\">" + name.escapeHTML() + "</span>: ";
    title += "<span class=\"value\">" + textValue;
    if (priority && priority.length)
        title += " !" + priority;
    title += "</span>;";

    if (value) {
        // FIXME: this dosen't catch keyword based colors like black and white
        var colors = value.match(/(rgb\([0-9]+,\s?[0-9]+,\s?[0-9]+\))|(rgba\([0-9]+,\s?[0-9]+,\s?[0-9]+,\s?[0-9]+\))/g);
        if (colors) {
            var colorsLength = colors.length;
            for (var i = 0; i < colorsLength; ++i)
                title += "<span class=\"swatch\" style=\"background-color: " + colors[i] + "\"></span>";
        }
    }

    if (classes.length)
        title += "</span>";

    return title;
}

WebInspector.StylePropertyTreeElement.populate = function(element)
{
    if (element.children.length)
        return;

    var prop = element.representedObject;
    if (prop.subProperties && prop.subProperties.length > 1) {
        for (var i = 0; i < prop.subProperties.length; ++i) {
            var name = prop.subProperties[i];
            var overloaded = (prop.unusedProperties[prop.name] || prop.unusedProperties[name]);
            var priority = prop.style.getPropertyPriority(name);
            var title = WebInspector.StylePropertyTreeElement.createTitle(name, prop.style, overloaded, priority, element.computedStyle);
            var subitem = new TreeElement(title, {}, false);
            element.appendChild(subitem);
        }
    }
}

WebInspector.RequestResponseTreeElement = function(name, object)
{
    var title = "<span class=\"name\">" + name.escapeHTML() + "</span>";

    var hasChildren = object instanceof Object;
    if (!hasChildren) {
        var objectAsString = object.toString();
        title += ": <span class=\"value\">" + objectAsString.escapeHTML() + "</span>";
    }

    var item = new TreeElement(title, object, hasChildren);
    item.onpopulate = WebInspector.RequestResponseTreeElement.onpopulate;
    return item;
}

WebInspector.RequestResponseTreeElement.onpopulate = function(element)
{
    if (element.children.length)
        return;

    for (var field in element.representedObject) {
        var item = new WebInspector.RequestResponseTreeElement(field, element.representedObject[field]);
        element.appendChild(item);
    }
}

WebInspector.DOMPropertyTreeElement = function(name, object)
{
    var title = "<span class=\"name\">" + name.escapeHTML() + "</span>: ";
    title += "<span class=\"value\">" + Object.describe(object[name], true).escapeHTML() + "</span>";

    var hasChildren = false;
    var type = typeof object[name];
    if (object[name] && (type === "object" || type === "function")) {
        for (value in object[name]) {
            hasChildren = true;
            break;
        }
    }

    var representedObj = { object: object, name: name };
    var item = new TreeElement(title, representedObj, hasChildren);
    item.onpopulate = WebInspector.DOMPropertyTreeElement.populate;
    return item;
}

WebInspector.DOMPropertyTreeElement.populate = function(element)
{
    if (element.children.length)
        return;

    var parent = element.representedObject.object[element.representedObject.name];
    var properties = Object.sortedProperties(parent);
    for (var i = 0; i < properties.length; ++i) {
        if (properties[i] === "__treeElementIdentifier")
            continue;
        var item = new WebInspector.DOMPropertyTreeElement(properties[i], parent);
        element.appendChild(item);
    }
}
