/*
 * Copyright (C) 2008, 2013-2015 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.         IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DataGrid = class DataGrid extends WebInspector.View
{
    constructor(columnsData, editCallback, deleteCallback, preferredColumnOrder)
    {
        super();

        this.columns = new Map;
        this.orderedColumns = [];

        this._sortColumnIdentifier = null;
        this._sortColumnIdentifierSetting = null;
        this._sortOrder = WebInspector.DataGrid.SortOrder.Indeterminate;
        this._sortOrderSetting = null;

        this.children = [];
        this.selectedNode = null;
        this.expandNodesWhenArrowing = false;
        this.root = true;
        this.hasChildren = false;
        this.expanded = true;
        this.revealed = true;
        this.selected = false;
        this.dataGrid = this;
        this.indentWidth = 15;
        this.resizers = [];
        this._columnWidthsInitialized = false;

        this.element.className = "data-grid";
        this.element.tabIndex = 0;
        this.element.addEventListener("keydown", this._keyDown.bind(this), false);
        this.element.copyHandler = this;

        this._headerTableElement = document.createElement("table");
        this._headerTableElement.className = "header";
        this._headerTableColumnGroupElement = this._headerTableElement.createChild("colgroup");
        this._headerTableBodyElement = this._headerTableElement.createChild("tbody");
        this._headerTableRowElement = this._headerTableBodyElement.createChild("tr");
        this._headerTableCellElements = new Map;

        this._scrollContainerElement = document.createElement("div");
        this._scrollContainerElement.className = "data-container";

        this._dataTableElement = this._scrollContainerElement.createChild("table");
        this._dataTableElement.className = "data";

        this._dataTableElement.addEventListener("mousedown", this._mouseDownInDataTable.bind(this));
        this._dataTableElement.addEventListener("click", this._clickInDataTable.bind(this));
        this._dataTableElement.addEventListener("contextmenu", this._contextMenuInDataTable.bind(this), true);

        // FIXME: Add a createCallback which is different from editCallback and has different
        // behavior when creating a new node.
        if (editCallback) {
            this._dataTableElement.addEventListener("dblclick", this._ondblclick.bind(this), false);
            this._editCallback = editCallback;
        }
        if (deleteCallback)
            this._deleteCallback = deleteCallback;

        this._dataTableColumnGroupElement = this._headerTableColumnGroupElement.cloneNode(true);
        this._dataTableElement.appendChild(this._dataTableColumnGroupElement);

        // This element is used by DataGridNodes to manipulate table rows and cells.
        this.dataTableBodyElement = this._dataTableElement.createChild("tbody");
        this._fillerRowElement = this.dataTableBodyElement.createChild("tr");
        this._fillerRowElement.className = "filler";

        this.element.appendChild(this._headerTableElement);
        this.element.appendChild(this._scrollContainerElement);

        if (preferredColumnOrder) {
            for (var columnIdentifier of preferredColumnOrder)
                this.insertColumn(columnIdentifier, columnsData[columnIdentifier]);
        } else {
            for (var columnIdentifier in columnsData)
                this.insertColumn(columnIdentifier, columnsData[columnIdentifier]);
        }
    }

    static createSortableDataGrid(columnNames, values)
    {
        var numColumns = columnNames.length;
        if (!numColumns)
            return null;

        var columnsData = {};
        for (var columnName of columnNames) {
            columnsData[columnName] = {
                width: columnName.length,
                title: columnName,
                sortable: true,
            };
        }

        var dataGrid = new WebInspector.DataGrid(columnsData, undefined, undefined, columnNames);
        for (var i = 0; i < values.length / numColumns; ++i) {
            var data = {};
            for (var j = 0; j < columnNames.length; ++j)
                data[columnNames[j]] = values[numColumns * i + j];

            var node = new WebInspector.DataGridNode(data, false);
            dataGrid.appendChild(node);
        }

        function sortDataGrid()
        {
            var sortColumnIdentifier = dataGrid.sortColumnIdentifier;

            var columnIsNumeric = true;
            for (var node of dataGrid.children) {
                var value = dataGrid.textForDataGridNodeColumn(node, sortColumnIdentifier);
                if (isNaN(Number(value)))
                    columnIsNumeric = false;
            }

            function comparator(dataGridNode1, dataGridNode2)
            {
                var item1 = dataGrid.textForDataGridNodeColumn(dataGridNode1, sortColumnIdentifier);
                var item2 = dataGrid.textForDataGridNodeColumn(dataGridNode2, sortColumnIdentifier);

                var comparison;
                if (columnIsNumeric) {
                    var number1 = parseFloat(item1);
                    var number2 = parseFloat(item2);
                    comparison = number1 < number2 ? -1 : (number1 > number2 ? 1 : 0);
                } else
                    comparison = item1 < item2 ? -1 : (item1 > item2 ? 1 : 0);

                return comparison;
            }

            dataGrid.sortNodes(comparator);
        }

        dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, sortDataGrid, this);

        dataGrid.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;
        dataGrid.sortColumnIdentifier = columnNames[0];

        return dataGrid;
    }

    get refreshCallback()
    {
        return this._refreshCallback;
    }

    set refreshCallback(refreshCallback)
    {
        this._refreshCallback = refreshCallback;
    }

    get sortOrder()
    {
        return this._sortOrder;
    }

    set sortOrder(order)
    {
        if (order === this._sortOrder)
            return;

        this._sortOrder = order;

        if (this._sortOrderSetting)
            this._sortOrderSetting.value = this._sortOrder;

        if (!this._sortColumnIdentifier)
            return;

        var sortHeaderCellElement = this._headerTableCellElements.get(this._sortColumnIdentifier);

        sortHeaderCellElement.classList.toggle(WebInspector.DataGrid.SortColumnAscendingStyleClassName, this._sortOrder === WebInspector.DataGrid.SortOrder.Ascending);
        sortHeaderCellElement.classList.toggle(WebInspector.DataGrid.SortColumnDescendingStyleClassName, this._sortOrder === WebInspector.DataGrid.SortOrder.Descending);

        this.dispatchEventToListeners(WebInspector.DataGrid.Event.SortChanged);
    }

    set sortOrderSetting(setting)
    {
        console.assert(setting instanceof WebInspector.Setting);

        this._sortOrderSetting = setting;
        if (this._sortOrderSetting.value)
            this.sortOrder = this._sortOrderSetting.value;
    }

    get sortColumnIdentifier()
    {
        return this._sortColumnIdentifier;
    }

    set sortColumnIdentifier(columnIdentifier)
    {
        console.assert(columnIdentifier && this.columns.has(columnIdentifier));
        console.assert("sortable" in this.columns.get(columnIdentifier));

        if (this._sortColumnIdentifier === columnIdentifier)
            return;

        let oldSortColumnIdentifier = this._sortColumnIdentifier;
        this._sortColumnIdentifier = columnIdentifier;
        this._updateSortedColumn(oldSortColumnIdentifier);
    }

    set sortColumnIdentifierSetting(setting)
    {
        console.assert(setting instanceof WebInspector.Setting);

        this._sortColumnIdentifierSetting = setting;
        if (this._sortColumnIdentifierSetting.value)
            this.sortColumnIdentifier = this._sortColumnIdentifierSetting.value;
    }

    _updateSortedColumn(oldSortColumnIdentifier)
    {
        if (this._sortColumnIdentifierSetting)
            this._sortColumnIdentifierSetting.value = this._sortColumnIdentifier;

        if (oldSortColumnIdentifier) {
            let oldSortHeaderCellElement = this._headerTableCellElements.get(oldSortColumnIdentifier);
            oldSortHeaderCellElement.classList.remove(WebInspector.DataGrid.SortColumnAscendingStyleClassName);
            oldSortHeaderCellElement.classList.remove(WebInspector.DataGrid.SortColumnDescendingStyleClassName);
        }

        if (this._sortColumnIdentifier) {
            let newSortHeaderCellElement = this._headerTableCellElements.get(this._sortColumnIdentifier);
            newSortHeaderCellElement.classList.toggle(WebInspector.DataGrid.SortColumnAscendingStyleClassName, this._sortOrder === WebInspector.DataGrid.SortOrder.Ascending);
            newSortHeaderCellElement.classList.toggle(WebInspector.DataGrid.SortColumnDescendingStyleClassName, this._sortOrder === WebInspector.DataGrid.SortOrder.Descending);
        }

        this.dispatchEventToListeners(WebInspector.DataGrid.Event.SortChanged);
    }

    _ondblclick(event)
    {
        if (this._editing || this._editingNode)
            return;

        this._startEditing(event.target);
    }

    _startEditingNodeAtColumnIndex(node, columnIndex)
    {
        console.assert(node, "Invalid argument: must provide DataGridNode to edit.");

        this._editing = true;
        this._editingNode = node;
        this._editingNode.select();

        var element = this._editingNode._element.children[columnIndex];
        WebInspector.startEditing(element, this._startEditingConfig(element));
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    }

    _startEditing(target)
    {
        var element = target.enclosingNodeOrSelfWithNodeName("td");
        if (!element)
            return;

        this._editingNode = this.dataGridNodeFromNode(target);
        if (!this._editingNode) {
            if (!this.placeholderNode)
                return;
            this._editingNode = this.placeholderNode;
        }

        // Force editing the 1st column when editing the placeholder node
        if (this._editingNode.isPlaceholderNode)
            return this._startEditingNodeAtColumnIndex(this._editingNode, 0);

        this._editing = true;
        WebInspector.startEditing(element, this._startEditingConfig(element));

        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    }

    _startEditingConfig(element)
    {
        return new WebInspector.EditingConfig(this._editingCommitted.bind(this), this._editingCancelled.bind(this), element.textContent);
    }

    _editingCommitted(element, newText, oldText, context, moveDirection)
    {
        var columnIdentifier = element.__columnIdentifier;
        var columnIndex = this.orderedColumns.indexOf(columnIdentifier);

        var textBeforeEditing = this._editingNode.data[columnIdentifier] || "";
        var currentEditingNode = this._editingNode;

        // Returns an object with the next node and column index to edit, and whether it
        // is an appropriate time to re-sort the table rows. When editing, we want to
        // postpone sorting until we switch rows or wrap around a row.
        function determineNextCell(valueDidChange) {
            if (moveDirection === "forward") {
                if (columnIndex < this.orderedColumns.length - 1)
                    return {shouldSort: false, editingNode: currentEditingNode, columnIndex: columnIndex + 1};

                // Continue by editing the first column of the next row if it exists.
                var nextDataGridNode = currentEditingNode.traverseNextNode(true, null, true);
                return {shouldSort: true, editingNode: nextDataGridNode || currentEditingNode, columnIndex: 0};
            }

            if (moveDirection === "backward") {
                if (columnIndex > 0)
                    return {shouldSort: false, editingNode: currentEditingNode, columnIndex: columnIndex - 1};

                var previousDataGridNode = currentEditingNode.traversePreviousNode(true, null, true);
                return {shouldSort: true, editingNode: previousDataGridNode || currentEditingNode, columnIndex: this.orderedColumns.length - 1};
            }

            // If we are not moving in any direction, then sort and stop.
            return {shouldSort: true};
        }

        function moveToNextCell(valueDidChange) {
            var moveCommand = determineNextCell.call(this, valueDidChange);
            if (moveCommand.shouldSort && this._sortAfterEditingCallback) {
                this._sortAfterEditingCallback();
                this._sortAfterEditingCallback = null;
            }
            if (moveCommand.editingNode)
                this._startEditingNodeAtColumnIndex(moveCommand.editingNode, moveCommand.columnIndex);
        }

        this._editingCancelled(element);

        // Update table's data model, and delegate to the callback to update other models.
        currentEditingNode.data[columnIdentifier] = newText.trim();
        this._editCallback(currentEditingNode, columnIdentifier, textBeforeEditing, newText, moveDirection);

        var textDidChange = textBeforeEditing.trim() !== newText.trim();
        moveToNextCell.call(this, textDidChange);
    }

    _editingCancelled(element)
    {
        console.assert(this._editingNode.element === element.enclosingNodeOrSelfWithNodeName("tr"));
        this._editing = false;
        this._editingNode = null;
    }

    autoSizeColumns(minPercent, maxPercent, maxDescentLevel)
    {
        if (minPercent)
            minPercent = Math.min(minPercent, Math.floor(100 / this.orderedColumns.length));
        var widths = {};
        // For the first width approximation, use the character length of column titles.
        for (var [identifier, column] of this.columns)
            widths[identifier] = (column["title"] || "").length;

        // Now approximate the width of each column as max(title, cells).
        var children = maxDescentLevel ? this._enumerateChildren(this, [], maxDescentLevel + 1) : this.children;
        for (var node of children) {
            for (var identifier of this.columns.keys()) {
                var text = this.textForDataGridNodeColumn(node, identifier);
                if (text.length > widths[identifier])
                    widths[identifier] = text.length;
            }
        }

        var totalColumnWidths = 0;
        for (var identifier of this.columns.keys())
            totalColumnWidths += widths[identifier];

        // Compute percentages and clamp desired widths to min and max widths.
        var recoupPercent = 0;
        for (var identifier of this.columns.keys()) {
            var width = Math.round(100 * widths[identifier] / totalColumnWidths);
            if (minPercent && width < minPercent) {
                recoupPercent += (minPercent - width);
                width = minPercent;
            } else if (maxPercent && width > maxPercent) {
                recoupPercent -= (width - maxPercent);
                width = maxPercent;
            }
            widths[identifier] = width;
        }

        // If we assigned too much width due to the above, reduce column widths.
        while (minPercent && recoupPercent > 0) {
            for (var identifier of this.columns.keys()) {
                if (widths[identifier] > minPercent) {
                    --widths[identifier];
                    --recoupPercent;
                    if (!recoupPercent)
                        break;
                }
            }
        }

        // If extra width remains after clamping widths, expand column widths.
        while (maxPercent && recoupPercent < 0) {
            for (var identifier of this.columns.keys()) {
                if (widths[identifier] < maxPercent) {
                    ++widths[identifier];
                    ++recoupPercent;
                    if (!recoupPercent)
                        break;
                }
            }
        }

        for (var [identifier, column] of this.columns)
            column["element"].style.width = widths[identifier] + "%";
        this._columnWidthsInitialized = false;
        this.needsLayout();
    }

    insertColumn(columnIdentifier, columnData, insertionIndex)
    {
        if (insertionIndex === undefined)
            insertionIndex = this.orderedColumns.length;
        insertionIndex = Number.constrain(insertionIndex, 0, this.orderedColumns.length);

        var listeners = new WebInspector.EventListenerSet(this, "DataGrid column DOM listeners");

        // Copy configuration properties instead of keeping a reference to the passed-in object.
        var column = Object.shallowCopy(columnData);
        column["listeners"] = listeners;
        column["ordinal"] = insertionIndex;
        column["columnIdentifier"] = columnIdentifier;

        this.orderedColumns.splice(insertionIndex, 0, columnIdentifier);

        for (var [identifier, existingColumn] of this.columns) {
            var ordinal = existingColumn["ordinal"];
            if (ordinal >= insertionIndex) // Also adjust the "old" column at insertion index.
                existingColumn["ordinal"] = ordinal + 1;
        }
        this.columns.set(columnIdentifier, column);

        if (column["disclosure"])
            this.disclosureColumnIdentifier = columnIdentifier;

        var headerColumnElement = document.createElement("col");
        if (column["width"])
            headerColumnElement.style.width = column["width"];
        column["element"] = headerColumnElement;
        var referenceElement = this._headerTableColumnGroupElement.children[insertionIndex];
        this._headerTableColumnGroupElement.insertBefore(headerColumnElement, referenceElement);

        var headerCellElement = document.createElement("th");
        headerCellElement.className = columnIdentifier + "-column";
        headerCellElement.columnIdentifier = columnIdentifier;
        if (column["aligned"])
            headerCellElement.classList.add(column["aligned"]);
        this._headerTableCellElements.set(columnIdentifier, headerCellElement);
        var referenceElement = this._headerTableRowElement.children[insertionIndex];
        this._headerTableRowElement.insertBefore(headerCellElement, referenceElement);

        if (column["headerView"]) {
            let headerView = column["headerView"];
            console.assert(headerView instanceof WebInspector.View);

            headerCellElement.appendChild(headerView.element);
            this.addSubview(headerView);
        } else {
            let titleElement = headerCellElement.createChild("div");
            if (column["titleDOMFragment"])
                titleElement.appendChild(column["titleDOMFragment"]);
            else
                titleElement.textContent = column["title"] || "";
        }

        if (column["sortable"]) {
            listeners.register(headerCellElement, "click", this._headerCellClicked);
            headerCellElement.classList.add(WebInspector.DataGrid.SortableColumnStyleClassName);
        }

        if (column["group"])
            headerCellElement.classList.add("column-group-" + column["group"]);

        if (column["collapsesGroup"]) {
            console.assert(column["group"] !== column["collapsesGroup"]);

            var dividerElement = headerCellElement.createChild("div");
            dividerElement.className = "divider";

            var collapseDiv = headerCellElement.createChild("div");
            collapseDiv.className = "collapser-button";
            collapseDiv.title = this._collapserButtonCollapseColumnsToolTip();
            listeners.register(collapseDiv, "mouseover", this._mouseoverColumnCollapser);
            listeners.register(collapseDiv, "mouseout", this._mouseoutColumnCollapser);
            listeners.register(collapseDiv, "click", this._clickInColumnCollapser);

            headerCellElement.collapsesGroup = column["collapsesGroup"];
            headerCellElement.classList.add("collapser");
        }

        this._headerTableColumnGroupElement.span = this.orderedColumns.length;

        var dataColumnElement = headerColumnElement.cloneNode();
        var referenceElement = this._dataTableColumnGroupElement.children[insertionIndex];
        this._dataTableColumnGroupElement.insertBefore(dataColumnElement, referenceElement);
        column["bodyElement"] = dataColumnElement;

        var fillerCellElement = document.createElement("td");
        fillerCellElement.className = columnIdentifier + "-column";
        fillerCellElement.__columnIdentifier = columnIdentifier;
        if (column["group"])
            fillerCellElement.classList.add("column-group-" + column["group"]);
        var referenceElement = this._fillerRowElement.children[insertionIndex];
        this._fillerRowElement.insertBefore(fillerCellElement, referenceElement);

        listeners.install();

        if (column["hidden"])
            this._hideColumn(columnIdentifier);
    }

    removeColumn(columnIdentifier)
    {
        console.assert(this.columns.has(columnIdentifier));
        var removedColumn = this.columns.get(columnIdentifier);
        this.columns.delete(columnIdentifier);
        this.orderedColumns.splice(this.orderedColumns.indexOf(columnIdentifier), 1);

        var removedOrdinal = removedColumn["ordinal"];
        for (var [identifier, column] of this.columns) {
            var ordinal = column["ordinal"];
            if (ordinal > removedOrdinal)
                column["ordinal"] = ordinal - 1;
        }

        removedColumn["listeners"].uninstall(true);

        if (removedColumn["disclosure"])
            this.disclosureColumnIdentifier = undefined;

        if (this.sortColumnIdentifier === columnIdentifier)
            this.sortColumnIdentifier = null;

        this._headerTableCellElements.delete(columnIdentifier);
        this._headerTableRowElement.children[removedOrdinal].remove();
        this._headerTableColumnGroupElement.children[removedOrdinal].remove();
        this._dataTableColumnGroupElement.children[removedOrdinal].remove();
        this._fillerRowElement.children[removedOrdinal].remove();

        this._headerTableColumnGroupElement.span = this.orderedColumns.length;

        for (var child of this.children)
            child.refresh();
    }

    _enumerateChildren(rootNode, result, maxLevel)
    {
        if (!rootNode.root)
            result.push(rootNode);
        if (!maxLevel)
            return;
        for (var i = 0; i < rootNode.children.length; ++i)
            this._enumerateChildren(rootNode.children[i], result, maxLevel - 1);
        return result;
    }

    // Updates the widths of the table, including the positions of the column
    // resizers.
    //
    // IMPORTANT: This function MUST be called once after the element of the
    // DataGrid is attached to its parent element and every subsequent time the
    // width of the parent element is changed in order to make it possible to
    // resize the columns.
    //
    // If this function is not called after the DataGrid is attached to its
    // parent element, then the DataGrid's columns will not be resizable.
    layout()
    {
        // Do not attempt to use offsets if we're not attached to the document tree yet.
        if (!this._columnWidthsInitialized && this.element.offsetWidth) {
            // Give all the columns initial widths now so that during a resize,
            // when the two columns that get resized get a percent value for
            // their widths, all the other columns already have percent values
            // for their widths.
            var headerTableColumnElements = this._headerTableColumnGroupElement.children;
            var tableWidth = this._dataTableElement.offsetWidth;
            var numColumns = headerTableColumnElements.length;
            for (var i = 0; i < numColumns; i++) {
                var headerCellElement = this._headerTableBodyElement.rows[0].cells[i]
                if (this._isColumnVisible(headerCellElement.columnIdentifier)) {
                    var columnWidth = headerCellElement.offsetWidth;
                    var percentWidth = ((columnWidth / tableWidth) * 100) + "%";
                    this._headerTableColumnGroupElement.children[i].style.width = percentWidth;
                    this._dataTableColumnGroupElement.children[i].style.width = percentWidth;
                } else {
                    this._headerTableColumnGroupElement.children[i].style.width = 0;
                    this._dataTableColumnGroupElement.children[i].style.width = 0;
                }
            }

            this._columnWidthsInitialized = true;
        }

        this._positionResizerElements();
        this._positionHeaderViews();
    }

    columnWidthsMap()
    {
        var result = {};
        for (var [identifier, column] of this.columns) {
            var width = this._headerTableColumnGroupElement.children[column["ordinal"]].style.width;
            result[columnIdentifier] = parseFloat(width);
        }
        return result;
    }

    applyColumnWidthsMap(columnWidthsMap)
    {
        for (var [identifier, column] of this.columns) {
            var width = (columnWidthsMap[identifier] || 0) + "%";
            var ordinal = column["ordinal"];
            this._headerTableColumnGroupElement.children[ordinal].style.width = width;
            this._dataTableColumnGroupElement.children[ordinal].style.width = width;
        }

        this.needsLayout();
    }

    _isColumnVisible(columnIdentifier)
    {
        return !this.columns.get(columnIdentifier)["hidden"];
    }

    _showColumn(columnIdentifier)
    {
        this.columns.get(columnIdentifier)["hidden"] = false;
    }

    _hideColumn(columnIdentifier)
    {
        var column = this.columns.get(columnIdentifier);
        column["hidden"] = true;

        var columnElement = column["element"];
        columnElement.style.width = 0;

        this._columnWidthsInitialized = false;
    }

    get scrollContainer()
    {
        return this._scrollContainerElement;
    }

    isScrolledToLastRow()
    {
        return this._scrollContainerElement.isScrolledToBottom();
    }

    scrollToLastRow()
    {
        this._scrollContainerElement.scrollTop = this._scrollContainerElement.scrollHeight - this._scrollContainerElement.offsetHeight;
    }

    _positionResizerElements()
    {
        var left = 0;
        var previousResizer = null;

        // Make n - 1 resizers for n columns.
        for (var i = 0; i < this.orderedColumns.length - 1; ++i) {
            // Create a new resizer if one does not exist for this column.
            if (i === this.resizers.length) {
                resizer = new WebInspector.Resizer(WebInspector.Resizer.RuleOrientation.Vertical, this);
                this.resizers[i] = resizer;
                // This resizer is associated with the column to its right.
                this.element.appendChild(resizer.element);
            }

            var resizer = this.resizers[i];

            // Get the width of the cell in the first (and only) row of the
            // header table in order to determine the width of the column, since
            // it is not possible to query a column for its width.
            left += this._headerTableBodyElement.rows[0].cells[i].offsetWidth;

            if (this._isColumnVisible(this.orderedColumns[i])) {
                resizer.element.style.removeProperty("display");
                resizer.element.style.left = left + "px";
                resizer[WebInspector.DataGrid.PreviousColumnOrdinalSymbol] = i;
                if (previousResizer)
                    previousResizer[WebInspector.DataGrid.NextColumnOrdinalSymbol] = i;
                previousResizer = resizer;
            } else {
                resizer.element.style.setProperty("display", "none");
                resizer[WebInspector.DataGrid.PreviousColumnOrdinalSymbol] = 0;
                resizer[WebInspector.DataGrid.NextColumnOrdinalSymbol] = 0;
            }
        }
        if (previousResizer)
            previousResizer[WebInspector.DataGrid.NextColumnOrdinalSymbol] = this.orderedColumns.length - 1;
    }

    _positionHeaderViews()
    {
        let visibleHeaderViews = false;
        for (let column of this.columns.values()) {
            if (column["headerView"] && !column["hidden"]) {
                visibleHeaderViews = true;
                break;
            }
        }

        if (!visibleHeaderViews)
            return;

        let left = 0;
        for (let columnIdentifier of this.orderedColumns) {
            let column = this.columns.get(columnIdentifier);
            console.assert(column, "Missing column data for header cell with columnIdentifier " + columnIdentifier);
            if (!column)
                continue;

            let columnWidth = this._headerTableCellElements.get(columnIdentifier).offsetWidth;
            let headerView = column["headerView"];
            if (headerView) {
                headerView.element.style.left = left + "px";
                headerView.element.style.width = columnWidth + "px";
                headerView.updateLayout(WebInspector.View.LayoutReason.Resize);
            }

            left += columnWidth;
        }
    }

    addPlaceholderNode()
    {
        if (this.placeholderNode)
            this.placeholderNode.makeNormal();

        var emptyData = {};
        for (var identifier of this.columns.keys())
            emptyData[identifier] = "";
        this.placeholderNode = new WebInspector.PlaceholderDataGridNode(emptyData);
        this.appendChild(this.placeholderNode);
    }

    appendChild(child)
    {
        this.insertChild(child, this.children.length);
    }

    insertChild(child, index)
    {
        console.assert(child);
        if (!child)
            return;

        console.assert(child.parent !== this);
        if (child.parent === this)
            return;

        if (child.parent)
            child.parent.removeChild(child);

        this.children.splice(index, 0, child);
        this.hasChildren = true;

        child.parent = this;
        child.dataGrid = this.dataGrid;
        child._recalculateSiblings(index);

        delete child._depth;
        delete child._revealed;
        delete child._attached;
        child._shouldRefreshChildren = true;

        var current = child.children[0];
        while (current) {
            current.dataGrid = this.dataGrid;
            delete current._depth;
            delete current._revealed;
            delete current._attached;
            current._shouldRefreshChildren = true;
            current = current.traverseNextNode(false, child, true);
        }

        if (this.expanded)
            child._attach();
    }

    removeChild(child)
    {
        console.assert(child);
        if (!child)
            return;

        console.assert(child.parent === this);
        if (child.parent !== this)
            return;

        child.deselect();
        child._detach();

        this.children.remove(child, true);

        if (child.previousSibling)
            child.previousSibling.nextSibling = child.nextSibling;
        if (child.nextSibling)
            child.nextSibling.previousSibling = child.previousSibling;

        child.dataGrid = null;
        child.parent = null;
        child.nextSibling = null;
        child.previousSibling = null;

        if (this.children.length <= 0)
            this.hasChildren = false;

        console.assert(!child.isPlaceholderNode, "Shouldn't delete the placeholder node.");
    }

    removeChildren()
    {
        for (var i = 0; i < this.children.length; ++i) {
            var child = this.children[i];
            child.deselect();
            child._detach();

            child.dataGrid = null;
            child.parent = null;
            child.nextSibling = null;
            child.previousSibling = null;
        }

        this.children = [];
        this.hasChildren = false;
    }

    removeChildrenRecursive()
    {
        var childrenToRemove = this.children;

        var child = this.children[0];
        while (child) {
            if (child.children.length)
                childrenToRemove = childrenToRemove.concat(child.children);
            child = child.traverseNextNode(false, this, true);
        }

        for (var i = 0; i < childrenToRemove.length; ++i) {
            child = childrenToRemove[i];
            child.deselect();
            child._detach();

            child.children = [];
            child.dataGrid = null;
            child.parent = null;
            child.nextSibling = null;
            child.previousSibling = null;
        }

        this.children = [];
    }

    sortNodes(comparator)
    {
        if (this._sortNodesRequestId)
            return;

        this._sortNodesRequestId = window.requestAnimationFrame(this._sortNodesCallback.bind(this, comparator));
    }

    sortNodesImmediately(comparator)
    {
        this._sortNodesCallback(comparator);
    }

    _sortNodesCallback(comparator)
    {
        function comparatorWrapper(aRow, bRow)
        {
            var aNode = aRow._dataGridNode;
            var bNode = bRow._dataGridNode;

            if (aNode.isPlaceholderNode)
                return 1;
            if (bNode.isPlaceholderNode)
                return -1;

            var reverseFactor = this.sortOrder !== WebInspector.DataGrid.SortOrder.Ascending ? -1 : 1;
            return reverseFactor * comparator(aNode, bNode);
        }

        this._sortNodesRequestId = undefined;

        if (this._editing) {
            this._sortAfterEditingCallback = this.sortNodes.bind(this, comparator);
            return;
        }

        var tbody = this.dataTableBodyElement;
        var childNodes = tbody.childNodes;
        var fillerRowElement = tbody.lastChild;

        var sortedRowElements = Array.prototype.slice.call(childNodes, 0, childNodes.length - 1);
        sortedRowElements.sort(comparatorWrapper.bind(this));

        tbody.removeChildren();

        var previousSiblingNode = null;
        for (var rowElement of sortedRowElements) {
            var node = rowElement._dataGridNode;
            node.previousSibling = previousSiblingNode;
            if (previousSiblingNode)
                previousSiblingNode.nextSibling = node;
            tbody.appendChild(rowElement);
            previousSiblingNode = node;
        }

        if (previousSiblingNode)
            previousSiblingNode.nextSibling = null;

        tbody.appendChild(fillerRowElement); // We expect to find a filler row when attaching nodes.
    }

    _keyDown(event)
    {
        if (!this.selectedNode || event.shiftKey || event.metaKey || event.ctrlKey || this._editing)
            return;

        var handled = false;
        var nextSelectedNode;
        if (event.keyIdentifier === "Up" && !event.altKey) {
            nextSelectedNode = this.selectedNode.traversePreviousNode(true);
            while (nextSelectedNode && !nextSelectedNode.selectable)
                nextSelectedNode = nextSelectedNode.traversePreviousNode(true);
            handled = nextSelectedNode ? true : false;
        } else if (event.keyIdentifier === "Down" && !event.altKey) {
            nextSelectedNode = this.selectedNode.traverseNextNode(true);
            while (nextSelectedNode && !nextSelectedNode.selectable)
                nextSelectedNode = nextSelectedNode.traverseNextNode(true);
            handled = nextSelectedNode ? true : false;
        } else if (event.keyIdentifier === "Left") {
            if (this.selectedNode.expanded) {
                if (event.altKey)
                    this.selectedNode.collapseRecursively();
                else
                    this.selectedNode.collapse();
                handled = true;
            } else if (this.selectedNode.parent && !this.selectedNode.parent.root) {
                handled = true;
                if (this.selectedNode.parent.selectable) {
                    nextSelectedNode = this.selectedNode.parent;
                    handled = nextSelectedNode ? true : false;
                } else if (this.selectedNode.parent)
                    this.selectedNode.parent.collapse();
            }
        } else if (event.keyIdentifier === "Right") {
            if (!this.selectedNode.revealed) {
                this.selectedNode.reveal();
                handled = true;
            } else if (this.selectedNode.hasChildren) {
                handled = true;
                if (this.selectedNode.expanded) {
                    nextSelectedNode = this.selectedNode.children[0];
                    handled = nextSelectedNode ? true : false;
                } else {
                    if (event.altKey)
                        this.selectedNode.expandRecursively();
                    else
                        this.selectedNode.expand();
                }
            }
        } else if (event.keyCode === 8 || event.keyCode === 46) {
            if (this._deleteCallback) {
                handled = true;
                this._deleteCallback(this.selectedNode);
            }
        } else if (isEnterKey(event)) {
            if (this._editCallback) {
                handled = true;
                this._startEditing(this.selectedNode._element.children[0]);
            }
        }

        if (nextSelectedNode) {
            nextSelectedNode.reveal();
            nextSelectedNode.select();
        }

        if (handled) {
            event.preventDefault();
            event.stopPropagation();
        }
    }

    expand()
    {
        // This is the root, do nothing.
    }

    collapse()
    {
        // This is the root, do nothing.
    }

    reveal()
    {
        // This is the root, do nothing.
    }

    revealAndSelect()
    {
        // This is the root, do nothing.
    }

    dataGridNodeFromNode(target)
    {
        var rowElement = target.enclosingNodeOrSelfWithNodeName("tr");
        return rowElement && rowElement._dataGridNode;
    }

    dataGridNodeFromPoint(x, y)
    {
        var node = this._dataTableElement.ownerDocument.elementFromPoint(x, y);
        var rowElement = node.enclosingNodeOrSelfWithNodeName("tr");
        return rowElement && rowElement._dataGridNode;
    }

    _headerCellClicked(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.columnIdentifier || !cell.classList.contains(WebInspector.DataGrid.SortableColumnStyleClassName))
            return;

        var clickedColumnIdentifier = cell.columnIdentifier;
        if (this.sortColumnIdentifier === clickedColumnIdentifier) {
            if (this.sortOrder !== WebInspector.DataGrid.SortOrder.Descending)
                this.sortOrder = WebInspector.DataGrid.SortOrder.Descending;
            else
                this.sortOrder = WebInspector.DataGrid.SortOrder.Ascending;
        } else
            this.sortColumnIdentifier = clickedColumnIdentifier;
    }

    _mouseoverColumnCollapser(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        cell.classList.add("mouse-over-collapser");
    }

    _mouseoutColumnCollapser(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        cell.classList.remove("mouse-over-collapser");
    }

    _clickInColumnCollapser(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        this._collapseColumnGroupWithCell(cell);

        event.stopPropagation();
        event.preventDefault();
    }

    collapseColumnGroup(columnGroup)
    {
        var collapserColumnIdentifier = null;
        for (var [identifier, column] of this.columns) {
            if (column["collapsesGroup"] === columnGroup) {
                collapserColumnIdentifier = identifier;
                break;
            }
        }

        console.assert(collapserColumnIdentifier);
        if (!collapserColumnIdentifier)
            return;

        var cell = this._headerTableCellElements.get(collapserColumnIdentifier);
        this._collapseColumnGroupWithCell(cell);
    }

    _collapseColumnGroupWithCell(cell)
    {
        var columnsWillCollapse = cell.classList.toggle("collapsed");

        this.willToggleColumnGroup(cell.collapsesGroup, columnsWillCollapse);

        var showOrHide = columnsWillCollapse ? this._hideColumn : this._showColumn;
        for (var [identifier, column] of this.columns) {
            if (column["group"] === cell.collapsesGroup)
                showOrHide.call(this, identifier);
        }

        var collapserButton = cell.querySelector(".collapser-button");
        if (collapserButton)
            collapserButton.title = columnsWillCollapse ? this._collapserButtonExpandColumnsToolTip() : this._collapserButtonCollapseColumnsToolTip();

        this.didToggleColumnGroup(cell.collapsesGroup, columnsWillCollapse);
    }

    _collapserButtonCollapseColumnsToolTip()
    {
        return WebInspector.UIString("Collapse columns");
    }

    _collapserButtonExpandColumnsToolTip()
    {
        return WebInspector.UIString("Expand columns");
    }

    willToggleColumnGroup(columnGroup, willCollapse)
    {
        // Implemented by subclasses if needed.
    }

    didToggleColumnGroup(columnGroup, didCollapse)
    {
        // Implemented by subclasses if needed.
    }

    headerTableHeader(columnIdentifier)
    {
        return this._headerTableCellElements.get(columnIdentifier);
    }

    _mouseDownInDataTable(event)
    {
        var gridNode = this.dataGridNodeFromNode(event.target);
        if (!gridNode || !gridNode.selectable)
            return;

        if (gridNode.isEventWithinDisclosureTriangle(event))
            return;

        if (event.metaKey) {
            if (gridNode.selected)
                gridNode.deselect();
            else
                gridNode.select();
        } else
            gridNode.select();
    }

    _contextMenuInDataTable(event)
    {
        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        let gridNode = this.dataGridNodeFromNode(event.target);
        if (this.dataGrid._refreshCallback && (!gridNode || gridNode !== this.placeholderNode))
            contextMenu.appendItem(WebInspector.UIString("Refresh"), this._refreshCallback.bind(this));

        if (gridNode && gridNode.selectable && gridNode.copyable && !gridNode.isEventWithinDisclosureTriangle(event)) {
            contextMenu.appendItem(WebInspector.UIString("Copy Row"), this._copyRow.bind(this, event.target));

            if (this.dataGrid._editCallback) {
                if (gridNode === this.placeholderNode)
                    contextMenu.appendItem(WebInspector.UIString("Add New"), this._startEditing.bind(this, event.target));
                else {
                    let element = event.target.enclosingNodeOrSelfWithNodeName("td");
                    let columnIdentifier = element.__columnIdentifier;
                    let columnTitle = this.dataGrid.columns.get(columnIdentifier)["title"];
                    contextMenu.appendItem(WebInspector.UIString("Edit “%s”").format(columnTitle), this._startEditing.bind(this, event.target));
                }
            }
            if (this.dataGrid._deleteCallback && gridNode !== this.placeholderNode)
                contextMenu.appendItem(WebInspector.UIString("Delete"), this._deleteCallback.bind(this, gridNode));
        }
    }

    _clickInDataTable(event)
    {
        var gridNode = this.dataGridNodeFromNode(event.target);
        if (!gridNode || !gridNode.hasChildren)
            return;

        if (!gridNode.isEventWithinDisclosureTriangle(event))
            return;

        if (gridNode.expanded) {
            if (event.altKey)
                gridNode.collapseRecursively();
            else
                gridNode.collapse();
        } else {
            if (event.altKey)
                gridNode.expandRecursively();
            else
                gridNode.expand();
        }
    }

    textForDataGridNodeColumn(node, columnIdentifier)
    {
        var data = node.data[columnIdentifier];
        return (data instanceof Node ? data.textContent : data) || "";
    }

    _copyTextForDataGridNode(node)
    {
        var fields = [];
        for (var identifier of node.dataGrid.orderedColumns)
            fields.push(this.textForDataGridNodeColumn(node, identifier));

        var tabSeparatedValues = fields.join("\t");
        return tabSeparatedValues;
    }

    handleBeforeCopyEvent(event)
    {
        if (this.selectedNode && window.getSelection().isCollapsed)
            event.preventDefault();
    }

    handleCopyEvent(event)
    {
        if (!this.selectedNode || !window.getSelection().isCollapsed)
            return;

        var copyText = this._copyTextForDataGridNode(this.selectedNode);
        event.clipboardData.setData("text/plain", copyText);
        event.stopPropagation();
        event.preventDefault();
    }

    _copyRow(target)
    {
        var gridNode = this.dataGridNodeFromNode(target);
        if (!gridNode)
            return;

        var copyText = this._copyTextForDataGridNode(gridNode);
        InspectorFrontendHost.copyText(copyText);
    }

    get resizeMethod()
    {
        if (!this._resizeMethod)
            return WebInspector.DataGrid.ResizeMethod.Nearest;
        return this._resizeMethod;
    }

    set resizeMethod(method)
    {
        this._resizeMethod = method;
    }

    resizerDragStarted(resizer)
    {
        if (!resizer[WebInspector.DataGrid.NextColumnOrdinalSymbol])
            return true; // Abort the drag;

        this._currentResizer = resizer;
    }

    resizerDragging(resizer, positionDelta)
    {
        console.assert(resizer === this._currentResizer, resizer, this._currentResizer);
        if (resizer != this._currentResizer)
            return;

        // Constrain the dragpoint to be within the containing div of the
        // datagrid.
        var dragPoint = (resizer.initialPosition - positionDelta) - this.element.totalOffsetLeft;
        // Constrain the dragpoint to be within the space made up by the
        // column directly to the left and the column directly to the right.
        var leftCellIndex = resizer[WebInspector.DataGrid.PreviousColumnOrdinalSymbol];
        var rightCellIndex = resizer[WebInspector.DataGrid.NextColumnOrdinalSymbol];
        var firstRowCells = this._headerTableBodyElement.rows[0].cells;
        var leftEdgeOfPreviousColumn = 0;
        for (var i = 0; i < leftCellIndex; i++)
            leftEdgeOfPreviousColumn += firstRowCells[i].offsetWidth;

        // Differences for other resize methods
        if (this.resizeMethod === WebInspector.DataGrid.ResizeMethod.Last) {
            rightCellIndex = this.resizers.length;
        } else if (this.resizeMethod === WebInspector.DataGrid.ResizeMethod.First) {
            leftEdgeOfPreviousColumn += firstRowCells[leftCellIndex].offsetWidth - firstRowCells[0].offsetWidth;
            leftCellIndex = 0;
        }

        var rightEdgeOfNextColumn = leftEdgeOfPreviousColumn + firstRowCells[leftCellIndex].offsetWidth + firstRowCells[rightCellIndex].offsetWidth;

        // Give each column some padding so that they don't disappear.
        var leftMinimum = leftEdgeOfPreviousColumn + this.ColumnResizePadding;
        var rightMaximum = rightEdgeOfNextColumn - this.ColumnResizePadding;

        dragPoint = Number.constrain(dragPoint, leftMinimum, rightMaximum);

        resizer.element.style.left = (dragPoint - this.CenterResizerOverBorderAdjustment) + "px";

        var percentLeftColumn = (((dragPoint - leftEdgeOfPreviousColumn) / this._dataTableElement.offsetWidth) * 100) + "%";
        this._headerTableColumnGroupElement.children[leftCellIndex].style.width = percentLeftColumn;
        this._dataTableColumnGroupElement.children[leftCellIndex].style.width = percentLeftColumn;

        var percentRightColumn = (((rightEdgeOfNextColumn - dragPoint) / this._dataTableElement.offsetWidth) * 100) + "%";
        this._headerTableColumnGroupElement.children[rightCellIndex].style.width =  percentRightColumn;
        this._dataTableColumnGroupElement.children[rightCellIndex].style.width = percentRightColumn;

        this._positionResizerElements();
        this._positionHeaderViews();
        event.preventDefault();
    }

    resizerDragEnded(resizer)
    {
        console.assert(resizer === this._currentResizer, resizer, this._currentResizer);
        if (resizer != this._currentResizer)
            return;

        this._currentResizer = null;
    }
};

WebInspector.DataGrid.Event = {
    SortChanged: "datagrid-sort-changed",
    SelectedNodeChanged: "datagrid-selected-node-changed",
    ExpandedNode: "datagrid-expanded-node",
    CollapsedNode: "datagrid-collapsed-node",
    GoToArrowClicked: "datagrid-go-to-arrow-clicked"
};

WebInspector.DataGrid.ResizeMethod = {
    Nearest: "nearest",
    First: "first",
    Last: "last"
};

WebInspector.DataGrid.SortOrder = {
    Indeterminate: "data-grid-sort-order-indeterminate",
    Ascending: "data-grid-sort-order-ascending",
    Descending: "data-grid-sort-order-descending"
};

WebInspector.DataGrid.PreviousColumnOrdinalSymbol = Symbol("previous-column-ordinal");
WebInspector.DataGrid.NextColumnOrdinalSymbol = Symbol("next-column-ordinal");

WebInspector.DataGrid.ColumnResizePadding = 10;
WebInspector.DataGrid.CenterResizerOverBorderAdjustment = 3;

WebInspector.DataGrid.SortColumnAscendingStyleClassName = "sort-ascending";
WebInspector.DataGrid.SortColumnDescendingStyleClassName = "sort-descending";
WebInspector.DataGrid.SortableColumnStyleClassName = "sortable";

WebInspector.DataGridNode = class DataGridNode extends WebInspector.Object
{
    constructor(data, hasChildren)
    {
        super();

        this._expanded = false;
        this._hidden = false;
        this._selected = false;
        this._copyable = true;
        this._shouldRefreshChildren = true;
        this._data = data || {};
        this.hasChildren = hasChildren || false;
        this.children = [];
        this.dataGrid = null;
        this.parent = null;
        this.previousSibling = null;
        this.nextSibling = null;
        this.disclosureToggleWidth = 10;
    }

    get hidden()
    {
        return this._hidden;
    }

    set hidden(x)
    {
        x = !!x;

        if (this._hidden === x)
            return;

        this._hidden = x;
        if (this._element)
            this._element.classList.toggle("hidden", this._hidden);
    }

    get selectable()
    {
        return this._element && !this._hidden;
    }

    get copyable()
    {
        return this._copyable;
    }

    set copyable(x)
    {
        this._copyable = x;
    }

    get element()
    {
        if (this._element)
            return this._element;

        if (!this.dataGrid)
            return null;

        this._element = document.createElement("tr");
        this._element._dataGridNode = this;

        if (this.hasChildren)
            this._element.classList.add("parent");
        if (this.expanded)
            this._element.classList.add("expanded");
        if (this.selected)
            this._element.classList.add("selected");
        if (this.revealed)
            this._element.classList.add("revealed");
        if (this._hidden)
            this._element.classList.add("hidden");

        this.createCells();
        return this._element;
    }

    createCells()
    {
        for (var columnIdentifier of this.dataGrid.orderedColumns)
            this._element.appendChild(this.createCell(columnIdentifier));
    }

    createGoToArrowButton(cellElement)
    {
        function buttonClicked(event)
        {
            if (this.hidden || !this.revealed)
                return;

            event.stopPropagation();

            let columnIdentifier = cellElement.__columnIdentifier;
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.GoToArrowClicked, {dataGridNode: this, columnIdentifier});
        }

        let button = WebInspector.createGoToArrowButton();
        button.addEventListener("click", buttonClicked.bind(this));

        let contentElement = cellElement.firstChild;
        contentElement.appendChild(button);
    }

    refreshIfNeeded()
    {
        if (!this._needsRefresh)
            return;

        this._needsRefresh = false;

        this.refresh();
    }

    needsRefresh()
    {
        this._needsRefresh = true;

        if (!this._revealed)
            return;

        if (this._scheduledRefreshIdentifier)
            return;

        this._scheduledRefreshIdentifier = requestAnimationFrame(this.refresh.bind(this));
    }

    get data()
    {
        return this._data;
    }

    set data(x)
    {
        console.assert(typeof x === "object", "Data should be an object.");

        x = x || {};

        if (Object.shallowEqual(this._data, x))
            return;

        this._data = x;
        this.needsRefresh();
    }

    get revealed()
    {
        if ("_revealed" in this)
            return this._revealed;

        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded) {
                this._revealed = false;
                return false;
            }

            currentAncestor = currentAncestor.parent;
        }

        this._revealed = true;
        return true;
    }

    set hasChildren(x)
    {
        if (this._hasChildren === x)
            return;

        this._hasChildren = x;

        if (!this._element)
            return;

        if (this._hasChildren)
        {
            this._element.classList.add("parent");
            if (this.expanded)
                this._element.classList.add("expanded");
        }
        else
        {
            this._element.classList.remove("parent", "expanded");
        }
    }

    get hasChildren()
    {
        return this._hasChildren;
    }

    set revealed(x)
    {
        if (this._revealed === x)
            return;

        this._revealed = x;

        if (this._element) {
            if (this._revealed)
                this._element.classList.add("revealed");
            else
                this._element.classList.remove("revealed");
        }

        this.refreshIfNeeded();

        for (var i = 0; i < this.children.length; ++i)
            this.children[i].revealed = x && this.expanded;
    }

    get depth()
    {
        if ("_depth" in this)
            return this._depth;
        if (this.parent && !this.parent.root)
            this._depth = this.parent.depth + 1;
        else
            this._depth = 0;
        return this._depth;
    }

    get leftPadding()
    {
        if (typeof(this._leftPadding) === "number")
            return this._leftPadding;

        this._leftPadding = this.depth * this.dataGrid.indentWidth;
        return this._leftPadding;
    }

    get shouldRefreshChildren()
    {
        return this._shouldRefreshChildren;
    }

    set shouldRefreshChildren(x)
    {
        this._shouldRefreshChildren = x;
        if (x && this.expanded)
            this.expand();
    }

    get selected()
    {
        return this._selected;
    }

    set selected(x)
    {
        if (x)
            this.select();
        else
            this.deselect();
    }

    get expanded()
    {
        return this._expanded;
    }

    set expanded(x)
    {
        if (x)
            this.expand();
        else
            this.collapse();
    }

    refresh()
    {
        if (!this._element || !this.dataGrid)
            return;

        if (this._scheduledRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledRefreshIdentifier);
            this._scheduledRefreshIdentifier = undefined;
        }

        this._needsRefresh = false;

        this._element.removeChildren();
        this.createCells();
    }

    updateLayout()
    {
        // Implemented by subclasses if needed.
    }

    createCell(columnIdentifier)
    {
        var cellElement = document.createElement("td");
        cellElement.className = columnIdentifier + "-column";
        cellElement.__columnIdentifier = columnIdentifier;

        var column = this.dataGrid.columns.get(columnIdentifier);

        if (column["aligned"])
            cellElement.classList.add(column["aligned"]);

        if (column["group"])
            cellElement.classList.add("column-group-" + column["group"]);

        var div = cellElement.createChild("div");
        var content = this.createCellContent(columnIdentifier, cellElement);
        div.append(content);

        if (column["icon"]) {
            let iconElement = document.createElement("div");
            iconElement.classList.add("icon");
            div.insertBefore(iconElement, div.firstChild);
        }

        if (columnIdentifier === this.dataGrid.disclosureColumnIdentifier) {
            cellElement.classList.add("disclosure");
            if (this.leftPadding)
                cellElement.style.setProperty("padding-left", this.leftPadding + "px");
        }

        return cellElement;
    }

    createCellContent(columnIdentifier)
    {
        return this.data[columnIdentifier] || "\u200b"; // Zero width space to keep the cell from collapsing.
    }

    elementWithColumnIdentifier(columnIdentifier)
    {
        var index = this.dataGrid.orderedColumns.indexOf(columnIdentifier);
        if (index === -1)
            return null;

        return this._element.children[index];
    }

    // Share these functions with DataGrid. They are written to work with a DataGridNode this object.
    appendChild() { return WebInspector.DataGrid.prototype.appendChild.apply(this, arguments); }
    insertChild() { return WebInspector.DataGrid.prototype.insertChild.apply(this, arguments); }
    removeChild() { return WebInspector.DataGrid.prototype.removeChild.apply(this, arguments); }
    removeChildren() { return WebInspector.DataGrid.prototype.removeChildren.apply(this, arguments); }
    removeChildrenRecursive() { return WebInspector.DataGrid.prototype.removeChildrenRecursive.apply(this, arguments); }

    _recalculateSiblings(myIndex)
    {
        if (!this.parent)
            return;

        var previousChild = (myIndex > 0 ? this.parent.children[myIndex - 1] : null);

        if (previousChild) {
            previousChild.nextSibling = this;
            this.previousSibling = previousChild;
        } else
            this.previousSibling = null;

        var nextChild = this.parent.children[myIndex + 1];

        if (nextChild) {
            nextChild.previousSibling = this;
            this.nextSibling = nextChild;
        } else
            this.nextSibling = null;
    }

    collapse()
    {
        if (this._element)
            this._element.classList.remove("expanded");

        this._expanded = false;

        for (var i = 0; i < this.children.length; ++i)
            this.children[i].revealed = false;

        this.dispatchEventToListeners("collapsed");

        if (this.dataGrid)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.CollapsedNode, {dataGridNode: this});
    }

    collapseRecursively()
    {
        var item = this;
        while (item) {
            if (item.expanded)
                item.collapse();
            item = item.traverseNextNode(false, this, true);
        }
    }

    expand()
    {
        if (!this.hasChildren || this.expanded)
            return;

        if (this.revealed && !this._shouldRefreshChildren)
            for (var i = 0; i < this.children.length; ++i)
                this.children[i].revealed = true;

        if (this._shouldRefreshChildren) {
            for (var i = 0; i < this.children.length; ++i)
                this.children[i]._detach();

            this.dispatchEventToListeners("populate");

            if (this._attached) {
                for (var i = 0; i < this.children.length; ++i) {
                    var child = this.children[i];
                    if (this.revealed)
                        child.revealed = true;
                    child._attach();
                }
            }

            this._shouldRefreshChildren = false;
        }

        if (this._element)
            this._element.classList.add("expanded");

        this._expanded = true;

        this.dispatchEventToListeners("expanded");

        if (this.dataGrid)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.ExpandedNode, {dataGridNode: this});
    }

    expandRecursively()
    {
        var item = this;
        while (item) {
            item.expand();
            item = item.traverseNextNode(false, this);
        }
    }

    reveal()
    {
        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded)
                currentAncestor.expand();
            currentAncestor = currentAncestor.parent;
        }

        this.element.scrollIntoViewIfNeeded(false);

        this.dispatchEventToListeners("revealed");
    }

    select(suppressSelectedEvent)
    {
        if (!this.dataGrid || !this.selectable || this.selected)
            return;

        if (this.dataGrid.selectedNode)
            this.dataGrid.selectedNode.deselect(true);

        this._selected = true;
        this.dataGrid.selectedNode = this;

        if (this._element)
            this._element.classList.add("selected");

        if (!suppressSelectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged);
    }

    revealAndSelect()
    {
        this.reveal();
        this.select();
    }

    deselect(suppressDeselectedEvent)
    {
        if (!this.dataGrid || this.dataGrid.selectedNode !== this || !this.selected)
            return;

        this._selected = false;
        this.dataGrid.selectedNode = null;

        if (this._element)
            this._element.classList.remove("selected");

        if (!suppressDeselectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged);
    }

    traverseNextNode(skipHidden, stayWithin, dontPopulate, info)
    {
        if (!dontPopulate && this.hasChildren)
            this.dispatchEventToListeners("populate");

        if (info)
            info.depthChange = 0;

        var node = (!skipHidden || this.revealed) ? this.children[0] : null;
        if (node && (!skipHidden || this.expanded)) {
            if (info)
                info.depthChange = 1;
            return node;
        }

        if (this === stayWithin)
            return null;

        node = (!skipHidden || this.revealed) ? this.nextSibling : null;
        if (node)
            return node;

        node = this;
        while (node && !node.root && !((!skipHidden || node.revealed) ? node.nextSibling : null) && node.parent !== stayWithin) {
            if (info)
                info.depthChange -= 1;
            node = node.parent;
        }

        if (!node)
            return null;

        return (!skipHidden || node.revealed) ? node.nextSibling : null;
    }

    traversePreviousNode(skipHidden, dontPopulate)
    {
        var node = (!skipHidden || this.revealed) ? this.previousSibling : null;
        if (!dontPopulate && node && node.hasChildren)
            node.dispatchEventToListeners("populate");

        while (node && ((!skipHidden || (node.revealed && node.expanded)) ? node.children.lastValue : null)) {
            if (!dontPopulate && node.hasChildren)
                node.dispatchEventToListeners("populate");
            node = ((!skipHidden || (node.revealed && node.expanded)) ? node.children.lastValue : null);
        }

        if (node)
            return node;

        if (!this.parent || this.parent.root)
            return null;

        return this.parent;
    }

    isEventWithinDisclosureTriangle(event)
    {
        if (!this.hasChildren)
            return false;
        let cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell.classList.contains("disclosure"))
            return false;

        let computedLeftPadding = window.getComputedStyle(cell).getPropertyCSSValue("padding-left").getFloatValue(CSSPrimitiveValue.CSS_PX);
        let left = cell.totalOffsetLeft + computedLeftPadding;
        return event.pageX >= left && event.pageX <= left + this.disclosureToggleWidth;
    }

    _attach()
    {
        if (!this.dataGrid || this._attached)
            return;

        this._attached = true;

        var nextElement = null;

        if (!this.isPlaceholderNode) {
            var previousGridNode = this.traversePreviousNode(true, true);
            if (previousGridNode && previousGridNode.element.parentNode)
                nextElement = previousGridNode.element.nextSibling;
            else if (!previousGridNode)
                nextElement = this.dataGrid.dataTableBodyElement.firstChild;
        }

        // If there is no next grid node, then append before the last child since the last child is the filler row.
        console.assert(this.dataGrid.dataTableBodyElement.lastChild.classList.contains("filler"));

        if (!nextElement)
            nextElement = this.dataGrid.dataTableBodyElement.lastChild;

        this.dataGrid.dataTableBodyElement.insertBefore(this.element, nextElement);

        if (this.expanded)
            for (var i = 0; i < this.children.length; ++i)
                this.children[i]._attach();
    }

    _detach()
    {
        if (!this._attached)
            return;

        this._attached = false;

        if (this._element && this._element.parentNode)
            this._element.parentNode.removeChild(this._element);

        for (var i = 0; i < this.children.length; ++i)
            this.children[i]._detach();
    }

    savePosition()
    {
        if (this._savedPosition)
            return;

        console.assert(this.parent);
        if (!this.parent)
            return;

        this._savedPosition = {
            parent: this.parent,
            index: this.parent.children.indexOf(this)
        };
    }

    restorePosition()
    {
        if (!this._savedPosition)
            return;

        if (this.parent !== this._savedPosition.parent)
            this._savedPosition.parent.insertChild(this, this._savedPosition.index);

        this._savedPosition = null;
    }
};

// Used to create a new table row when entering new data by editing cells.
WebInspector.PlaceholderDataGridNode = class PlaceholderDataGridNode extends WebInspector.DataGridNode
{
    constructor(data)
    {
        super(data, false);
        this.isPlaceholderNode = true;
    }

    makeNormal()
    {
        this.isPlaceholderNode = false;
    }
};
