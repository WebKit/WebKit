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

WebInspector.ResourceView = function(resource)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("resource-view");

    this.resource = resource;

    this.headersElement = document.createElement("div");
    this.headersElement.className = "resource-view-headers";
    this.element.appendChild(this.headersElement);

    this.contentElement = document.createElement("div");
    this.contentElement.className = "resource-view-content";
    this.element.appendChild(this.contentElement);

    this.headersListElement = document.createElement("ol");
    this.headersListElement.className = "outline-disclosure";
    this.headersElement.appendChild(this.headersListElement);

    this.headersTreeOutline = new TreeOutline(this.headersListElement);
    this.headersTreeOutline.expandTreeElementsWhenArrowing = true;

    this.urlTreeElement = new TreeElement("", null, false);
    this.urlTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.urlTreeElement);

    this.requestHeadersTreeElement = new TreeElement("", null, true);
    this.requestHeadersTreeElement.expanded = false;
    this.requestHeadersTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.requestHeadersTreeElement);

    this.responseHeadersTreeElement = new TreeElement("", null, true);
    this.responseHeadersTreeElement.expanded = false;
    this.responseHeadersTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.responseHeadersTreeElement);

    this.headersVisible = true;

    resource.addEventListener("url changed", this._refreshURL, this);
    resource.addEventListener("requestHeaders changed", this._refreshRequestHeaders, this);
    resource.addEventListener("responseHeaders changed", this._refreshResponseHeaders, this);

    this._refreshURL();
    this._refreshRequestHeaders();
    this._refreshResponseHeaders();
}

WebInspector.ResourceView.prototype = {
    get headersVisible()
    {
        return this._headersVisible;
    },

    set headersVisible(x)
    {
        if (x === this._headersVisible)
            return;

        this._headersVisible = x;

        if (x)
            this.element.addStyleClass("headers-visible");
        else
            this.element.removeStyleClass("headers-visible");
    },

    attach: function()
    {
        if (!this.element.parentNode) {
            var parentElement = (document.getElementById("resource-views") || document.getElementById("script-resource-views"));
            if (parentElement)
                parentElement.appendChild(this.element);
        }
    },

    _refreshURL: function()
    {
        this.urlTreeElement.title = this.resource.url.escapeHTML();
    },

    _refreshRequestHeaders: function()
    {
        this._refreshHeaders(WebInspector.UIString("Request Headers"), this.resource.sortedRequestHeaders, this.requestHeadersTreeElement);
    },

    _refreshResponseHeaders: function()
    {
        this._refreshHeaders(WebInspector.UIString("Response Headers"), this.resource.sortedResponseHeaders, this.responseHeadersTreeElement);
    },

    _refreshHeaders: function(title, headers, headersTreeElement)
    {
        headersTreeElement.removeChildren();

        var length = headers.length;
        headersTreeElement.title = title.escapeHTML() + "<span class=\"header-count\">" + WebInspector.UIString(" (%d)", length) + "</span>";
        headersTreeElement.hidden = !length;

        var length = headers.length;
        for (var i = 0; i < length; ++i) {
            var title = "<div class=\"header-name\">" + headers[i].header.escapeHTML() + ":</div>";
            title += "<div class=\"header-value\">" + headers[i].value.escapeHTML() + "</div>"

            var headerTreeElement = new TreeElement(title, null, false);
            headerTreeElement.selectable = false;
            headersTreeElement.appendChild(headerTreeElement);
        }
    }
}

WebInspector.ResourceView.prototype.__proto__ = WebInspector.View.prototype;
