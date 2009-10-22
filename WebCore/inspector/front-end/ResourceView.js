/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) IBM Corp. 2009  All rights reserved.
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

    this.httpInformationTreeElement = new TreeElement("", null, true);
    this.httpInformationTreeElement.expanded = false;
    this.httpInformationTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.httpInformationTreeElement);
     
    this.requestHeadersTreeElement = new TreeElement("", null, true);
    this.requestHeadersTreeElement.expanded = false;
    this.requestHeadersTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.requestHeadersTreeElement);

    this._decodeHover = WebInspector.UIString("Double-Click to toggle between URL encoded and decoded formats");
    this._decodeRequestParameters = true;

    this.queryStringTreeElement = new TreeElement("", null, true);
    this.queryStringTreeElement.expanded = false;
    this.queryStringTreeElement.selectable = false;
    this.queryStringTreeElement.hidden = true;
    this.headersTreeOutline.appendChild(this.queryStringTreeElement);

    this.formDataTreeElement = new TreeElement("", null, true);
    this.formDataTreeElement.expanded = false;
    this.formDataTreeElement.selectable = false;
    this.formDataTreeElement.hidden = true;
    this.headersTreeOutline.appendChild(this.formDataTreeElement);

    this.requestPayloadTreeElement = new TreeElement(WebInspector.UIString("Request Payload"), null, true);
    this.requestPayloadTreeElement.expanded = false;
    this.requestPayloadTreeElement.selectable = false;
    this.requestPayloadTreeElement.hidden = true;
    this.headersTreeOutline.appendChild(this.requestPayloadTreeElement);

    this.responseHeadersTreeElement = new TreeElement("", null, true);
    this.responseHeadersTreeElement.expanded = false;
    this.responseHeadersTreeElement.selectable = false;
    this.headersTreeOutline.appendChild(this.responseHeadersTreeElement);

    this.headersVisible = true;

    resource.addEventListener("url changed", this._refreshURL, this);
    resource.addEventListener("requestHeaders changed", this._refreshRequestHeaders, this);
    resource.addEventListener("responseHeaders changed", this._refreshResponseHeaders, this);
    resource.addEventListener("finished", this._refreshHTTPInformation, this);

    this._refreshURL();
    this._refreshRequestHeaders();
    this._refreshResponseHeaders();
    this._refreshHTTPInformation();
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
        var url = this.resource.url;
        var statusCodeImage = "";
        if (this.resource.statusCode) {
            var statusImageSource = "";
            
            if (this.resource.statusCode < 300)
                statusImageSource = "Images/successGreenDot.png";
            else if (this.resource.statusCode < 400)
                statusImageSource = "Images/warningOrangeDot.png";
            else
                statusImageSource = "Images/errorRedDot.png";
        
            statusCodeImage = "<img class=\"resource-status-image\" src=\"" + statusImageSource + "\" title=\"" + WebInspector.Resource.StatusTextForCode(this.resource.statusCode) + "\">";
        }

        this.urlTreeElement.title = statusCodeImage + "<span class=\"resource-url\">" + url.escapeHTML() + "</span>";
        this._refreshQueryString();
    },

    _refreshQueryString: function()
    {
        var url = this.resource.url;
        var hasQueryString = url.indexOf("?") >= 0;

        if (!hasQueryString) {
            this.queryStringTreeElement.hidden = true;
            return;
        }

        this.queryStringTreeElement.hidden = false;
        var parmString = url.split("?", 2)[1];
        this._refreshParms(WebInspector.UIString("Query String Parameters"), parmString, this.queryStringTreeElement);
    },

    _refreshFormData: function()
    {
        this.formDataTreeElement.hidden = true;
        this.requestPayloadTreeElement.hidden = true;

        var isFormData = this.resource.requestFormData;
        if (!isFormData)
            return;

        var isFormEncoded = false;
        var requestContentType = this._getHeaderValue(this.resource.requestHeaders, "Content-Type");
        if (requestContentType && requestContentType.match(/^application\/x-www-form-urlencoded\s*(;.*)?$/i))
            isFormEncoded = true;

        if (isFormEncoded) {
            this.formDataTreeElement.hidden = false;
            this._refreshParms(WebInspector.UIString("Form Data"), this.resource.requestFormData, this.formDataTreeElement);
        } else {
            this.requestPayloadTreeElement.hidden = false;
            this._refreshRequestPayload(this.resource.requestFormData);
        }
    },

    _refreshRequestPayload: function(formData)
    {
        this.requestPayloadTreeElement.removeChildren();

        var title = "<div class=\"header-name\">&nbsp;</div>";
        title += "<div class=\"raw-form-data header-value\">" + formData.escapeHTML() + "</div>";
        var parmTreeElement = new TreeElement(title, null, false);
        parmTreeElement.selectable = false;
        this.requestPayloadTreeElement.appendChild(parmTreeElement);
    },

    _refreshParms: function(title, parmString, parmsTreeElement)
    {
        var parms = parmString.split("&");
        for (var i = 0; i < parms.length; ++i) {
            var parm = parms[i];
            parm = parm.split("=", 2);
            if (parm.length == 1)
                parm.push("");
            parms[i] = parm;
        }

        parmsTreeElement.removeChildren();

        parmsTreeElement.title = title + "<span class=\"header-count\">" + WebInspector.UIString(" (%d)", parms.length) + "</span>";

        for (var i = 0; i < parms.length; ++i) {
            var key = parms[i][0];
            var value = parms[i][1];

            var errorDecoding = false;
            if (this._decodeRequestParameters) {
                if (value.indexOf("%") >= 0) {
                    try {
                        value = decodeURIComponent(value);
                    } catch(e) {
                        errorDecoding = true;
                    }
                }
                    
                value = value.replace(/\+/g, " ");
            }

            valueEscaped = value.escapeHTML();
            if (errorDecoding)
                valueEscaped += " <span class=\"error-message\">" + WebInspector.UIString("(unable to decode value)").escapeHTML() + "</span>";

            var title = "<div class=\"header-name\">" + key.escapeHTML() + ":</div>";
            title += "<div class=\"header-value\">" + valueEscaped + "</div>";

            var parmTreeElement = new TreeElement(title, null, false);
            parmTreeElement.selectable = false;
            parmTreeElement.tooltip = this._decodeHover;
            parmTreeElement.ondblclick = this._toggleURLdecoding.bind(this);
            parmsTreeElement.appendChild(parmTreeElement);
        }
    },

    _toggleURLdecoding: function(treeElement, event)
    {
        this._decodeRequestParameters = !this._decodeRequestParameters;
        this._refreshQueryString();
        this._refreshFormData();
    },

    _getHeaderValue: function(headers, key)
    {
        var lowerKey = key.toLowerCase();
        for (var testKey in headers) {
            if (testKey.toLowerCase() === lowerKey)
                return headers[testKey];
        }
    },

    _refreshRequestHeaders: function()
    {
        this._refreshHeaders(WebInspector.UIString("Request Headers"), this.resource.sortedRequestHeaders, this.requestHeadersTreeElement);
        this._refreshFormData();
    },

    _refreshResponseHeaders: function()
    {
        this._refreshHeaders(WebInspector.UIString("Response Headers"), this.resource.sortedResponseHeaders, this.responseHeadersTreeElement);
    },

    _refreshHTTPInformation: function()
    {
        const listElements = 2;

        var headerElement = this.httpInformationTreeElement;
        headerElement.removeChildren();
        headerElement.hidden = !this.resource.statusCode;

        if (this.resource.statusCode) {
            headerElement.title = WebInspector.UIString("HTTP Information") +  "<span class=\"header-count\">" + WebInspector.UIString(" (%d)", listElements) + "</span>";
        
            var title = "<div class=\"header-name\">" + WebInspector.UIString("Request Method") + ":</div>";
            title += "<div class=\"header-value\">" + this.resource.requestMethod + "</div>"
            
            var headerTreeElement = new TreeElement(title, null, false);
            headerTreeElement.selectable = false;
            headerElement.appendChild(headerTreeElement);
            
            title = "<div class=\"header-name\">" + WebInspector.UIString("Status Code") + ":</div>";
            title += "<div class=\"header-value\">" + WebInspector.Resource.StatusTextForCode(this.resource.statusCode) + "</div>"
            
            headerTreeElement = new TreeElement(title, null, false);
            headerTreeElement.selectable = false;
            headerElement.appendChild(headerTreeElement);
        }
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
