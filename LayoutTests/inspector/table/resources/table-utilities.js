TestPage.registerInitializer(() => {
    InspectorTest.TableDataSource = class TableDataSource
    {
        constructor(items)
        {
            this._items = items || [];
        }

        get items() { return this._items; }

        tableNumberOfRows(table)
        {
            return this._items.length;
        }
    };

    InspectorTest.TableDelegate = class TableDelegate
    {
        constructor(items)
        {
            this.items = items || [];
        }

        tableDidRemoveRows(table, rowIndexes)
        {
            // Prevent data source from getting out of sync.
            for (let index = rowIndexes.length - 1; index >= 0; --index)
                this.items.splice(index, 1);
        }

        tableSelectionDidChange(table)
        {
            InspectorTest.pass("Table selection changed.");
        }

        tablePopulateCell(table, cell, column, rowIndex)
        {
            let item = this.items[rowIndex];
            InspectorTest.assert(item, "Should have an item for row " + rowIndex);
            InspectorTest.assert(item[column.identifier], "Should have data for column " + column.identifier);
            cell.textContent = item[column.identifier];
            return cell;
        }
    };

    function createDataSource(numberOfRows = 10) {
        let items = [];
        for (let i = 0; i < numberOfRows; ++i)
            items.push({index: i, name: `Row ${i}`});

        return new InspectorTest.TableDataSource(items);
    }

    function createTableInternal(dataSource, delegate) {
        InspectorTest.assert(dataSource instanceof InspectorTest.TableDataSource);
        InspectorTest.assert(delegate instanceof InspectorTest.TableDelegate);

        const rowHeight = 20;
        let table = new WI.Table("test", dataSource, delegate, rowHeight);
        table.addColumn(new WI.TableColumn("index", "Index"));
        table.addColumn(new WI.TableColumn("name", "Name"));

        table.updateLayout();

        return table;
    }

    InspectorTest.createTable = function(numberOfRows) {
        let dataSource = createDataSource(numberOfRows);
        let delegate = new InspectorTest.TableDelegate(dataSource.items);
        return createTableInternal(dataSource, delegate);
    };

    InspectorTest.createTableWithDelegate = function(delegate, numberOfRows) {
        InspectorTest.assert(delegate instanceof InspectorTest.TableDelegate);

        let dataSource = createDataSource(numberOfRows);
        delegate.items = dataSource.items;
        return createTableInternal(dataSource, delegate);
    };
});
