/*
 * Copyright (C) 2008, 2013, 2014 Apple Inc. All Rights Reserved.
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

WebInspector.DataGrid = function(columnsData, editCallback, deleteCallback)
{
    this.columns = new Map;
    this.orderedColumns = [];

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
    this.resizerElements = [];
    this._columnWidthsInitialized = false;

    this.element = document.createElement("div");
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

    for (var columnIdentifier in columnsData)
        this.insertColumn(columnIdentifier, columnsData[columnIdentifier]);

    this._generateSortIndicatorImagesIfNeeded();
}

WebInspector.DataGrid.Event = {
    DidLayout: "datagrid-did-layout",
    SortChanged: "datagrid-sort-changed",
    SelectedNodeChanged: "datagrid-selected-node-changed",
    ExpandedNode: "datagrid-expanded-node",
    CollapsedNode: "datagrid-collapsed-node"
};

/**
 * @param {Array.<string>} columnNames
 * @param {Array.<string>} values
 */
WebInspector.DataGrid.createSortableDataGrid = function(columnNames, values)
{
    var numColumns = columnNames.length;
    if (!numColumns)
        return null;

    var columnsData = {};

    for (var columnName of columnNames) {
        var column = {};
        column.width = columnName.length;
        column.title = columnName;
        column.sortable = true;

        columnsData[columnName] = column;
    }

    var dataGrid = new WebInspector.DataGrid(columnsData);
    for (var i = 0; i < values.length / numColumns; ++i) {
        var data = {};
        for (var j = 0; j < columnNames.length; ++j)
            data[columnNames[j]] = values[numColumns * i + j];

        var node = new WebInspector.DataGridNode(data, false);
        node.selectable = false;
        dataGrid.appendChild(node);
    }

    function sortDataGrid()
    {
        var sortColumnIdentifier = dataGrid.sortColumnIdentifier;
        var sortAscending = dataGrid.sortOrder === "ascending" ? 1 : -1;

        for (var node of dataGrid.children) {
            if (isNaN(Number(node.data[sortColumnIdentifier] || "")))
                columnIsNumeric = false;
        }

        function comparator(dataGridNode1, dataGridNode2)
        {
            var item1 = dataGridNode1.data[sortColumnIdentifier] || "";
            var item2 = dataGridNode2.data[sortColumnIdentifier] || "";

            var comparison;
            if (columnIsNumeric) {
                // Sort numbers based on comparing their values rather than a lexicographical comparison.
                var number1 = parseFloat(item1);
                var number2 = parseFloat(item2);
                comparison = number1 < number2 ? -1 : (number1 > number2 ? 1 : 0);
            } else
                comparison = item1 < item2 ? -1 : (item1 > item2 ? 1 : 0);

            return sortDirection * comparison;
        }

        dataGrid.sortNodes(comparator);
    }

    dataGrid.addEventListener(WebInspector.DataGrid.Event.SortChanged, sortDataGrid, this);
    return dataGrid;
}

