/*
 * Copyright (C) 2013, 2015, 2018 Apple Inc. All rights reserved.
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

WI.CookieStorageContentView = class CookieStorageContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        super(representedObject);

        this.element.classList.add("cookie-storage");

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("cookie-storage-refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._refreshButtonClicked, this);

        this.update();
    }

    // Public

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem];
    }

    update()
    {
        PageAgent.getCookies().then((payload) => {
            this._cookies = this._filterCookies(payload.cookies);
            this._rebuildTable();
        }).catch((error) => {
            console.error("Could not fetch cookies: ", error);
        });
    }

    saveToCookie(cookie)
    {
        cookie.type = WI.ContentViewCookieType.CookieStorage;
        cookie.host = this.representedObject.host;
    }

    get scrollableElements()
    {
        if (!this._dataGrid)
            return [];
        return [this._dataGrid.scrollContainer];
    }

    // Private

    _rebuildTable()
    {
        // FIXME <https://webkit.org/b/151400>: If there are no cookies, add placeholder explanatory text.
        if (!this._dataGrid) {
            var columns = {name: {}, value: {}, domain: {}, path: {}, expires: {}, size: {}, http: {}, secure: {}, sameSite: {}};

            columns.name.title = WI.UIString("Name");
            columns.name.sortable = true;
            columns.name.width = "24%";
            columns.name.locked = true;

            columns.value.title = WI.UIString("Value");
            columns.value.sortable = true;
            columns.value.width = "34%";
            columns.value.locked = true;

            columns.domain.title = WI.UIString("Domain");
            columns.domain.sortable = true;
            columns.domain.width = "6%";

            columns.path.title = WI.UIString("Path");
            columns.path.sortable = true;
            columns.path.width = "6%";

            columns.expires.title = WI.UIString("Expires");
            columns.expires.sortable = true;
            columns.expires.width = "6%";

            columns.size.title = WI.UIString("Size");
            columns.size.aligned = "right";
            columns.size.sortable = true;
            columns.size.width = "6%";

            columns.http.title = WI.UIString("HTTP");
            columns.http.aligned = "centered";
            columns.http.sortable = true;
            columns.http.width = "6%";

            columns.secure.title = WI.UIString("Secure");
            columns.secure.aligned = "centered";
            columns.secure.sortable = true;
            columns.secure.width = "6%";

            columns.sameSite.title = WI.UIString("Same-Site");
            columns.sameSite.sortable = true;
            columns.sameSite.width = "6%";

            this._dataGrid = new WI.DataGrid(columns, null, this._deleteCallback.bind(this));
            this._dataGrid.columnChooserEnabled = true;
            this._dataGrid.addEventListener(WI.DataGrid.Event.SortChanged, this._sortDataGrid, this);
            this._dataGrid.sortColumnIdentifier = "name";
            this._dataGrid.createSettings("cookie-storage-content-view");

            this.addSubview(this._dataGrid);
            this._dataGrid.updateLayout();
        }

        console.assert(this._dataGrid);
        this._dataGrid.removeChildren();

        for (let cookie of this._cookies) {
            const checkmark = "\u2713";
            var data = {
                name: cookie.name,
                value: cookie.value,
                domain: cookie.domain || "",
                path: cookie.path || "",
                expires: "",
                size: Number.bytesToString(cookie.size),
                http: cookie.httpOnly ? checkmark : "",
                secure: cookie.secure ? checkmark : "",
                sameSite: cookie.sameSite && cookie.sameSite !== WI.Cookie.SameSiteType.None ? WI.Cookie.displayNameForSameSiteType(cookie.sameSite) : "",
            };

            if (cookie.type !== WI.CookieType.Request)
                data["expires"] = cookie.session ? WI.UIString("Session") : new Date(cookie.expires).toLocaleString();

            var node = new WI.DataGridNode(data);
            node.cookie = cookie;

            this._dataGrid.appendChild(node);
        }
    }

    _filterCookies(cookies)
    {
        let resourceMatchesStorageDomain = (resource) => {
            let urlComponents = resource.urlComponents;
            return urlComponents && urlComponents.host && urlComponents.host === this.representedObject.host;
        };

        let allResources = [];
        for (let frame of WI.networkManager.frames) {
            // The main resource isn't in the list of resources, so add it as a candidate.
            allResources.push(frame.mainResource, ...frame.resourceCollection);
        }

        let resourcesForDomain = allResources.filter(resourceMatchesStorageDomain);

        let cookiesForDomain = cookies.filter((cookie) => {
            return resourcesForDomain.some((resource) => {
                return WI.CookieStorageObject.cookieMatchesResourceURL(cookie, resource.url);
            });
        });
        return cookiesForDomain;
    }

    _sortDataGrid()
    {
        function localeCompare(field, nodeA, nodeB)
        {
            return (nodeA.data[field] + "").extendedLocaleCompare(nodeB.data[field] + "");
        }

        function numberCompare(field, nodeA, nodeB)
        {
            return nodeA.cookie[field] - nodeB.cookie[field];
        }

        function expiresCompare(nodeA, nodeB)
        {
            if (nodeA.cookie.session !== nodeB.cookie.session)
                return nodeA.cookie.session ? -1 : 1;

            if (nodeA.cookie.session)
                return 0;

            return nodeA.cookie.expires - nodeB.cookie.expires;
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
            case "sameSite": comparator = localeCompare.bind(this, "sameSite"); break;
            case "name":
            default: comparator = localeCompare.bind(this, "name"); break;
        }

        console.assert(comparator);
        this._dataGrid.sortNodes(comparator);
    }

    _deleteCallback(node)
    {
        if (!node || !node.cookie)
            return;

        var cookie = node.cookie;
        var cookieURL = (cookie.secure ? "https://" : "http://") + cookie.domain + cookie.path;
        PageAgent.deleteCookie(cookie.name, cookieURL);

        this.update();
    }

    _refreshButtonClicked(event)
    {
        this.update();
    }
};

WI.CookieType = {
    Request: 0,
    Response: 1
};
