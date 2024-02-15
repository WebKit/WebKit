
class DashboardPage extends PageWithHeading {
    constructor(name, table, toolbar)
    {
        console.assert(toolbar instanceof DashboardToolbar);
        super(name, toolbar);
        this._table = table;
        this._charts = [];
        this._needsTableConstruction = true;
        this._needsStatusUpdate = true;
        this._statusViews = [];

        this._startTime = Date.now() - 60 * 24 * 3600 * 1000;
        this._endTime = Date.now();

        this._tableGroups = null;
    }

    routeName() { return `dashboard/${this._name}`; }

    serializeState()
    {
        return {numberOfDays: this.toolbar().numberOfDays()};
    }

    updateFromSerializedState(state, isOpen)
    {
        if (!isOpen || state.numberOfDays)
            this.toolbar().setNumberOfDays(state.numberOfDays);

        var startTime = this.toolbar().startTime();
        var endTime = this.toolbar().endTime();
        for (var chart of this._charts)
            chart.setDomain(startTime, endTime);

        this._needsTableConstruction = true;
        if (!isOpen)
            this.enqueueToRender();
    }

    open(state)
    {
        if (!this._tableGroups) {
            var columnCount = 0;
            var tableGroups = [];
            for (var row of this._table) {
                if (!row.some(function (cell) { return cell instanceof Array; })) {
                    tableGroups.push([]);
                    row = [''].concat(row);
                }
                tableGroups[tableGroups.length - 1].push(row);
                columnCount = Math.max(columnCount, row.length);
            }

            for (var group of tableGroups) {
                for (var row of group) {
                    for (var i = 0; i < row.length; i++) {
                        if (row[i] instanceof Array)
                            row[i] = this._createChartForCell(row[i]);
                    }
                    while (row.length < columnCount)
                        row.push([]);
                }
            }

            this._tableGroups = tableGroups;
        }

        super.open(state);
    }

    render()
    {
        super.render();

        console.assert(this._tableGroups);

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        if (this._needsTableConstruction) {
            var tree = [];
            var router = this.router();
            var startTime = this.toolbar().startTime();
            for (var group of this._tableGroups) {
                tree.push(element('thead', element('tr',
                    group[0].map(function (cell, cellIndex) {
                        if (!cellIndex)
                            return element('th', {class: 'heading-column'});
                        return element('td', cell.content || cell);
                    }))));

                tree.push(element('tbody', group.slice(1).map(function (row) {
                    return element('tr', row.map(function (cell, cellIndex) {
                        if (!cellIndex)
                            return element('th', element('span', {class: 'vertical-label'}, cell));

                        if (!cell.chart)
                            return element('td', cell);

                        var url = router.url('charts', 
                            ChartsPage.createStateForDashboardItem(cell.platform.id(), cell.metric.id(), startTime));
                        return element('td', [cell.statusView, link(cell.chart.element(), cell.label, url)]);
                    }));
                })));
            }

            this.renderReplace(this.content().querySelector('.dashboard-table'), tree);
            this._needsTableConstruction = false;
        }

        for (var chart of this._charts)
            chart.enqueueToRender();

        if (this._needsStatusUpdate) {
            for (var statusView of this._statusViews)
                statusView.enqueueToRender();
            this._needsStatusUpdate = false;
        }
    }

    _createChartForCell(cell)
    {
        console.assert(this.router());

        var platformId = cell[0];
        var metricId = cell[1];
        if (!platformId || !metricId)
            return '';

        var result = ChartStyles.resolveConfiguration(platformId, metricId);
        if (result.error)
            return result.error;

        var options = ChartStyles.dashboardOptions(result.metric.makeFormatter(3));
        let chart = new TimeSeriesChart(ChartStyles.createSourceList(result.platform, result.metric, false, false, true), options);
        chart.listenToAction('dataChange', () => this._fetchedData())
        this._charts.push(chart);

        var statusView = new DashboardChartStatusView(result.metric, chart);
        this._statusViews.push(statusView);

        return {
            chart: chart,
            statusView: statusView,
            platform: result.platform,
            metric: result.metric,
            label: result.metric.fullName() + ' on ' + result.platform.label()
        };
    }

    _fetchedData()
    {
        if (this._needsStatusUpdate)
            return;

        this._needsStatusUpdate = true;
        setTimeout(() => { this.enqueueToRender(); }, 10);
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading"><table class="dashboard-table"></table></section>`;
    }

    static cssTemplate()
    {
        return `
            .dashboard-table {
                table-layout: fixed;
            }
            .dashboard-table td,
            .dashboard-table th {
                border: none;
                text-align: center;
            }

            .dashboard-table th,
            .dashboard-table thead td {
                color: #f96;
                font-weight: inherit;
                font-size: 1.1rem;
                text-align: center;
                padding: 0.2rem 0.4rem;
            }
            .dashboard-table th {
                height: 10rem;
                width: 2rem;
                position: relative;
            }
            .dashboard-table .heading-column {
                width: 2rem;
                height: 1rem;
            }
            .dashboard-table th .vertical-label {
                position: absolute;
                left: 0;
                right: 0;
                display: block;
                -webkit-transform: rotate(-90deg) translate(-50%, 0);
                -webkit-transform-origin: 0 0;
                transform: rotate(-90deg) translate(-50%, 0);
                transform-origin: 0 0;
                width: 10rem;
                height: 2rem;
                line-height: 1.8rem;
            }
            table.dashboard-table {
                width: 100%;
                height: 100%;
                border: 0;
            }
            .dashboard-table td time-series-chart {
                height: 10rem;
            }
            .dashboard-table td > chart-status-view {
                display: block;
                width: 100%;
            }

            .dashboard-table td *:first-child {
                margin: 0 0 0.2rem 0;
            }
        `;
    }
}
