/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.NetworkTableContentView = class NetworkTableContentView extends WI.ContentView
{
    constructor(representedObject, extraArguments)
    {
        super(representedObject);

        this._entries = [];
        this._entriesSortComparator = null;
        this._filteredEntries = [];
        this._pendingInsertions = [];
        this._pendingUpdates = [];

        this._table = null;
        this._nameColumnWidthSetting = new WI.Setting("network-table-content-view-name-column-width", 250);

        // FIXME: Resource Detail View.
        // FIXME: Network Timeline.
        // FIXME: Filter text field.
        // FIXME: Throttling.
        // FIXME: HAR Export.

        const exclusive = true;
        this._typeFilterScopeBarItemAll = new WI.ScopeBarItem("network-type-filter-all", WI.UIString("All"), exclusive);
        let typeFilterScopeBarItems = [this._typeFilterScopeBarItemAll];

        let uniqueTypes = [
            ["Document", (type) => type === WI.Resource.Type.Document],
            ["Stylesheet", (type) => type === WI.Resource.Type.Stylesheet],
            ["Image", (type) => type === WI.Resource.Type.Image],
            ["Font", (type) => type === WI.Resource.Type.Font],
            ["Script", (type) => type === WI.Resource.Type.Script],
            ["XHR", (type) => type === WI.Resource.Type.XHR || type === WI.Resource.Type.Fetch],
            ["Other", (type) => type === WI.Resource.Type.Other || type === WI.Resource.Type.WebSocket],
        ];
        for (let [key, checker] of uniqueTypes) {
            let type = WI.Resource.Type[key];
            let scopeBarItem = new WI.ScopeBarItem("network-type-filter-" + key, WI.NetworkTableContentView.shortDisplayNameForResourceType(type))
            scopeBarItem.__checker = checker;
            typeFilterScopeBarItems.push(scopeBarItem);
        }

        this._typeFilterScopeBar = new WI.ScopeBar("network-type-filter-scope-bar", typeFilterScopeBarItems, typeFilterScopeBarItems[0]);
        this._typeFilterScopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._typeFilterScopeBarSelectionChanged, this);

        this._activeTypeFilters = this._generateTypeFilter();

        // COMPATIBILITY (iOS 10.3): Network.setDisableResourceCaching did not exist.
        if (window.NetworkAgent && NetworkAgent.setResourceCachingDisabled) {
            let toolTipForDisableResourceCache = WI.UIString("Ignore the resource cache when loading resources");
            let activatedToolTipForDisableResourceCache = WI.UIString("Use the resource cache when loading resources");
            this._disableResourceCacheNavigationItem = new WI.ActivateButtonNavigationItem("disable-resource-cache", toolTipForDisableResourceCache, activatedToolTipForDisableResourceCache, "Images/IgnoreCaches.svg", 16, 16);
            this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;

            this._disableResourceCacheNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleDisableResourceCache, this);
            WI.resourceCachingDisabledSetting.addEventListener(WI.Setting.Event.Changed, this._resourceCachingDisabledSettingChanged, this);
        }

        this._clearNetworkItemsNavigationItem = new WI.ButtonNavigationItem("clear-network-items", WI.UIString("Clear Network Items (%s)").format(WI.clearKeyboardShortcut.displayName), "Images/NavigationItemClear.svg", 16, 16);
        this._clearNetworkItemsNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => this.reset());

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Resource.addEventListener(WI.Resource.Event.LoadingDidFinish, this._resourceLoadingDidFinish, this);
        WI.Resource.addEventListener(WI.Resource.Event.LoadingDidFail, this._resourceLoadingDidFail, this);
        WI.Resource.addEventListener(WI.Resource.Event.TransferSizeDidChange, this._resourceTransferSizeDidChange, this);
        WI.timelineManager.persistentNetworkTimeline.addEventListener(WI.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
    }

    // Static

    static shortDisplayNameForResourceType(type)
    {
        switch (type) {
        case WI.Resource.Type.Document:
            return WI.UIString("Document");
        case WI.Resource.Type.Stylesheet:
            return "CSS";
        case WI.Resource.Type.Image:
            return WI.UIString("Image");
        case WI.Resource.Type.Font:
            return WI.UIString("Font");
        case WI.Resource.Type.Script:
            return "JS";
        case WI.Resource.Type.XHR:
        case WI.Resource.Type.Fetch:
            return "XHR";
        case WI.Resource.Type.Ping:
            return WI.UIString("ping");
        case WI.Resource.Type.Beacon:
            return WI.UIString("Beacon");
        case WI.Resource.Type.WebSocket:
        case WI.Resource.Type.Other:
            return WI.UIString("Other");
        default:
            console.error("Unknown resource type", type);
            return null;
        }
    }

    // Public

    get selectionPathComponents()
    {
        return null;
    }

    get navigationItems()
    {
        let items = [this._typeFilterScopeBar];

        if (this._disableResourceCacheNavigationItem)
            items.push(this._disableResourceCacheNavigationItem);
        items.push(this._clearNetworkItemsNavigationItem);

        return items;
    }

    shown()
    {
        super.shown();

        if (this._table)
            this._table.restoreScrollPosition();
    }

    closed()
    {
        super.closed();

        WI.Frame.removeEventListener(null, null, this);
        WI.Resource.removeEventListener(null, null, this);
        WI.timelineManager.persistentNetworkTimeline.removeEventListener(WI.Timeline.Event.RecordAdded, this._networkTimelineRecordAdded, this);
    }

    reset()
    {
        this._entries = [];
        this._filteredEntries = [];
        this._pendingInsertions = [];

        if (this._table) {
            this._table.clearSelectedRow();
            this._table.reloadData();
        }
    }

    // Table dataSource

    tableNumberOfRows(table)
    {
        return this._filteredEntries.length;
    }

    tableSortChanged(table)
    {
        this._generateSortComparator();

        if (!this._entriesSortComparator)
            return;

        this._entries = this._entries.sort(this._entriesSortComparator);
        this._updateFilteredEntries();
        this._table.reloadData();
    }

    // Table delegate

    tableCellClicked(table, cell, column, rowIndex, event)
    {
        // FIXME: Show resource detail view.
    }

    tableCellContextMenuClicked(table, cell, column, rowIndex, event)
    {
        if (column !== this._nameColumn)
            return;

        this._table.selectRow(rowIndex);

        let entry = this._filteredEntries[rowIndex];
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        WI.appendContextMenuItemsForSourceCode(contextMenu, entry.resource);
    }

    tableSelectedRowChanged(table, rowIndex)
    {
        // FIXME: Show resource detail view.
    }

    tablePopulateCell(table, cell, column, rowIndex)
    {
        let entry = this._filteredEntries[rowIndex];

        cell.classList.toggle("error", entry.resource.hadLoadingError());

        switch (column.identifier) {
        case "name":
            this._populateNameCell(cell, entry);
            break;
        case "domain":
            cell.textContent = entry.domain || emDash;
            break;
        case "type":
            cell.textContent = entry.displayType || emDash;
            break;
        case "mimeType":
            cell.textContent = entry.mimeType || emDash;
            break;
        case "method":
            cell.textContent = entry.method || emDash;
            break;
        case "scheme":
            cell.textContent = entry.scheme || emDash;
            break;
        case "status":
            cell.textContent = entry.status || emDash;
            break;
        case "protocol":
            cell.textContent = entry.protocol || emDash;
            break;
        case "priority":
            cell.textContent = WI.Resource.displayNameForPriority(entry.priority) || emDash;
            break;
        case "remoteAddress":
            cell.textContent = entry.remoteAddress || emDash;
            break;
        case "connectionIdentifier":
            cell.textContent = entry.connectionIdentifier || emDash;
            break;
        case "resourceSize":
            cell.textContent = isNaN(entry.resourceSize) ? emDash : Number.bytesToString(entry.resourceSize);
            break;
        case "transferSize":
            this._populateTransferSizeCell(cell, entry);
            break;
        case "time":
            // FIXME: <https://webkit.org/b/176748> Web Inspector: Frontend sometimes receives resources with negative duration (responseEnd - requestStart)
            cell.textContent = isNaN(entry.time) ? emDash : Number.secondsToString(Math.max(entry.time, 0));
            break;
        case "waterfall":
            // FIXME: Waterfall graph.
            cell.textContent = emDash;
            break;
        }

        return cell;
    }

    // Private

    _populateNameCell(cell, entry)
    {
        console.assert(!cell.firstChild, "We expect the cell to be empty.", cell, cell.firstChild);

        let resource = entry.resource;
        if (resource.isLoading()) {
            let statusElement = cell.appendChild(document.createElement("div"));
            statusElement.className = "status";
            let spinner = new WI.IndeterminateProgressSpinner;
            statusElement.appendChild(spinner.element);
        }

        let iconElement = cell.appendChild(document.createElement("img"));
        iconElement.className = "icon";
        cell.classList.add(WI.ResourceTreeElement.ResourceIconStyleClassName, entry.resource.type);

        let nameElement = cell.appendChild(document.createElement("span"));
        nameElement.textContent = entry.name;
    }

    _populateTransferSizeCell(cell, entry)
    {
        let responseSource = entry.resource.responseSource;
        if (responseSource === WI.Resource.ResponseSource.MemoryCache) {
            cell.classList.add("cache-type");
            cell.textContent = WI.UIString("(memory)");
            return;
        }
        if (responseSource === WI.Resource.ResponseSource.DiskCache) {
            cell.classList.add("cache-type");
            cell.textContent = WI.UIString("(disk)");
            return;
        }

        let transferSize = entry.transferSize;
        cell.textContent = isNaN(transferSize) ? emDash : Number.bytesToString(transferSize);
        console.assert(!cell.classList.contains("cache-type"), "Should not have cache-type class on cell.");
    }

    _generateSortComparator()
    {
        let sortColumnIdentifier = this._table.sortColumnIdentifier;
        if (!sortColumnIdentifier) {
            this._entriesSortComparator = null;
            return;
        }

        let comparator;

        switch (sortColumnIdentifier) {
        case "name":
        case "domain":
        case "mimeType":
        case "method":
        case "scheme":
        case "protocol":
        case "remoteAddress":
            // Simple string.
            comparator = (a, b) => (a[sortColumnIdentifier] || "").extendedLocaleCompare(b[sortColumnIdentifier] || "");
            break;

        case "status":
        case "connectionIdentifier":
        case "resourceSize":
        case "time":
            // Simple number.
            comparator = (a, b) => {
                let aValue = a[sortColumnIdentifier];
                if (isNaN(aValue))
                    return 1;
                let bValue = b[sortColumnIdentifier];
                if (isNaN(bValue))
                    return -1;
                return aValue - bValue;
            }
            break;

        case "priority":
            // Resource.NetworkPriority enum.
            comparator = (a, b) => WI.Resource.comparePriority(a.priority, b.priority);
            break;

        case "type":
            // Sort by displayType string.
            comparator = (a, b) => (a.displayType || "").extendedLocaleCompare(b.displayType || "");
            break;

        case "transferSize":
            // Handle (memory) and (disk) values.
            comparator = (a, b) => {
                let transferSizeA = a.transferSize;
                let transferSizeB = b.transferSize;

                // Treat NaN as the largest value.
                if (isNaN(transferSizeA))
                    return 1;
                if (isNaN(transferSizeB))
                    return -1;

                // Treat memory cache and disk cache as small values.
                let sourceA = a.resource.responseSource;
                if (sourceA === WI.Resource.ResponseSource.MemoryCache)
                    transferSizeA = -20;
                else if (sourceA === WI.Resource.ResponseSource.DiskCache)
                    transferSizeA = -10;

                let sourceB = b.resource.responseSource;
                if (sourceB === WI.Resource.ResponseSource.MemoryCache)
                    transferSizeB = -20;
                else if (sourceB === WI.Resource.ResponseSource.DiskCache)
                    transferSizeB = -10;

                return transferSizeA - transferSizeB;
            };
            break;

        case "waterfall":
            // Sort by startTime number.
            comparator = comparator = (a, b) => a.startTime - b.startTime;
            break;

        default:
            console.assert("Unexpected sort column", sortColumnIdentifier);
            return;
        }

        let reverseFactor = this._table.sortOrder === WI.Table.SortOrder.Ascending ? 1 : -1;
        this._entriesSortComparator = (a, b) => reverseFactor * comparator(a, b);
    }

    // Protected

    initialLayout()
    {
        this._nameColumn = new WI.TableColumn("name", WI.UIString("Name"), {
            initialWidth: this._nameColumnWidthSetting.value,
            minWidth: WI.Sidebar.AbsoluteMinimumWidth,
            maxWidth: 500,
            resizeType: WI.TableColumn.ResizeType.Locked,
        });

        this._nameColumn.addEventListener(WI.TableColumn.Event.WidthDidChange, this._tableNameColumnDidChangeWidth, this);

        this._domainColumn = new WI.TableColumn("domain", WI.UIString("Domain"), {
            minWidth: 120,
            maxWidth: 200,
        });

        this._typeColumn = new WI.TableColumn("type", WI.UIString("Type"), {
            minWidth: 70,
            maxWidth: 120,
        });

        this._mimeTypeColumn = new WI.TableColumn("mimeType", WI.UIString("MIME Type"), {
            hidden: true,
            minWidth: 100,
            maxWidth: 150,
        });

        this._methodColumn = new WI.TableColumn("method", WI.UIString("Method"), {
            hidden: true,
            minWidth: 55,
            maxWidth: 80,
        });

        this._schemeColumn = new WI.TableColumn("scheme", WI.UIString("Scheme"), {
            hidden: true,
            minWidth: 55,
            maxWidth: 80,
        });

        this._statusColumn = new WI.TableColumn("status", WI.UIString("Status"), {
            hidden: true,
            minWidth: 50,
            maxWidth: 50,
            align: "left",
        });

        this._protocolColumn = new WI.TableColumn("protocol", WI.UIString("Protocol"), {
            hidden: true,
            minWidth: 65,
            maxWidth: 80,
        });

        this._priorityColumn = new WI.TableColumn("priority", WI.UIString("Priority"), {
            hidden: true,
            minWidth: 65,
            maxWidth: 80,
        });

        this._remoteAddressColumn = new WI.TableColumn("remoteAddress", WI.UIString("IP Address"), {
            hidden: true,
            minWidth: 150,
        });

        this._connectionIdentifierColumn = new WI.TableColumn("connectionIdentifier", WI.UIString("Connection ID"), {
            hidden: true,
            minWidth: 50,
            maxWidth: 120,
            align: "right",
        });

        this._resourceSizeColumn = new WI.TableColumn("resourceSize", WI.UIString("Resource Size"), {
            hidden: true,
            minWidth: 80,
            maxWidth: 100,
            align: "right",
        });

        this._transferSizeColumn = new WI.TableColumn("transferSize", WI.UIString("Transfer Size"), {
            minWidth: 100,
            maxWidth: 150,
            align: "right",
        });

        this._timeColumn = new WI.TableColumn("time", WI.UIString("Time"), {
            minWidth: 65,
            maxWidth: 90,
            align: "right",
        });

        this._waterfallColumn = new WI.TableColumn("waterfall", WI.UIString("Waterfall"), {
            minWidth: 230,
        });

        this._table = new WI.Table("network-table", this, this, 20);

        this._table.addColumn(this._nameColumn);
        this._table.addColumn(this._domainColumn);
        this._table.addColumn(this._typeColumn);
        this._table.addColumn(this._mimeTypeColumn);
        this._table.addColumn(this._methodColumn);
        this._table.addColumn(this._schemeColumn);
        this._table.addColumn(this._statusColumn);
        this._table.addColumn(this._protocolColumn);
        this._table.addColumn(this._priorityColumn);
        this._table.addColumn(this._remoteAddressColumn);
        this._table.addColumn(this._connectionIdentifierColumn);
        this._table.addColumn(this._resourceSizeColumn);
        this._table.addColumn(this._transferSizeColumn);
        this._table.addColumn(this._timeColumn);
        this._table.addColumn(this._waterfallColumn);

        this.addSubview(this._table);
    }

    layout()
    {
        this._processPendingEntries();
    }

    handleClearShortcut(event)
    {
        this.reset();
    }

    // Private

    _processPendingEntries()
    {
        let needsSort = this._pendingUpdates.length > 0;

        // No global sort is needed, so just insert new records into their sorted position.
        if (!needsSort) {
            let originalLength = this._pendingInsertions.length;
            for (let resource of this._pendingInsertions)
                this._insertResourceAndReloadTable(resource);
            console.assert(this._pendingInsertions.length === originalLength);
            this._pendingInsertions = [];
            return;
        }

        for (let resource of this._pendingInsertions)
            this._entries.push(this._entryForResource(resource));
        this._pendingInsertions = [];

        for (let resource of this._pendingUpdates)
            this._updateEntryForResource(resource);
        this._pendingUpdates = [];

        this._updateSortAndFilteredEntries();
        this._table.reloadData();
    }

    _rowIndexForResource(resource)
    {
        return this._filteredEntries.findIndex((x) => x.resource === resource);
    }

    _updateEntryForResource(resource)
    {
        let index = this._entries.findIndex((x) => x.resource === resource);
        if (index === -1)
            return;

        let entry = this._entryForResource(resource);
        this._entries[index] = entry;

        let rowIndex = this._rowIndexForResource(resource);
        if (rowIndex === -1)
            return;

        this._filteredEntries[rowIndex] = entry;
    }

    _resourceCachingDisabledSettingChanged()
    {
        this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;
    }

    _toggleDisableResourceCache()
    {
        WI.resourceCachingDisabledSetting.value = !WI.resourceCachingDisabledSetting.value;
    }

    _mainResourceDidChange(event)
    {
        let frame = event.target;
        if (!frame.isMainFrame() || !WI.settings.clearNetworkOnNavigate.value)
            return;

        this.reset();

        this._insertResourceAndReloadTable(frame.mainResource);
    }

    _resourceLoadingDidFinish(event)
    {
        let resource = event.target;
        this._pendingUpdates.push(resource);
        this.needsLayout();
    }

    _resourceLoadingDidFail(event)
    {
        let resource = event.target;
        this._pendingUpdates.push(resource);
        this.needsLayout();
    }

    _resourceTransferSizeDidChange(event)
    {
        if (!this._table)
            return;

        let resource = event.target;

        // In the unlikely event that this is the sort column, we may need to resort.
        if (this._table.sortColumnIdentifier === "transferSize") {
            this._pendingUpdates.push(resource);
            this.needsLayout();
            return;
        }

        let index = this._entries.findIndex((x) => x.resource === resource);
        if (index === -1)
            return;

        let entry = this._entries[index];
        entry.transferSize = !isNaN(resource.networkTotalTransferSize) ? resource.networkTotalTransferSize : resource.estimatedTotalTransferSize;

        let rowIndex = this._rowIndexForResource(resource);
        if (rowIndex === -1)
            return;

        this._table.reloadCell(rowIndex, "transferSize");
    }

    _networkTimelineRecordAdded(event)
    {
        let resourceTimelineRecord = event.data.record;
        console.assert(resourceTimelineRecord instanceof WI.ResourceTimelineRecord);

        let resource = resourceTimelineRecord.resource;
        this._insertResourceAndReloadTable(resource)
    }

    _isDefaultSort()
    {
        return this._table.sortColumnIdentifier === "waterfall" && this._table.sortOrder === WI.Table.SortOrder.Ascending;
    }

    _insertResourceAndReloadTable(resource)
    {
        if (!(WI.tabBrowser.selectedTabContentView instanceof WI.NetworkTabContentView)) {
            this._pendingInsertions.push(resource);
            return;
        }

        console.assert(this._table);
        if (!this._table)
            return;

        let entry = this._entryForResource(resource);

        // Default sort has fast path.
        if (this._isDefaultSort() || !this._entriesSortComparator) {
            this._entries.push(entry);
            if (this._passFilter(entry)) {
                this._filteredEntries.push(entry);
                this._table.reloadDataAddedToEndOnly();
            }
            return;
        }

        insertObjectIntoSortedArray(entry, this._entries, this._entriesSortComparator);

        if (this._passFilter(entry)) {
            insertObjectIntoSortedArray(entry, this._filteredEntries, this._entriesSortComparator);

            // Probably a useless optimization here, but if we only added this row to the end
            // we may avoid recreating all visible rows by saying as such.
            if (this._filteredEntries.lastValue === entry)
                this._table.reloadDataAddedToEndOnly();
            else
                this._table.reloadData();
        }
    }

    _displayType(resource)
    {
        if (resource.type === WI.Resource.Type.Image || resource.type === WI.Resource.Type.Font) {
            let fileExtension;
            if (resource.mimeType)
                fileExtension = WI.fileExtensionForMIMEType(resource.mimeType);
            if (!fileExtension)
                fileExtension = WI.fileExtensionForURL(resource.url);
            if (fileExtension)
                return fileExtension;
        }

        return WI.NetworkTableContentView.shortDisplayNameForResourceType(resource.type).toLowerCase();
    }

    _entryForResource(resource)
    {
        // FIXME: <https://webkit.org/b/143632> Web Inspector: Resources with the same name in different folders aren't distinguished
        // FIXME: <https://webkit.org/b/176765> Web Inspector: Resource names should be less ambiguous

        return {
            resource,
            name: WI.displayNameForURL(resource.url, resource.urlComponents),
            domain: WI.displayNameForHost(resource.urlComponents.host),
            scheme: resource.urlComponents.scheme ? resource.urlComponents.scheme.toLowerCase() : "",
            method: resource.requestMethod,
            type: resource.type,
            displayType: this._displayType(resource),
            mimeType: resource.mimeType,
            status: resource.statusCode,
            cached: resource.cached,
            resourceSize: resource.size,
            transferSize: !isNaN(resource.networkTotalTransferSize) ? resource.networkTotalTransferSize : resource.estimatedTotalTransferSize,
            time: resource.duration,
            protocol: resource.protocol,
            priority: resource.priority,
            remoteAddress: resource.remoteAddress,
            connectionIdentifier: resource.connectionIdentifier,
            startTime: resource.firstTimestamp,
        };
    }

    _passFilter(entry)
    {
        if (!this._activeTypeFilters)
            return true;

        return this._activeTypeFilters.some((checker) => checker(entry.resource.type));
    }

    _updateSortAndFilteredEntries()
    {
        this._entries = this._entries.sort(this._entriesSortComparator);
        this._updateFilteredEntries();
    }

    _updateFilteredEntries()
    {
        if (this._activeTypeFilters)
            this._filteredEntries = this._entries.filter(this._passFilter, this);
        else
            this._filteredEntries = this._entries.slice();
    }

    _generateTypeFilter()
    {
        let selectedItems = this._typeFilterScopeBar.selectedItems;
        if (!selectedItems.length || selectedItems.includes(this._typeFilterScopeBarItemAll))
            return null;

        return selectedItems.map((item) => item.__checker);
    }

    _areFilterListsIdentical(listA, listB)
    {
        if (listA && listB) {
            if (listA.length !== listB.length)
                return false;

            for (let i = 0; i < listA.length; ++i) {
                if (listA[i] !== listB[i])
                    return false;
            }

            return true;
        }

        return false;
    }

    _typeFilterScopeBarSelectionChanged(event)
    {
        // FIXME: <https://webkit.org/b/176763> Web Inspector: ScopeBar SelectionChanged event may dispatch multiple times for a single logical change
        // We can't use shallow equals here because the contents are functions.
        let oldFilter = this._activeTypeFilters;
        let newFilter = this._generateTypeFilter();
        if (this._areFilterListsIdentical(oldFilter, newFilter))
            return;

        this._activeTypeFilters = newFilter;
        this._updateFilteredEntries();
        this._table.reloadData();
    }

    _tableNameColumnDidChangeWidth(event)
    {
        this._nameColumnWidthSetting.value = event.target.width;
    }
};
