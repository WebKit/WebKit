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

WebInspector.Resource = function(requestHeaders, url, domain, path, lastPathComponent, identifier, mainResource, cached)
{
    this.identifier = identifier;

    this.startTime = -1;
    this.endTime = -1;
    this.mainResource = mainResource;
    this.requestHeaders = requestHeaders;
    this.url = url;
    this.domain = domain;
    this.path = path;
    this.lastPathComponent = lastPathComponent;
    this.cached = cached;

    this.listItem = new WebInspector.ResourceTreeElement(this);
    this.updateTitle();

    this.category = WebInspector.resourceCategories.other;
}

// Keep these in sync with WebCore::InspectorResource::Type
WebInspector.Resource.Type = {
    Document:   0,
    Stylesheet: 1,
    Image:      2,
    Font:       3,
    Script:     4,
    Other:      5,

    isTextType: function(type)
    {
        return (type == this.Document) || (type == this.Stylesheet) || (type == this.Script);
    },

    toString: function(type)
    {
        switch (type) {
            case this.Document:
                return WebInspector.UIString("document");
            case this.Stylesheet:
                return WebInspector.UIString("stylesheet");
            case this.Image:
                return WebInspector.UIString("image");
            case this.Font:
                return WebInspector.UIString("font");
            case this.Script:
                return WebInspector.UIString("script");
            case this.Other:
            default:
                return WebInspector.UIString("other");
        }
    }
}