WebInspector.DataGrid.prototype = {
    get refreshCallback()
    {
        return this._refreshCallback;
    },

    set refreshCallback(refreshCallback)
    {
        this._refreshCallback = refreshCallback;
    },

    _ondblclick: function(event)
    {
        if (this._editing || this._editingNode)
            return;

        this._startEditing(event.target);
    },

    _startEditingNodeAtColumnIndex: function(node, columnIndex)
    {
        console.assert(node, "Invalid argument: must provide DataGridNode to edit.");

        this._editing = true;
        this._editingNode = node;
        this._editingNode.select();

        var element = this._editingNode._element.children[columnIndex];
        WebInspector.startEditing(element, this._startEditingConfig(element));
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },

    _startEditing: function(target)
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
    },

    _startEditingConfig: function(element)
    {
        return new WebInspector.EditingConfig(this._editingCommitted.bind(this), this._editingCancelled.bind(this), element.textContent);
    },

    _editingCommitted: function(element, newText, oldText, context, moveDirection)
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

            // If we are not moving in any direction, then sort but don't move.
            return {shouldSort: true, editingNode: currentEditingNode, columnIndex: columnIndex};
        }

        function moveToNextCell(valueDidChange) {
            var moveCommand = determineNextCell.call(this, valueDidChange);
            if (moveCommand.shouldSort && this._sortAfterEditingCallback) {
                this._sortAfterEditingCallback();
                delete this._sortAfterEditingCallback;
            }
            this._startEditingNodeAtColumnIndex(moveCommand.editingNode, moveCommand.columnIndex);
        }

        this._editingCancelled(element);

        // Update table's data model, and delegate to the callback to update other models.
        currentEditingNode.data[columnIdentifier] = newText.trim();
        this._editCallback(currentEditingNode, columnIdentifier, textBeforeEditing, newText, moveDirection);

        var textDidChange = textBeforeEditing.trim() !== newText.trim();
        moveToNextCell.call(this, textDidChange);
    },

    _editingCancelled: function(element)
    {
        console.assert(this._editingNode.element === element.enclosingNodeOrSelfWithNodeName("tr"));
        delete this._editing;
        this._editingNode = null;
    },

    get sortColumnIdentifier()
    {
        return this._sortColumnCell ? this._sortColumnCell.columnIdentifier : null;
    },

    get sortOrder()
    {
        if (!this._sortColumnCell || this._sortColumnCell.classList.contains("sort-ascending"))
            return "ascending";
        if (this._sortColumnCell.classList.contains("sort-descending"))
            return "descending";
        return null;
    },

    autoSizeColumns: function(minPercent, maxPercent, maxDescentLevel)
    {
        if (minPercent)
            minPercent = Math.min(minPercent, Math.floor(100 / this.orderedColumns.length));
        var widths = {};
        // For the first width approximation, use the character length of column titles.
        for (var [identifier, column] of this.columns)
            widths[identifier] = column.get("title", "").length;

        // Now approximate the width of each column as max(title, cells).
        var children = maxDescentLevel ? this._enumerateChildren(this, [], maxDescentLevel + 1) : this.children;
        for (var node of children) {
            for (var identifier of this.columns.keys()) {
                var text = node.data[identifier] || "";
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
            column.get("element").style.width = widths[identifier] + "%";
        this._columnWidthsInitialized = false;
        this.updateLayout();
    },

    insertColumn: function(columnIdentifier, columnData, insertionIndex) {
        if (typeof insertionIndex === "undefined")
            insertionIndex = this.orderedColumns.length;
        insertionIndex = Number.constrain(insertionIndex, 0, this.orderedColumns.length);

        var listeners = new WebInspector.EventListenerSet(this, "DataGrid column DOM listeners");

        // Copy configuration properties instead of keeping a reference to the passed-in object.
        var column = new Map;
        for (var propertyName in columnData)
            column.set(propertyName, columnData[propertyName]);

        column.set("listeners", listeners);
        column.set("ordinal", insertionIndex);
        column.set("columnIdentifier", columnIdentifier);
        this.orderedColumns.splice(insertionIndex, 0, columnIdentifier);

        for (var [identifier, existingColumn] of this.columns) {
            var ordinal = existingColumn.get("ordinal");
            if (ordinal >= insertionIndex) // Also adjust the "old" column at insertion index.
                existingColumn.set("ordinal", ordinal + 1);
        }
        this.columns.set(columnIdentifier, column);

        if (column.has("disclosure"))
            this.disclosureColumnIdentifier = columnIdentifier;

        var headerColumnElement = document.createElement("col");
        if (column.has("width"))
            headerColumnElement.style.width = column.get("width");
        column.set("element", headerColumnElement);
        var referenceElement = this._headerTableColumnGroupElement.children[insertionIndex];
        this._headerTableColumnGroupElement.insertBefore(headerColumnElement, referenceElement);

        var headerCellElement = document.createElement("th");
        headerCellElement.className = columnIdentifier + "-column";
        headerCellElement.columnIdentifier = columnIdentifier;
        if (column.has("aligned"))
            headerCellElement.classList.add(column.get("aligned"));
        this._headerTableCellElements.set(columnIdentifier, headerCellElement);
        var referenceElement = this._headerTableRowElement.children[insertionIndex];
        this._headerTableRowElement.insertBefore(headerCellElement, referenceElement);

        var div = headerCellElement.createChild("div");
        if (column.has("titleDOMFragment"))
            div.appendChild(column.get("titleDOMFragment"));
        else
            div.textContent = column.get("title", "");

        if (column.has("sort")) {
            headerCellElement.classList.add("sort-" + column.get("sort"));
            this._sortColumnCell = headerCellElement;
        }

        if (column.has("sortable")) {
            listeners.register(headerCellElement, "click", this._clickInHeaderCell, false);
            headerCellElement.classList.add("sortable");
        }

        if (column.has("group"))
            headerCellElement.classList.add("column-group-" + column.get("group"));

        if (column.has("collapsesGroup")) {
            console.assert(column.get("group") !== column.get("collapsesGroup"));

            var dividerElement = headerCellElement.createChild("div");
            dividerElement.className = "divider";

            var collapseDiv = headerCellElement.createChild("div");
            collapseDiv.className = "collapser-button";
            collapseDiv.title = this._collapserButtonCollapseColumnsToolTip();
            listeners.register(collapseDiv, "mouseover", this._mouseoverColumnCollapser);
            listeners.register(collapseDiv, "mouseout", this._mouseoutColumnCollapser);
            listeners.register(collapseDiv, "click", this._clickInColumnCollapser);

            headerCellElement.collapsesGroup = column.get("collapsesGroup");
            headerCellElement.classList.add("collapser");
        }

        this._headerTableColumnGroupElement.span = this.orderedColumns.length;

        var dataColumnElement = headerColumnElement.cloneNode();
        var referenceElement = this._dataTableColumnGroupElement.children[insertionIndex];
        this._dataTableColumnGroupElement.insertBefore(dataColumnElement, referenceElement);
        column.set("bodyElement", dataColumnElement);

        var fillerCellElement = document.createElement("td");
        fillerCellElement.className = columnIdentifier + "-column";
        fillerCellElement.__columnIdentifier = columnIdentifier;
        if (column.has("group"))
            fillerCellElement.classList.add("column-group-" + column.get("group"));
        var referenceElement = this._fillerRowElement.children[insertionIndex];
        this._fillerRowElement.insertBefore(fillerCellElement, referenceElement);

        listeners.install();

        if (column.has("hidden"))
            this._hideColumn(columnIdentifier);
    },

    removeColumn: function(columnIdentifier)
    {
        console.assert(this.columns.has(columnIdentifier));
        var removedColumn = this.columns.get(columnIdentifier);
        this.columns.delete(columnIdentifier);
        this.orderedColumns.splice(this.orderedColumns.indexOf(columnIdentifier), 1);

        var removedOrdinal = removedColumn.get("ordinal");
        for (var [identifier, column] of this.columns) {
            var ordinal = column.get("ordinal");
            if (ordinal > removedOrdinal)
                column.set("ordinal", ordinal - 1);
        }

        removedColumn.get("listeners").uninstall(true);

        if (removedColumn.has("disclosure"))
            delete this.disclosureColumnIdentifier;

        if (removedColumn.has("sort"))
            delete this._sortColumnCell;

        this._headerTableCellElements.delete(columnIdentifier);
        this._headerTableRowElement.children[removedOrdinal].remove();
        this._headerTableColumnGroupElement.children[removedOrdinal].remove();
        this._dataTableColumnGroupElement.children[removedOrdinal].remove();
        this._fillerRowElement.children[removedOrdinal].remove();

        this._headerTableColumnGroupElement.span = this.orderedColumns.length;

        for (var child of this.children)
            child.refresh();
    },

    _enumerateChildren: function(rootNode, result, maxLevel)
    {
        if (!rootNode.root)
            result.push(rootNode);
        if (!maxLevel)
            return;
        for (var i = 0; i < rootNode.children.length; ++i)
            this._enumerateChildren(rootNode.children[i], result, maxLevel - 1);
        return result;
    },

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
    updateLayout: function()
    {
        // Do not attempt to use offsetes if we're not attached to the document tree yet.
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
        this.dispatchEventToListeners(WebInspector.DataGrid.Event.DidLayout);
    },

    columnWidthsMap: function()
    {
        var result = {};
        for (var [identifier, column] of this.columns) {
            var width = this._headerTableColumnGroupElement.children[column.get("ordinal")].style.width;
            result[columnIdentifier] = parseFloat(width);
        }
        return result;
    },

    applyColumnWidthsMap: function(columnWidthsMap)
    {
        for (var [identifier, column] of this.columns) {
            var width = (columnWidthsMap[identifier] || 0) + "%";
            var ordinal = column.get("ordinal");
            this._headerTableColumnGroupElement.children[ordinal].style.width = width;
            this._dataTableColumnGroupElement.children[ordinal].style.width = width;
        }

        this.updateLayout();
    },

    _isColumnVisible: function(columnIdentifier)
    {
        return !this.columns.get(columnIdentifier).has("hidden");
    },

    _showColumn: function(columnIdentifier)
    {
        this.columns.get(columnIdentifier).delete("hidden");
    },

    _hideColumn: function(columnIdentifier)
    {
        var column = this.columns.get(columnIdentifier);
        column.set("hidden", true);

        var columnElement = column.get("element");
        columnElement.style.width = 0;

        this._columnWidthsInitialized = false;
    },

    get scrollContainer()
    {
        return this._scrollContainerElement;
    },

    isScrolledToLastRow: function()
    {
        return this._scrollContainerElement.isScrolledToBottom();
    },

    scrollToLastRow: function()
    {
        this._scrollContainerElement.scrollTop = this._scrollContainerElement.scrollHeight - this._scrollContainerElement.offsetHeight;
    },

    _positionResizerElements: function()
    {
        var left = 0;
        var previousResizerElement = null;

        // Make n - 1 resizers for n columns.
        for (var i = 0; i < this.orderedColumns.length - 1; ++i) {
            var resizerElement = this.resizerElements[i];

            if (!resizerElement) {
                // This is the first call to updateWidth, so the resizers need
                // to be created.
                resizerElement = document.createElement("div");
                resizerElement.classList.add("data-grid-resizer");
                // This resizer is associated with the column to its right.
                resizerElement.addEventListener("mousedown", this._startResizerDragging.bind(this), false);
                this.element.appendChild(resizerElement);
                this.resizerElements[i] = resizerElement;
            }

            // Get the width of the cell in the first (and only) row of the
            // header table in order to determine the width of the column, since
            // it is not possible to query a column for its width.
            left += this._headerTableBodyElement.rows[0].cells[i].offsetWidth;

            if (this._isColumnVisible(this.orderedColumns[i])) {
                resizerElement.style.removeProperty("display");
                resizerElement.style.left = left + "px";
                resizerElement.leftNeighboringColumnID = i;
                if (previousResizerElement)
                    previousResizerElement.rightNeighboringColumnID = i;
                previousResizerElement = resizerElement;
            } else {
                resizerElement.style.setProperty("display", "none");
                resizerElement.leftNeighboringColumnID = 0;
                resizerElement.rightNeighboringColumnID = 0;
            }
        }
        if (previousResizerElement)
            previousResizerElement.rightNeighboringColumnID = this.orderedColumns.length - 1;
    },

    addPlaceholderNode: function()
    {
        if (this.placeholderNode)
            this.placeholderNode.makeNormal();

        var emptyData = {};
        for (var identifier of this.columns.keys())
            emptyData[identifier] = "";
        this.placeholderNode = new WebInspector.PlaceholderDataGridNode(emptyData);
        this.appendChild(this.placeholderNode);
    },

    appendChild: function(child)
    {
        this.insertChild(child, this.children.length);
    },

    insertChild: function(child, index)
    {
        if (!child)
            throw("insertChild: Node can't be undefined or null.");
        if (child.parent === this)
            throw("insertChild: Node is already a child of this node.");

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
    },

    removeChild: function(child)
    {
        if (!child)
            throw("removeChild: Node can't be undefined or null.");
        if (child.parent !== this)
            throw("removeChild: Node is not a child of this node.");

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

        console.assert(!child.isPlaceholderNode, "Shouldn't delete the placeholder node.")
    },

    removeChildren: function()
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
    },

    removeChildrenRecursive: function()
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
    },

    sortNodes: function(comparator)
    {
        function comparatorWrapper(aRow, bRow)
        {
            var reverseFactor = this.sortOrder !== "asceding" ? -1 : 1;
            var aNode = aRow._dataGridNode;
            var bNode = bRow._dataGridNode;
            if (aNode._data.summaryRow || aNode.isPlaceholderNode)
                return 1;
            if (bNode._data.summaryRow || bNode.isPlaceholderNode)
                return -1;

            return reverseFactor * comparator(aNode, bNode);
        }

        if (this._editing) {
            this._sortAfterEditingCallback = this.sortNodes.bind(this, comparator);
            return;
        }

        var tbody = this.dataTableBodyElement;
        var childNodes = tbody.childNodes;
        var fillerRowElement = tbody.lastChild;

        var sortedRowElements = Array.prototype.slice.call(childNodes, 0, childNodes.length - 1);
        sortedRowElements.sort(comparatorWrapper);

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
    },

    _keyDown: function(event)
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
    },

    expand: function()
    {
        // This is the root, do nothing.
    },

    collapse: function()
    {
        // This is the root, do nothing.
    },

    reveal: function()
    {
        // This is the root, do nothing.
    },

    revealAndSelect: function()
    {
        // This is the root, do nothing.
    },

    dataGridNodeFromNode: function(target)
    {
        var rowElement = target.enclosingNodeOrSelfWithNodeName("tr");
        return rowElement && rowElement._dataGridNode;
    },

    dataGridNodeFromPoint: function(x, y)
    {
        var node = this._dataTableElement.ownerDocument.elementFromPoint(x, y);
        var rowElement = node.enclosingNodeOrSelfWithNodeName("tr");
        return rowElement && rowElement._dataGridNode;
    },

    _clickInHeaderCell: function(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.columnIdentifier || !cell.classList.contains("sortable"))
            return;

        var sortOrder = this.sortOrder;

        if (this._sortColumnCell)
            this._sortColumnCell.removeMatchingStyleClasses("sort-\\w+");

        if (cell == this._sortColumnCell) {
            if (sortOrder === "ascending")
                sortOrder = "descending";
            else
                sortOrder = "ascending";
        }

        this._sortColumnCell = cell;

        cell.classList.add("sort-" + sortOrder);

        this.dispatchEventToListeners(WebInspector.DataGrid.Event.SortChanged);
    },

    _mouseoverColumnCollapser: function(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        cell.classList.add("mouse-over-collapser");
    },

    _mouseoutColumnCollapser: function(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        cell.classList.remove("mouse-over-collapser");
    },

    _clickInColumnCollapser: function(event)
    {
        var cell = event.target.enclosingNodeOrSelfWithNodeName("th");
        if (!cell || !cell.collapsesGroup)
            return;

        this._collapseColumnGroupWithCell(cell);

        event.stopPropagation();
        event.preventDefault();
    },

    collapseColumnGroup: function(columnGroup)
    {
        var collapserColumnIdentifier = null;
        for (var [identifier, column] of this.columns) {
            if (column.get("collapsesGroup") == columnGroup) {
                collapserColumnIdentifier = identifier;
                break;
            }
        }

        console.assert(collapserColumnIdentifier);
        if (!collapserColumnIdentifier)
            return;

        var cell = this._headerTableCellElements.get(collapserColumnIdentifier);
        this._collapseColumnGroupWithCell(cell);
    },

    _collapseColumnGroupWithCell: function(cell)
    {
        var columnsWillCollapse = cell.classList.toggle("collapsed");

        this.willToggleColumnGroup(cell.collapsesGroup, columnsWillCollapse);

        var showOrHide = columnsWillCollapse ? this._hideColumn : this._showColumn;
        for (var [identifier, column] of this.columns) {
            if (column.get("group") === cell.collapsesGroup)
                showOrHide.call(this, identifier);
        }

        var collapserButton = cell.querySelector(".collapser-button");
        if (collapserButton)
            collapserButton.title = columnsWillCollapse ? this._collapserButtonExpandColumnsToolTip() : this._collapserButtonCollapseColumnsToolTip();

        this.didToggleColumnGroup(cell.collapsesGroup, columnsWillCollapse);
    },

    _collapserButtonCollapseColumnsToolTip: function()
    {
        return WebInspector.UIString("Collapse columns");
    },

    _collapserButtonExpandColumnsToolTip: function()
    {
        return WebInspector.UIString("Expand columns");
    },

    willToggleColumnGroup: function(columnGroup, willCollapse)
    {
        // Implemented by subclasses if needed.
    },

    didToggleColumnGroup: function(columnGroup, didCollapse)
    {
        // Implemented by subclasses if needed.
    },

    isColumnSortColumn: function(columnIdentifier)
    {
        return this._sortColumnCell === this._headerTableCellElements.get(columnIdentifier);
    },

    markColumnAsSortedBy: function(columnIdentifier, sortOrder)
    {
        if (this._sortColumnCell)
            this._sortColumnCell.removeMatchingStyleClasses("sort-\\w+");
        this._sortColumnCell = this._headerTableCellElements.get(columnIdentifier);
        this._sortColumnCell.classList.add("sort-" + sortOrder);
    },

    headerTableHeader: function(columnIdentifier)
    {
        return this._headerTableCellElements.get(columnIdentifier);
    },

    _generateSortIndicatorImagesIfNeeded: function()
    {
        if (WebInspector.DataGrid._generatedSortIndicatorImages)
            return;

        WebInspector.DataGrid._generatedSortIndicatorImages = true;

        var specifications = {};
        specifications["arrow"] = {
            fillColor: [81, 81, 81],
            shadowColor: [255, 255, 255, 0.5],
            shadowOffsetX: 0,
            shadowOffsetY: 1,
            shadowBlur: 0
        };

        generateColoredImagesForCSS("Images/SortIndicatorDownArrow.svg", specifications, 9, 8, "data-grid-sort-indicator-down-");
        generateColoredImagesForCSS("Images/SortIndicatorUpArrow.svg", specifications, 9, 8, "data-grid-sort-indicator-up-");
    },

    _mouseDownInDataTable: function(event)
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
    },

    _contextMenuInDataTable: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu(event);

        var gridNode = this.dataGridNodeFromNode(event.target);
        if (this.dataGrid._refreshCallback && (!gridNode || gridNode !== this.placeholderNode))
            contextMenu.appendItem(WebInspector.UIString("Refresh"), this._refreshCallback.bind(this));

        if (gridNode && gridNode.selectable && !gridNode.isEventWithinDisclosureTriangle(event)) {
            contextMenu.appendItem(WebInspector.UIString("Copy Row"), this._copyRow.bind(this, event.target));

            if (this.dataGrid._editCallback) {
                if (gridNode === this.placeholderNode)
                    contextMenu.appendItem(WebInspector.UIString("Add New"), this._startEditing.bind(this, event.target));
                else {
                    var element = event.target.enclosingNodeOrSelfWithNodeName("td");
                    var columnIdentifier = element.__columnIdentifier;
                    var columnTitle = this.dataGrid.columns.get(columnIdentifier).get("title");
                    contextMenu.appendItem(WebInspector.UIString("Edit “%s”").format(columnTitle), this._startEditing.bind(this, event.target));
                }
            }
            if (this.dataGrid._deleteCallback && gridNode !== this.placeholderNode)
                contextMenu.appendItem(WebInspector.UIString("Delete"), this._deleteCallback.bind(this, gridNode));
        }

        contextMenu.show();
    },

    _clickInDataTable: function(event)
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
    },

    _copyTextForDataGridNode: function(node)
    {
        var fields = [];
        for (var identifier of node.dataGrid.orderedColumns)
            fields.push(node.data[identifier] || "");

        var tabSeparatedValues = fields.join("\t");
        return tabSeparatedValues;
    },

    handleBeforeCopyEvent: function(event)
    {
        if (this.selectedNode && window.getSelection().isCollapsed)
            event.preventDefault();
    },

    handleCopyEvent: function(event)
    {
        if (!this.selectedNode || !window.getSelection().isCollapsed)
            return;

        var copyText = this._copyTextForDataGridNode(this.selectedNode);
        event.clipboardData.setData("text/plain", copyText);
        event.stopPropagation();
        event.preventDefault();
    },

    _copyRow: function(target)
    {
        var gridNode = this.dataGridNodeFromNode(target);
        if (!gridNode)
            return;

        var copyText = this._copyTextForDataGridNode(gridNode);
        InspectorFrontendHost.copyText(copyText);
    },

    get resizeMethod()
    {
        if (typeof this._resizeMethod === "undefined")
            return WebInspector.DataGrid.ResizeMethod.Nearest;
        return this._resizeMethod;
    },

    set resizeMethod(method)
    {
        this._resizeMethod = method;
    },

    _startResizerDragging: function(event)
    {
        if (event.button !== 0 || event.ctrlKey)
            return;

        this._currentResizer = event.target;
        if (!this._currentResizer.rightNeighboringColumnID)
            return;

        WebInspector.elementDragStart(this._currentResizer, this._resizerDragging.bind(this),
            this._endResizerDragging.bind(this), event, "col-resize");
    },

    _resizerDragging: function(event)
    {
        if (event.button !== 0)
            return;

        var resizer = this._currentResizer;
        if (!resizer)
            return;

        // Constrain the dragpoint to be within the containing div of the
        // datagrid.
        var dragPoint = event.clientX - this.element.totalOffsetLeft;
        // Constrain the dragpoint to be within the space made up by the
        // column directly to the left and the column directly to the right.
        var leftCellIndex = resizer.leftNeighboringColumnID;
        var rightCellIndex = resizer.rightNeighboringColumnID;
        var firstRowCells = this._headerTableBodyElement.rows[0].cells;
        var leftEdgeOfPreviousColumn = 0;
        for (var i = 0; i < leftCellIndex; i++)
            leftEdgeOfPreviousColumn += firstRowCells[i].offsetWidth;

        // Differences for other resize methods
        if (this.resizeMethod == WebInspector.DataGrid.ResizeMethod.Last) {
            rightCellIndex = this.resizerElements.length;
        } else if (this.resizeMethod == WebInspector.DataGrid.ResizeMethod.First) {
            leftEdgeOfPreviousColumn += firstRowCells[leftCellIndex].offsetWidth - firstRowCells[0].offsetWidth;
            leftCellIndex = 0;
        }

        var rightEdgeOfNextColumn = leftEdgeOfPreviousColumn + firstRowCells[leftCellIndex].offsetWidth + firstRowCells[rightCellIndex].offsetWidth;

        // Give each column some padding so that they don't disappear.
        var leftMinimum = leftEdgeOfPreviousColumn + this.ColumnResizePadding;
        var rightMaximum = rightEdgeOfNextColumn - this.ColumnResizePadding;

        dragPoint = Number.constrain(dragPoint, leftMinimum, rightMaximum);

        resizer.style.left = (dragPoint - this.CenterResizerOverBorderAdjustment) + "px";

        var percentLeftColumn = (((dragPoint - leftEdgeOfPreviousColumn) / this._dataTableElement.offsetWidth) * 100) + "%";
        this._headerTableColumnGroupElement.children[leftCellIndex].style.width = percentLeftColumn;
        this._dataTableColumnGroupElement.children[leftCellIndex].style.width = percentLeftColumn;

        var percentRightColumn = (((rightEdgeOfNextColumn - dragPoint) / this._dataTableElement.offsetWidth) * 100) + "%";
        this._headerTableColumnGroupElement.children[rightCellIndex].style.width =  percentRightColumn;
        this._dataTableColumnGroupElement.children[rightCellIndex].style.width = percentRightColumn;

        this._positionResizerElements();
        event.preventDefault();
        this.dispatchEventToListeners(WebInspector.DataGrid.Event.DidLayout);
    },

    _endResizerDragging: function(event)
    {
        if (event.button !== 0)
            return;

        WebInspector.elementDragEnd(event);
        this._currentResizer = null;
        this.dispatchEventToListeners(WebInspector.DataGrid.Event.DidLayout);
    },

    ColumnResizePadding: 10,

    CenterResizerOverBorderAdjustment: 3,
}

