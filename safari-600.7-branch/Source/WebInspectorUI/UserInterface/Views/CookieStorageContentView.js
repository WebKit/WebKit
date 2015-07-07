/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.CookieStorageContentView = function(representedObject)
{
    WebInspector.ContentView.call(this, representedObject);

    this.element.classList.add(WebInspector.CookieStorageContentView.StyleClassName);

    this.update();
};

WebInspector.CookieStorageContentView.StyleClassName = "cookie-storage";

WebInspector.CookieStorageContentView.prototype = {
    constructor: WebInspector.CookieStorageContentView,

    // Public

    update: function()
    {
        function callback(error, cookies)
        {
            if (error)
                return;

            this._cookies = this._filterCookies(cookies);
            this._rebuildTable();
        }

        PageAgent.getCookies(callback.bind(this));
    },

    updateLayout: function()
    {
        if (this._dataGrid)
            this._dataGrid.updateLayout();
    },

    saveToCookie: function(cookie)
    {
        cookie.type = WebInspector.ContentViewCookieType.CookieStorage;
        cookie.host = this.representedObject.host;
    },

    get scrollableElements()
    {
        if (!this._dataGrid)
            return [];
        return [this._dataGrid.scrollContainer];
    },

    // Private

    _rebuildTable: function()
    {
        // FIXME: If there are no cookies, do we want to show an empty datagrid, or do something like the old
        // inspector and show some text saying there are no cookies?
        if (!this._dataGrid) {
            var columns = {name: {}, value: {}, domain: {}, path: {}, expires: {}, size: {}, http: {}, secure: {}};

            columns.name.title = WebInspector.UIString("Name");
            columns.name.sortable = true;
            columns.name.width = "24%";

            columns.value.title = WebInspector.UIString("Value");
            columns.value.sortable = true;
            columns.value.width = "34%";

            columns.domain.title = WebInspector.UIString("Domain");
            columns.domain.sortable = true;
            columns.domain.width = "7%";

            columns.path.title = WebInspector.UIString("Path");
            columns.path.sortable = true;
            columns.path.width = "7%";

            columns.expires.title = WebInspector.UIString("Expires");
            columns.expires.sortable = true;
            columns.expires.width = "7%";

            columns.size.title = WebInspector.UIString("Size");
            columns.size.aligned = "right";
            columns.size.sortable = true;
            columns.size.width = "7%";

            columns.http.title = WebInspector.UIString("HTTP");
            columns.http.aligned = "centered";
            columns.http.sortable = true;
            columns.http.width = "7%";

            columns.secure.title = WebInspector.UIString("Secure");
            columns.secure.aligned = "centered";
            columns.secure.sortable = true;
            columns.secure.width = "7%";

            this._dataGrid = new WebInspector.DataGrid(columns, null, this._deleteCallback.bind(this));
            this._dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, this._sortDataGrid, this);

            this.element.appendChild(this._dataGrid.element);
            this._dataGrid.updateLayout();
        }

        console.assert(this._dataGrid);
        this._dataGrid.removeChildren();

        for (var cookie of this._cookies) {
            const checkmark = "\u2713";
            var data = {
                "name": cookie.name,
                "value": cookie.value,
                "domain": cookie.domain || "",
                "path": cookie.path || "",
                "expires": "",
                "size": Number.bytesToString(cookie.size),
                "http": cookie.httpOnly ? checkmark : "",
                "secure": cookie.secure ? checkmark : "",
            };

            if (cookie.type !== WebInspector.CookieType.Request)
                data["expires"] = cookie.session ? WebInspector.UIString("Session") : new Date(cookie.expires).toLocaleString();

            var node = new WebInspector.DataGridNode(data);
            node.selectable = true;
            node.cookie = cookie;

            this._dataGrid.appendChild(node);
        }

        this._dataGrid.sortColumnIdentifier = "name";
    },

    _filterCookies: function(cookies)
    {
        var filteredCookies = [];
        var resourcesForDomain = [];

        var frames = WebInspector.frameResourceManager.frames;
        for (var i = 0; i < frames.length; ++i) {
            var resources = frames[i].resources;
            for (var j = 0; j < resources.length; ++j) {
                var urlComponents = resources[j].urlComponents;
                if (urlComponents && urlComponents.host && urlComponents.host === this.representedObject.host)
                    resourcesForDomain.push(resources[j].url);
            }

            // The main resource isn't always in the list of resources, make sure to add it to the list of resources
            // we get the URLs from.
            var mainResourceURLComponents = frames[i].mainResource.urlComponents;
            if (mainResourceURLComponents && mainResourceURLComponents.host && mainResourceURLComponents.host == this.representedObject.host)
                resourcesForDomain.push(frames[i].mainResource.url);
        }

        for (var i = 0; i < cookies.length; ++i) {
            for (var j = 0; j < resourcesForDomain.length; ++j) {
                if (WebInspector.cookieMatchesResourceURL(cookies[i], resourcesForDomain[j])) {
                    filteredCookies.push(cookies[i]);
                    break;
                }
            }
        }

        return filteredCookies;
    },

    _sortDataGrid: function()
    {
        function localeCompare(field, nodeA, nodeB)
        {
            return (nodeA.data[field] + "").localeCompare(nodeB.data[field] + "");
        }

        function numberCompare(field, nodeA, nodeB)
        {
            return nodeA.cookie[field] - nodeB.cookie[field];
        }

        function expiresCompare(nodeA, nodeB)
        {
            if (nodeA.cookie.session !== nodeB.cookie.session)
                return nodeA.cookie.session ? 1 : -1;

            if (nodeA.cookie.session)
                return 0;

            return nodeA.data["expires"] - nodeB.data["expires"];
        }

        var comparator;
        switch (this._dataGrid.sortColumnIdentifier) {
            case "value": comparator = localeCompare.bind(this, "value"); break;
            case "domain": comparator = localeCompare.bind(this, "domain"); break;
            case "path": comparator = localeCompare.bind(this, "path"); break;
            case "expires": comparator = expiresCompare; break;
            case "size": comparator = numberCompare.bind(this, "size"); break;
            case "http": comparator = localeCompare.bind(this, "http"); break;
            case "secure": comparator = localeCompare.bind(this, "secure"); break;
            case "name":
            default: comparator = localeCompare.bind(this, "name"); break;
        }

        console.assert(comparator);
        this._dataGrid.sortNodes(comparator);
    },

    _deleteCallback: function(node)
    {
        if (!node || !node.cookie)
            return;

        var cookie = node.cookie;
        var cookieURL = (cookie.secure ? "https://" : "http://") + cookie.domain + cookie.path;

        // COMPATIBILITY (iOS 6): PageAgent.deleteCookie used to take 'domain', now takes 'url'. Send both.
        PageAgent.deleteCookie.invoke({cookieName: cookie.name, domain: cookie.domain, url: cookieURL});

        this.update();
    }
};

WebInspector.CookieStorageContentView.prototype.__proto__ = WebInspector.ContentView.prototype;

WebInspector.cookieMatchesResourceURL = function(cookie, resourceURL)
{
    var parsedURL = parseURL(resourceURL);
    if (!parsedURL || !WebInspector.cookieDomainMatchesResourceDomain(cookie.domain, parsedURL.host))
        return false;

    return (parsedURL.path.startsWith(cookie.path)
        && (!cookie.port || parsedURL.port == cookie.port)
        && (!cookie.secure || parsedURL.scheme === "https"));
}

WebInspector.cookieDomainMatchesResourceDomain = function(cookieDomain, resourceDomain)
{
    if (cookieDomain.charAt(0) !== ".")
        return resourceDomain === cookieDomain;
    return !!resourceDomain.match(new RegExp("^([^\\.]+\\.)?" + cookieDomain.substring(1).escapeForRegExp() + "$"), "i");
}

WebInspector.CookieType = {
    Request: 0,
    Response: 1
};