WebInspector.Resource.prototype = {
    get url()
    {
        return this._url;
    },

    set url(x)
    {
        if (this._url === x)
            return;

        var oldURL = this._url;
        this._url = x;
        WebInspector.resourceURLChanged(this, oldURL);
        this.updateTitleSoon();
    },

    get domain()
    {
        return this._domain;
    },

    set domain(x)
    {
        if (this._domain === x)
            return;
        this._domain = x;
        this.updateTitleSoon();
    },

    get lastPathComponent()
    {
        return this._lastPathComponent;
    },

    set lastPathComponent(x)
    {
        if (this._lastPathComponent === x)
            return;
        this._lastPathComponent = x;
        this._lastPathComponentLowerCase = x ? x.toLowerCase() : null;
        this.updateTitleSoon();
    },

    get displayName()
    {
        var title = this.lastPathComponent;
        if (!title)
            title = this.domain;
        if (!title)
            title = this.url;
        return title;
    },

    get startTime()
    {
        return this._startTime;
    },

    set startTime(x)
    {
        if (this._startTime === x)
            return;

        this._startTime = x;

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.refresh();
    },

    get responseReceivedTime()
    {
        return this._responseReceivedTime;
    },

    set responseReceivedTime(x)
    {
        if (this._responseReceivedTime === x)
            return;

        this._responseReceivedTime = x;

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.refresh();
    },

    get endTime()
    {
        return this._endTime;
    },

    set endTime(x)
    {
        if (this._endTime === x)
            return;

        this._endTime = x;

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.refresh();
    },

    get contentLength()
    {
        return this._contentLength;
    },

    set contentLength(x)
    {
        if (this._contentLength === x)
            return;

        this._contentLength = x;

        if (this._expectedContentLength && this._expectedContentLength > x) {
            this.updateTitle();
            var canvas = document.getElementById("loadingIcon" + this.identifier);
            if (canvas)
                WebInspector.drawLoadingPieChart(canvas, (x / this._expectedContentLength));
        }

        WebInspector.networkPanel.updateSummaryGraphSoon();
    },

    get expectedContentLength()
    {
        return this._expectedContentLength;
    },

    set expectedContentLength(x)
    {
        if (this._expectedContentLength === x)
            return;

        this._expectedContentLength = x;

        if (x && this._contentLength && this._contentLength <= x) {
            var canvas = document.getElementById("loadingIcon" + this.identifier);
            if (canvas)
                WebInspector.drawLoadingPieChart(canvas, (this._contentLength / x));
        }
    },

    get finished()
    {
        return this._finished;
    },

    set finished(x)
    {
        if (this._finished === x)
            return;

        this._finished = x;

        if (x) {
            var canvas = document.getElementById("loadingIcon" + this.identifier);
            if (canvas)
                canvas.parentNode.removeChild(canvas);

            this._checkTips();
            this._checkWarnings();
        }

        this.updateTitleSoon();
        this.updatePanel();
    },

    get failed()
    {
        return this._failed;
    },

    set failed(x)
    {
        this._failed = x;

        this.updateTitleSoon();
        this.updatePanel();
    },

    get category()
    {
        return this._category;
    },

    set category(x)
    {
        if (this._category === x)
            return;

        var oldCategory = this._category;
        if (oldCategory)
            oldCategory.removeResource(this);

        this._category = x;
        this.updateTitle();

        if (this._category)
            this._category.addResource(this);

        this.updatePanel();
    },

    get mimeType()
    {
        return this._mimeType;
    },

    set mimeType(x)
    {
        if (this._mimeType === x)
            return;

        this._mimeType = x;
    },

    get type()
    {
        return this._type;
    },

    set type(x)
    {
        if (this._type === x)
            return;

        this._type = x;

        switch (x) {
            case WebInspector.Resource.Type.Document:
                this.category = WebInspector.resourceCategories.documents;
                break;
            case WebInspector.Resource.Type.Stylesheet:
                this.category = WebInspector.resourceCategories.stylesheets;
                break;
            case WebInspector.Resource.Type.Script:
                this.category = WebInspector.resourceCategories.scripts;
                break;
            case WebInspector.Resource.Type.Image:
                this.category = WebInspector.resourceCategories.images;
                break;
            case WebInspector.Resource.Type.Font:
                this.category = WebInspector.resourceCategories.fonts;
                break;
            case WebInspector.Resource.Type.Other:
            default:
                this.category = WebInspector.resourceCategories.other;
                break;
        }
    },

    get documentNode() {
        if ("identifier" in this)
            return InspectorController.getResourceDocumentNode(this.identifier);
        return null;
    },

    get requestHeaders()
    {
        if (this._requestHeaders === undefined)
            this._requestHeaders = {};
        return this._requestHeaders;
    },

    set requestHeaders(x)
    {
        if (this._requestHeaders === x)
            return;

        this._requestHeaders = x;
        delete this._sortedRequestHeaders;

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.refreshInfo();
    },

    get sortedRequestHeaders()
    {
        if (this._sortedRequestHeaders !== undefined)
            return this._sortedRequestHeaders;

        this._sortedRequestHeaders = [];
        for (var key in this.requestHeaders)
            this._sortedRequestHeaders.push({header: key, value: this.requestHeaders[key]});
        this._sortedRequestHeaders.sort(function(a,b) { return a.header.localeCompare(b.header) });

        return this._sortedRequestHeaders;
    },

    get responseHeaders()
    {
        if (this._responseHeaders === undefined)
            this._responseHeaders = {};
        return this._responseHeaders;
    },

    set responseHeaders(x)
    {
        if (this._responseHeaders === x)
            return;

        this._responseHeaders = x;
        delete this._sortedResponseHeaders;

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.refreshInfo();
    },

    get sortedResponseHeaders()
    {
        if (this._sortedResponseHeaders !== undefined)
            return this._sortedResponseHeaders;

        this._sortedResponseHeaders = [];
        for (var key in this.responseHeaders)
            this._sortedResponseHeaders.push({header: key, value: this.responseHeaders[key]});
        this._sortedResponseHeaders.sort(function(a,b) { return a.header.localeCompare(b.header) });

        return this._sortedResponseHeaders;
    },

    get tips()
    {
        if (!("_tips" in this))
            this._tips = {};
        return this._tips;
    },

    _addTip: function(tip)
    {
        if (tip.id in this.tips)
            return;

        this.tips[tip.id] = tip;

        // FIXME: Re-enable this code once we have a scope bar in the Console.
        // Otherwise, we flood the Console with too many tips.
        /*
        var msg = new WebInspector.ConsoleMessage(WebInspector.ConsoleMessage.MessageSource.Other,
                    WebInspector.ConsoleMessage.MessageLevel.Tip, tip.message, -1, this.url);
        WebInspector.consolePanel.addMessage(msg);
        */

        if (this.networkTimelineEntry)
            this.networkTimelineEntry.showingTipButton = true;
    },

    _checkTips: function()
    {
        for (var tip in WebInspector.Tips)
            this._checkTip(WebInspector.Tips[tip]);
    },

    _checkTip: function(tip)
    {
        var addTip = false;
        switch (tip.id) {
            case WebInspector.Tips.ResourceNotCompressed.id:
                addTip = this._shouldCompress();
                break;
        }

        if (addTip)
            this._addTip(tip);
    },

    _shouldCompress: function()
    {
        return WebInspector.Resource.Type.isTextType(this.type)
            && this.domain
            && !("Content-Encoding" in this.responseHeaders)
            && this.contentLength !== undefined
            && this.contentLength >= 512;
    },

    _mimeTypeIsConsistentWithType: function()
    {
        if (this.type === undefined || this.type === WebInspector.Resource.Type.Other)
            return true;

        if (this.mimeType in WebInspector.MIMETypes)
            return this.type in WebInspector.MIMETypes[this.mimeType];

        return true;
    },

    _checkWarnings: function()
    {
        for (var warning in WebInspector.Warnings)
            this._checkWarning(WebInspector.Warnings[warning]);
    },

    _checkWarning: function(warning)
    {
        var addWarning = false;
        var msg;
        switch (warning.id) {
            case WebInspector.Warnings.IncorrectMIMEType.id:
                if (!this._mimeTypeIsConsistentWithType())
                    msg = new WebInspector.ConsoleMessage(WebInspector.ConsoleMessage.MessageSource.Other,
                                WebInspector.ConsoleMessage.MessageLevel.Warning,
                                String.sprintf(WebInspector.Warnings.IncorrectMIMEType.message,
                                    WebInspector.Resource.Type.toString(this.type), this.mimeType),
                                -1, this.url);
                break;
        }

        if (msg)
            WebInspector.consolePanel.addMessage(msg);
    },

    updateTitleSoon: function()
    {
        if (this.updateTitleTimeout)
            return;
        this.updateTitleTimeout = setTimeout(this.updateTitle.bind(this), 0);
    },

    updateTitle: function()
    {
        delete this.updateTitleTimeout;

        var title = this.displayName;

        var info = "";
        if (this.domain && (!WebInspector.mainResource || (WebInspector.mainResource && this.domain !== WebInspector.mainResource.domain)))
            info = this.domain;

        if (this.path && this.lastPathComponent) {
            var lastPathComponentIndex = this.path.lastIndexOf("/" + this.lastPathComponent);
            if (lastPathComponentIndex != -1)
                info += this.path.substring(0, lastPathComponentIndex);
        }

        var fullTitle = "";

        if (this.errors)
            fullTitle += "<span class=\"count errors\">" + (this.errors + this.warnings) + "</span>";
        else if (this.warnings)
            fullTitle += "<span class=\"count warnings\">" + this.warnings + "</span>";

        fullTitle += "<span class=\"title" + (info && info.length ? "" : " only") + "\">" + title.escapeHTML() + "</span>";
        if (info && info.length)
            fullTitle += "<span class=\"info\">" + info.escapeHTML() + "</span>";

        var iconClass = "icon";
        switch (this.category) {
        default:
            break;
        case WebInspector.resourceCategories.images:
        case WebInspector.resourceCategories.other:
            iconClass = "icon plain";
            break;
        case WebInspector.resourceCategories.fonts:
            iconClass = "icon font";
        }

        if (!this.finished)
            fullTitle += "<div class=\"" + iconClass + "\"><canvas id=\"loadingIcon" + this.identifier + "\" class=\"progress\" width=\"16\" height=\"16\"></canvas></div>";
        else if (this.category === WebInspector.resourceCategories.images)
            fullTitle += "<div class=\"" + iconClass + "\"><img class=\"preview\" src=\"" + this.url + "\"></div>";
        else if (this.category === WebInspector.resourceCategories.fonts) {
            var uniqueFontName = "WebInspectorFontPreview" + this.identifier;

            this.fontStyleElement = document.createElement("style");
            this.fontStyleElement.textContent = "@font-face { font-family: \"" + uniqueFontName + "\"; src: url(" + this.url + "); }";
            document.getElementsByTagName("head").item(0).appendChild(this.fontStyleElement);

            fullTitle += "<div class=\"" + iconClass + "\"><div class=\"preview\" style=\"font-family: " + uniqueFontName + "\">Ag</div></div>";
        } else
            fullTitle += "<div class=\"" + iconClass + "\"></div>";

        this.listItem.title = fullTitle;
        this.listItem.tooltip = this.url;
    },

    updatePanel: function()
    {
        if (this._panel) {
            var current = (WebInspector.currentPanel === this._panel);

            this._panel.detach();
            delete this._panel;

            if (current)
                WebInspector.currentPanel = this.panel;
        }
    },

    get panel()
    {
        if (!this._panel) {
            if (this.finished && !this.failed) {
                switch (this.category) {
                case WebInspector.resourceCategories.documents:
                    this._panel = new WebInspector.DocumentPanel(this);
                    break;
                case WebInspector.resourceCategories.stylesheets:
                case WebInspector.resourceCategories.scripts:
                    this._panel = new WebInspector.SourcePanel(this);
                    break;
                case WebInspector.resourceCategories.images:
                    this._panel = new WebInspector.ImagePanel(this);
                    break;
                case WebInspector.resourceCategories.fonts:
                    this._panel = new WebInspector.FontPanel(this);
                    break;
                }
            }

            if (!this._panel)
                this._panel = new WebInspector.ResourcePanel(this);
        }

        return this._panel;
    },

    select: function()
    {
        WebInspector.navigateToResource(this);
    },

    deselect: function()
    {
        this.listItem.deselect(true);
        if (WebInspector.currentPanel === this._panel)
            WebInspector.currentPanel = null;
    },

    attach: function()
    {
        if (this._panel)
            this._panel.attach();
    },

    detach: function()
    {
        if (this._panel)
            this._panel.detach();
        if (this.fontStyleElement && this.fontStyleElement.parentNode)
            this.fontStyleElement.parentNode.removeChild(this.fontStyleElement);
    },

    get errors()
    {
        if (!("_errors" in this))
            this._errors = 0;

        return this._errors;
    },

    set errors(x)
    {
        if (this._errors === x)
            return;

        this._errors = x;
        this.updateTitleSoon();
    },

    get warnings()
    {
        if (!("_warnings" in this))
            this._warnings = 0;

        return this._warnings;
    },

    set warnings(x)
    {
        if (this._warnings === x)
            return;

        this._warnings = x;
        this.updateTitleSoon();
    }
}

WebInspector.ResourceTreeElement = function(resource)
{
    TreeElement.call(this, "", resource, false);
    this.resource = resource;
}

WebInspector.ResourceTreeElement.prototype = {
    onselect: function()
    {
        var selectedElement = WebInspector.fileOutline.selectedTreeElement;
        if (selectedElement)
            selectedElement.deselect();
        this.resource.select();
    },

    ondeselect: function()
    {
        this.resource.deselect();
    },

    onreveal: function()
    {
        if (!this.listItemElement || !this.treeOutline || !this.treeOutline.childrenListElement)
            return;
        this.treeOutline.childrenListElement.scrollToElement(this.listItemElement);
    }
}

WebInspector.ResourceTreeElement.prototype.__proto__ = TreeElement.prototype;