WebInspector.DataGrid.ResizeMethod = {
    Nearest: "nearest",
    First: "first",
    Last: "last"
};

WebInspector.DataGrid.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {boolean=} hasChildren
 */
WebInspector.DataGridNode = function(data, hasChildren)
{
    this._expanded = false;
    this._selected = false;
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

WebInspector.DataGridNode.prototype = {
    get selectable()
    {
        return !this._element || !this._element.classList.contains("hidden");
    },

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

        this.createCells();
        return this._element;
    },

    createCells: function()
    {
        for (var columnIdentifier of this.dataGrid.orderedColumns)
            this._element.appendChild(this.createCell(columnIdentifier));
    },

    refreshIfNeeded: function()
    {
        if (!this._needsRefresh)
            return;

        delete this._needsRefresh;

        this.refresh();
    },

    needsRefresh: function()
    {
        this._needsRefresh = true;

        if (!this._revealed)
            return;

        if (this._scheduledRefreshIdentifier)
            return;

        this._scheduledRefreshIdentifier = requestAnimationFrame(this.refresh.bind(this));
    },

    get data()
    {
        return this._data;
    },

    set data(x)
    {
        this._data = x || {};
        this.needsRefresh();
    },

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
    },

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
            this._element.classList.remove("parent");
            this._element.classList.remove("expanded");
        }
    },

    get hasChildren()
    {
        return this._hasChildren;
    },

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
    },

    get depth()
    {
        if ("_depth" in this)
            return this._depth;
        if (this.parent && !this.parent.root)
            this._depth = this.parent.depth + 1;
        else
            this._depth = 0;
        return this._depth;
    },

    get leftPadding()
    {
        if (typeof(this._leftPadding) === "number")
            return this._leftPadding;
        
        this._leftPadding = this.depth * this.dataGrid.indentWidth;
        return this._leftPadding;
    },

    get shouldRefreshChildren()
    {
        return this._shouldRefreshChildren;
    },

    set shouldRefreshChildren(x)
    {
        this._shouldRefreshChildren = x;
        if (x && this.expanded)
            this.expand();
    },

    get selected()
    {
        return this._selected;
    },

    set selected(x)
    {
        if (x)
            this.select();
        else
            this.deselect();
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

    refresh: function()
    {
        if (!this._element || !this.dataGrid)
            return;

        if (this._scheduledRefreshIdentifier) {
            cancelAnimationFrame(this._scheduledRefreshIdentifier);
            delete this._scheduledRefreshIdentifier;
        }

        delete this._needsRefresh;

        this._element.removeChildren();
        this.createCells();
    },

    updateLayout: function()
    {
        // Implemented by subclasses if needed.
    },

    createCell: function(columnIdentifier)
    {
        var cellElement = document.createElement("td");
        cellElement.className = columnIdentifier + "-column";
        cellElement.__columnIdentifier = columnIdentifier;

        var column = this.dataGrid.columns.get(columnIdentifier);

        if (column.has("aligned"))
            cellElement.classList.add(column.get("aligned"));

        if (column.has("group"))
            cellElement.classList.add("column-group-" + column.get("group"));

        var div = cellElement.createChild("div");
        var content = this.createCellContent(columnIdentifier, cellElement);
        div.appendChild(content instanceof Node ? content : document.createTextNode(content));

        if (columnIdentifier === this.dataGrid.disclosureColumnIdentifier) {
            cellElement.classList.add("disclosure");
            if (this.leftPadding)
                cellElement.style.setProperty("padding-left", this.leftPadding + "px");
        }

        return cellElement;
    },

    createCellContent: function(columnIdentifier)
    {
        return this.data[columnIdentifier] || "\u200b"; // Zero width space to keep the cell from collapsing.
    },

    elementWithColumnIdentifier: function(columnIdentifier)
    {
        var index = this.dataGrid.orderedColumns.indexOf(columnIdentifier);
        if (index === -1)
            return null;

        return this._element.children[index];
    },

    // Share these functions with DataGrid. They are written to work with a DataGridNode this object.
    appendChild: WebInspector.DataGrid.prototype.appendChild,
    insertChild: WebInspector.DataGrid.prototype.insertChild,
    removeChild: WebInspector.DataGrid.prototype.removeChild,
    removeChildren: WebInspector.DataGrid.prototype.removeChildren,
    removeChildrenRecursive: WebInspector.DataGrid.prototype.removeChildrenRecursive,

    _recalculateSiblings: function(myIndex)
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
    },

    collapse: function()
    {
        if (this._element)
            this._element.classList.remove("expanded");

        this._expanded = false;

        for (var i = 0; i < this.children.length; ++i)
            this.children[i].revealed = false;

        this.dispatchEventToListeners("collapsed");

        if (this.dataGrid)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.CollapsedNode, {dataGridNode: this});
    },

    collapseRecursively: function()
    {
        var item = this;
        while (item) {
            if (item.expanded)
                item.collapse();
            item = item.traverseNextNode(false, this, true);
        }
    },

    expand: function()
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

            delete this._shouldRefreshChildren;
        }

        if (this._element)
            this._element.classList.add("expanded");

        this._expanded = true;

        this.dispatchEventToListeners("expanded");

        if (this.dataGrid)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.ExpandedNode, {dataGridNode: this});
    },

    expandRecursively: function()
    {
        var item = this;
        while (item) {
            item.expand();
            item = item.traverseNextNode(false, this);
        }
    },

    reveal: function()
    {
        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded)
                currentAncestor.expand();
            currentAncestor = currentAncestor.parent;
        }

        this.element.scrollIntoViewIfNeeded(false);

        this.dispatchEventToListeners("revealed");
    },

    /**
     * @param {boolean=} supressSelectedEvent
     */
    select: function(supressSelectedEvent)
    {
        if (!this.dataGrid || !this.selectable || this.selected)
            return;

        if (this.dataGrid.selectedNode)
            this.dataGrid.selectedNode.deselect();

        this._selected = true;
        this.dataGrid.selectedNode = this;

        if (this._element)
            this._element.classList.add("selected");

        if (!supressSelectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged);
    },

    revealAndSelect: function()
    {
        this.reveal();
        this.select();
    },

    /**
     * @param {boolean=} supressDeselectedEvent
     */
    deselect: function(supressDeselectedEvent)
    {
        if (!this.dataGrid || this.dataGrid.selectedNode !== this || !this.selected)
            return;

        this._selected = false;
        this.dataGrid.selectedNode = null;

        if (this._element)
            this._element.classList.remove("selected");

        if (!supressDeselectedEvent)
            this.dataGrid.dispatchEventToListeners(WebInspector.DataGrid.Event.SelectedNodeChanged);
    },

    traverseNextNode: function(skipHidden, stayWithin, dontPopulate, info)
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
    },

    traversePreviousNode: function(skipHidden, dontPopulate)
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
    },

    isEventWithinDisclosureTriangle: function(event)
    {
        if (!this.hasChildren)
            return false;
        var cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell.classList.contains("disclosure"))
            return false;

        var left = cell.totalOffsetLeft + this.leftPadding;
        return event.pageX >= left && event.pageX <= left + this.disclosureToggleWidth;
    },

    _attach: function()
    {
        if (!this.dataGrid || this._attached)
            return;

        this._attached = true;

        var nextElement = null;

        var previousGridNode = this.traversePreviousNode(true, true);
        if (previousGridNode && previousGridNode.element.parentNode)
            nextElement = previousGridNode.element.nextSibling;
        else if (!previousGridNode)
            nextElement = this.dataGrid.dataTableBodyElement.firstChild;

        // If there is no next grid node, then append before the last child since the last child is the filler row.
        console.assert(this.dataGrid.dataTableBodyElement.lastChild.classList.contains("filler"));

        if (!nextElement)
            nextElement = this.dataGrid.dataTableBodyElement.lastChild;

        this.dataGrid.dataTableBodyElement.insertBefore(this.element, nextElement);

        if (this.expanded)
            for (var i = 0; i < this.children.length; ++i)
                this.children[i]._attach();
    },

    _detach: function()
    {
        if (!this._attached)
            return;

        this._attached = false;

        if (this._element && this._element.parentNode)
            this._element.parentNode.removeChild(this._element);

        for (var i = 0; i < this.children.length; ++i)
            this.children[i]._detach();
    },

    savePosition: function()
    {
        if (this._savedPosition)
            return;

        if (!this.parent)
            throw("savePosition: Node must have a parent.");
        this._savedPosition = {
            parent: this.parent,
            index: this.parent.children.indexOf(this)
        };
    },

    restorePosition: function()
    {
        if (!this._savedPosition)
            return;

        if (this.parent !== this._savedPosition.parent)
            this._savedPosition.parent.insertChild(this, this._savedPosition.index);

        delete this._savedPosition;
    }
}

WebInspector.DataGridNode.prototype.__proto__ = WebInspector.Object.prototype;

// Used to create a new table row when entering new data by editing cells.
WebInspector.PlaceholderDataGridNode = function(data)
{
    WebInspector.DataGridNode.call(this, data, false);
    this.isPlaceholderNode = true;
}

WebInspector.PlaceholderDataGridNode.prototype = {
    constructor: WebInspector.PlaceholderDataGridNode,
    __proto__: WebInspector.DataGridNode.prototype,

    makeNormal: function()
    {
        delete this.isPlaceholderNode;
        delete this.makeNormal;
    }
}
