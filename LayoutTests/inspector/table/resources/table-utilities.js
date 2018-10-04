TestPage.registerInitializer(() => {
    InspectorTest.TableDataSource = class TableDataSource
    {
        constructor(items)
        {
            this._items = items;
        }

        get items() { return this._items; }

        // Table DataSource

        tableNumberOfRows(table)
        {
            return this._items.length;
        }
    }

    InspectorTest.TableDelegate = class TableDelegate
    {
        constructor(items)
        {
            this._items = items;
        }

        tableSelectionDidChange(table)
        {
            InspectorTest.pass("Table selection changed.");
        }

        tablePopulateCell(table, cell, column, rowIndex)
        {
            let item = this._items[rowIndex];
            InspectorTest.assert(item, "Should have an item for row " + rowIndex);
            InspectorTest.assert(item[column.identifier], "Should have data for column " + column.identifier);
            cell.textContent = item[column.identifier];
            return cell;
        }
    }

    InspectorTest.createTable = function(delegate, dataSource) {
        if (!dataSource) {
            let items = [];
            for (let i = 0; i < 10; ++i) {
                items.push({
                    index: i,
                    name: "Row " + i,
                });
            }
            dataSource = new InspectorTest.TableDataSource(items);
        }

        delegate = delegate || new InspectorTest.TableDelegate(dataSource.items);

        const rowHeight = 20;
        let table = new WI.Table("test", dataSource, delegate, rowHeight);
        table.addColumn(new WI.TableColumn("index", WI.UIString("Index")));
        table.addColumn(new WI.TableColumn("name", WI.UIString("Name")));

        table.updateLayout();

        return table;
    }
});
