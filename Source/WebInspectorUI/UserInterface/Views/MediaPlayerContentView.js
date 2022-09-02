/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

WI.MediaPlayerContentView = class MediaPlayerContentView extends WI.ContentView
{
    constructor(player)
    {
        console.assert(player instanceof WI.MediaPlayer);

        super(player);

        this.element.classList.add("media");

        this._sortComparator = null;
        this._table = null;

        this._playerIdentifierRow = new WI.DetailsSectionSimpleRow(WI.UIString("Identifier"));
        this._playerURLRow = new WI.DetailsSectionSimpleRow(WI.UIString("URL"));
        this._playerContentType = new WI.DetailsSectionSimpleRow(WI.UIString("Content Type"));
        this._playerDuration = new WI.DetailsSectionSimpleRow(WI.UIString("Duration"));
        this._playerEngine = new WI.DetailsSectionSimpleRow(WI.UIString("Engine"));
        this._playerGroup = new WI.DetailsSectionGroup([this._playerIdentifierRow, this._playerURLRow, this._playerContentType, this._playerDuration, this._playerEngine]);
        this._playerSection = new WI.DetailsSection("media-player-details", WI.UIString("Media Player Details"), [this._playerGroup]);
        this.element.append(this._playerSection.element);

        this._updateDetails();
        this.representedObject.addEventListener(WI.MediaPlayer.Event.PlayerDidUpdate, this._updateDetails, this);

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this.handleRefreshButtonClicked, this);

        this.representedObject.addEventListener(WI.MediaPlayer.Event.EventsDidChange, this._pushEvent, this);
    }

    // Public

    get scrollableElements()
    {
        if (!this._table)
            return [];
        return [this._table.scrollContainer];
    }

    get navigationItems()
    {
        // The toggle recording NavigationItem isn't added to the ContentBrowser's NavigationBar.
        // It's added to the "quick access" NavigationBar shown when hovering the canvas in the overview.
        return [this._refreshButtonNavigationItem];
    }

    refreshPreview()
    {
        this.needsLayout();
    }

    handleRefreshButtonClicked()
    {
        this.refreshPreview();
    }

    // Table dataSource

    tableIndexForRepresentedObject(table, object)
    {
        let index = this.representedObject.events.indexOf(object);
        console.assert(index >= 0);
        return index;
    }

    tableRepresentedObjectForIndex(table, index)
    {
        console.assert(index >= 0 && index < this.representedObject.events.length);
        return this.representedObject.events[index];
    }

    tableNumberOfRows(table)
    {
        return this.representedObject.events.length;
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

    tablePopulateCell(table, cell, column, rowIndex)
    {
        const eventObject = this.representedObject.events[rowIndex];

        if (column.identifier === 'time')
            cell.textContent = eventObject['time'].toISOString().substring(11, 23);
        else
            cell.textContent = eventObject[column.identifier];
        return cell;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._table = new WI.Table("media-events-table", this, this, 20);
        this._table.allowsMultipleSelection = true;

        this._timeColumn = new WI.TableColumn("time", WI.UIString("Time"), {
            minWidth: 70,
            maxWidth: 200,
            initialWidth: 100,
            resizeType: WI.TableColumn.ResizeType.Locked,
        });

        this._eventColumn = new WI.TableColumn("playerEvent", WI.UIString("Event"), {
            minWidth: 70,
            maxWidth: 200,
            initialWidth: 100,
            hideable: false,
            resizeType: WI.TableColumn.ResizeType.Locked,
        });

        this._dataColumn = new WI.TableColumn("playerData", WI.UIString("Data"), {
            minWidth: 100,
            maxWidth: 600,
            initialWidth: 200,
            hideable: true,
        });

        this._table.addColumn(this._timeColumn);
        this._table.addColumn(this._eventColumn);
        this._table.addColumn(this._dataColumn);

        if (!this._table.sortColumnIdentifier) {
            this._table.sortOrder = WI.Table.SortOrder.Ascending;
            this._table.sortColumnIdentifier = "time";
        }

        this.addSubview(this._table);

        this._updateEvents();
        this.tableSortChanged();
    }

    layout()
    {
        super.layout();
    }

    attached()
    {
        super.attached();

        this.refreshPreview();
    }

    detached()
    {
        super.detached();
    }

    // Private

    _generateSortComparator()
    {
        let sortColumnIdentifier = this._table.sortColumnIdentifier;
        if (!sortColumnIdentifier) {
            this._sortComparator = null;
            return;
        }

        let comparator = null;

        if (sortColumnIdentifier == 'time')
            comparator = (a, b) => ((a['time'] > b['time']) - (a['time'] < b['time']));
        else
            comparator = (a, b) => (a[sortColumnIdentifier] || "").extendedLocaleCompare(b[sortColumnIdentifier] || "");

        let reverseFactor = this._table.sortOrder === WI.Table.SortOrder.Ascending ? 1 : -1;
        this._sortComparator = (a, b) => reverseFactor * comparator(a, b);
    }

    _updateSort()
    {
        if (!this._sortComparator)
            return;

        this.representedObject.events.sort(this._sortComparator);
    }

    _showError()
    {
        // if (this._previewImageElement)
        //     this._previewImageElement.remove();

        // if (!this._errorElement)
        //     this._errorElement = WI.createMessageTextView(WI.UIString("No Preview Available"), isError);

        // if (this._previewContainerElement)
        //     this._previewContainerElement.appendChild(this._errorElement);
    }

    _updateDetails()
    {
        const player = this.representedObject;
        this._playerIdentifierRow.value = player.identifier;
        this._playerURLRow.value = player.originUrl ? WI.linkifyStringAsFragment(player.originUrl) : '';
        this._playerContentType.value = player.contentType;
        this._playerDuration.value = WI.MediaPlayer.parseMediaTime(player.duration);
        this._playerEngine.value = player.engine;
    }

    _pushEvent(event)
    {
        this._updateEvents();
    }

    _updateEvents()
    {
        this._updateSort();
        this._table.reloadData();
    }
};
