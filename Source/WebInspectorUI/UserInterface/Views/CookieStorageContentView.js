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

        this._cookies = [];
        this._sortComparator = null;
        this._table = null;

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("cookie-storage-refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._refreshButtonClicked, this);
    }

    // Public

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem];
    }

    saveToCookie(cookie)
    {
        cookie.type = WI.ContentViewCookieType.CookieStorage;
        cookie.host = this.representedObject.host;
    }

    get scrollableElements()
    {
        if (!this._table)
            return [];
        return [this._table.scrollContainer];
    }

    handleCopyEvent(event)
    {
        if (!this._table || !this._table.selectedRows.length)
            return;

        let cookies = this._cookiesAtIndexes(this._table.selectedRows);
        if (!cookies.length)
            return;

        event.clipboardData.setData("text/plain", this._formatCookiesAsText(cookies));
        event.stopPropagation();
        event.preventDefault();
    }

    // Table dataSource

    tableIndexForRepresentedObject(table, object)
    {
        let index = this._cookies.indexOf(object);
        console.assert(index >= 0);
        return index;
    }

    tableRepresentedObjectForIndex(table, index)
    {
        console.assert(index >= 0 && index < this._cookies.length);
        return this._cookies[index];
    }

    tableNumberOfRows(table)
    {
        return this._cookies.length;
    }

    tableSortChanged(table)
    {
        this._generateSortComparator();

        if (!this._sortComparator)
            return;

        this._updateSort();
        this._table.reloadData();
    }

    // Table delegate

    tableCellContextMenuClicked(table, cell, column, rowIndex, event)
    {
        let contextMenu = WI.ContextMenu.createFromEvent(event);

        contextMenu.appendSeparator();
        contextMenu.appendItem(WI.UIString("Copy"), () => {
            let rowIndexes;
            if (table.isRowSelected(rowIndex))
                rowIndexes = table.selectedRows;
            else
                rowIndexes = [rowIndex];

            let cookies = this._cookiesAtIndexes(rowIndexes);
            InspectorFrontendHost.copyText(this._formatCookiesAsText(cookies));
        });

        contextMenu.appendItem(WI.UIString("Delete"), () => {
            if (table.isRowSelected(rowIndex))
                table.removeSelectedRows();
            else
                table.removeRow(rowIndex);
        });
        contextMenu.appendSeparator();
    }

    tableDidRemoveRows(table, rowIndexes)
    {
        if (!rowIndexes.length)
            return;

        for (let i = rowIndexes.length - 1; i >= 0; --i) {
            let rowIndex = rowIndexes[i];
            let cookie = this._cookies[rowIndex];
            console.assert(cookie, "Missing cookie for row " + rowIndex);
            if (!cookie)
                continue;

            this._cookies.splice(rowIndex, 1);

            PageAgent.deleteCookie(cookie.name, cookie.url);
        }
    }

    tablePopulateCell(table, cell, column, rowIndex)
    {
        let cookie = this._cookies[rowIndex];
        cell.textContent = this._formatCookiePropertyForColumn(cookie, column);
        return cell;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._table = new WI.Table("cookies-table", this, this, 20);
        this._table.allowsMultipleSelection = true;

        this._nameColumn = new WI.TableColumn("name", WI.UIString("Name"), {
            minWidth: 70,
            maxWidth: 300,
            initialWidth: 200,
            resizeType: WI.TableColumn.ResizeType.Locked,
        });

        this._valueColumn = new WI.TableColumn("value", WI.UIString("Value"), {
            minWidth: 100,
            maxWidth: 600,
            initialWidth: 200,
            hideable: false,
        });

        this._domainColumn = new WI.TableColumn("domain", WI.UIString("Domain"), {
            minWidth: 100,
            maxWidth: 200,
            initialWidth: 120,
        });

        this._pathColumn = new WI.TableColumn("path", WI.UIString("Path"), {
            minWidth: 50,
            maxWidth: 300,
            initialWidth: 100,
        });

        this._expiresColumn = new WI.TableColumn("expires", WI.UIString("Expires"), {
            minWidth: 100,
            maxWidth: 200,
            initialWidth: 150,
        });

        this._sizeColumn = new WI.TableColumn("size", WI.UIString("Size"), {
            minWidth: 50,
            maxWidth: 80,
            initialWidth: 65,
            align: "right",
        });

        this._secureColumn = new WI.TableColumn("secure", WI.unlocalizedString("Secure"), {
            minWidth: 70,
            maxWidth: 70,
            align: "center",
        });

        this._httpOnlyColumn = new WI.TableColumn("httpOnly", WI.unlocalizedString("HttpOnly"), {
            minWidth: 80,
            maxWidth: 80,
            align: "center",
        });

        this._sameSiteColumn = new WI.TableColumn("sameSite", WI.unlocalizedString("SameSite"), {
            minWidth: 40,
            maxWidth: 80,
            initialWidth: 70,
            align: "center",
        });

        this._table.addColumn(this._nameColumn);
        this._table.addColumn(this._valueColumn);
        this._table.addColumn(this._domainColumn);
        this._table.addColumn(this._pathColumn);
        this._table.addColumn(this._expiresColumn);
        this._table.addColumn(this._sizeColumn);
        this._table.addColumn(this._secureColumn);
        this._table.addColumn(this._httpOnlyColumn);
        this._table.addColumn(this._sameSiteColumn);

        if (!this._table.sortColumnIdentifier) {
            this._table.sortOrder = WI.Table.SortOrder.Ascending;
            this._table.sortColumnIdentifier = "name";
         }

        this.addSubview(this._table);

        this._table.element.addEventListener("keydown", this._handleTableKeyDown.bind(this));

        this._reloadCookies();
    }

    // Private

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

    _generateSortComparator()
    {
        let sortColumnIdentifier = this._table.sortColumnIdentifier;
        if (!sortColumnIdentifier) {
            this._sortComparator = null;
            return;
        }

        let comparator = null;

        switch (sortColumnIdentifier) {
        case "name":
        case "value":
        case "domain":
        case "path":
        case "sameSite":
            comparator = (a, b) => (a[sortColumnIdentifier] || "").extendedLocaleCompare(b[sortColumnIdentifier] || "");
            break;

        case "size":
        case "httpOnly":
        case "secure":
            comparator = (a, b) => a[sortColumnIdentifier] - b[sortColumnIdentifier];
            break;

        case "expires":
            comparator = (a, b) => {
                if (!a.expires)
                    return 1;
                if (!b.expires)
                    return -1;
                return a.expires - b.expires;
            };
            break;

        default:
            console.assert("Unexpected sort column", sortColumnIdentifier);
            return;
        }

        let reverseFactor = this._table.sortOrder === WI.Table.SortOrder.Ascending ? 1 : -1;
        this._sortComparator = (a, b) => reverseFactor * comparator(a, b);
    }

    _refreshButtonClicked(event)
    {
        this._reloadCookies();
    }

    _reloadCookies()
    {
        PageAgent.getCookies().then((payload) => {
            this._cookies = this._filterCookies(payload.cookies.map(WI.Cookie.fromPayload));
            this._updateSort();
            this._table.reloadData();
        }).catch((error) => {
            console.error("Could not fetch cookies: ", error);
        });
    }

    _updateSort()
    {
        if (!this._sortComparator)
            return;

        this._cookies.sort(this._sortComparator);
    }

    _handleTableKeyDown(event)
    {
        if (event.keyCode === WI.KeyboardShortcut.Key.Backspace.keyCode || event.keyCode === WI.KeyboardShortcut.Key.Delete.keyCode)
            this._table.removeSelectedRows();
    }

    _cookiesAtIndexes(indexes)
    {
        if (!this._cookies.length)
            return [];
        return indexes.map((index) => this._cookies[index]);
    }

    _formatCookiesAsText(cookies)
    {
        let visibleColumns = this._table.columns.filter((column) => !column.hidden);
        if (!visibleColumns.length)
            return "";

        let lines = cookies.map((cookie) => {
            const usePunctuation = false;
            let values = visibleColumns.map((column) => this._formatCookiePropertyForColumn(cookie, column, usePunctuation));
            return values.join("\t");
        });

        return lines.join("\n");
    }

    _formatCookiePropertyForColumn(cookie, column, usePunctuation = true)
    {
        const checkmark = "\u2713";
        const missingValue = usePunctuation ? emDash : "";
        const missingCheckmark = usePunctuation ? zeroWidthSpace : "";

        switch (column.identifier) {
        case "name":
            return cookie.name;
        case "value":
            return cookie.value;
        case "domain":
            return cookie.domain || missingValue;
        case "path":
            return cookie.path || missingValue;
        case "expires":
            return cookie.expires ? new Date(cookie.expires).toLocaleString() : WI.UIString("Session");
        case "size":
            return Number.bytesToString(cookie.size);
        case "secure":
            return cookie.secure ? checkmark : missingCheckmark;
        case "httpOnly":
            return cookie.httpOnly ? checkmark : missingCheckmark;
        case "sameSite":
            return cookie.sameSite === WI.Cookie.SameSiteType.None ? missingValue : WI.Cookie.displayNameForSameSiteType(cookie.sameSite);
        }

        console.assert("Unexpected table column " + column.identifier);
        return "";
    }
};

WI.CookieType = {
    Request: 0,
    Response: 1
};
