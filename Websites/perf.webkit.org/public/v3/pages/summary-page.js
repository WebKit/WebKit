
class SummaryPage extends PageWithHeading {

    constructor(summarySettings)
    {
        super('Summary', null);

        this._table = {
            heading: summarySettings.platformGroups.map(function (platformGroup) { return platformGroup.name; }),
            groups: [],
        };
        this._shouldConstructTable = true;
        this._renderQueue = [];

        var current = Date.now();
        var timeRange = [current - 7 * 24 * 3600 * 1000, current];
        for (var metricGroup of summarySettings.metricGroups) {
            var group = {name: metricGroup.name, rows: []};
            this._table.groups.push(group);
            for (var subMetricGroup of metricGroup.subgroups) {
                var row = {name: subMetricGroup.name, cells: []};
                group.rows.push(row);
                for (var platformGroup of summarySettings.platformGroups)
                    row.cells.push(this._createConfigurationGroupAndStartFetchingData(platformGroup.platforms, subMetricGroup.metrics, timeRange));
            }
        }
    }

    routeName() { return 'summary'; }

    open(state)
    {
        super.open(state);
    }
    
    render()
    {
        super.render();

        if (this._shouldConstructTable)
            this.renderReplace(this.content().querySelector('.summary-table'), this._constructTable());

        for (var render of this._renderQueue)
            render();
    }

    _createConfigurationGroupAndStartFetchingData(platformIdList, metricIdList, timeRange)
    {
        var platforms = platformIdList.map(function (id) { return Platform.findById(id); }).filter(function (obj) { return !!obj; });
        var metrics = metricIdList.map(function (id) { return Metric.findById(id); }).filter(function (obj) { return !!obj; });
        var configGroup = new SummaryPageConfigurationGroup(platforms, metrics);
        configGroup.fetchAndComputeSummary(timeRange).then(this.render.bind(this));
        return configGroup;
    }

    _constructTable()
    {
        var element = ComponentBase.createElement;

        var self = this;

        this._shouldConstructTable = false;
        this._renderQueue = [];

        return [
            element('thead',
                element('tr', [
                    element('td', {colspan: 2}),
                    this._table.heading.map(function (label) { return element('td', label); }),
                ])),
            this._table.groups.map(function (rowGroup) {
                return rowGroup.rows.map(function (row, rowIndex) {
                    var headings;
                    if (rowGroup.rows.length == 1)
                        headings = [element('th', {class: 'unifiedHeader', colspan: 2}, row.name)];
                    else {
                        headings = [element('th', {class: 'minorHeader'}, row.name)];
                        if (!rowIndex)
                            headings.unshift(element('th', {class: 'majorHeader', rowspan: rowGroup.rows.length}, rowGroup.name));
                    }
                    return element('tr', [headings, row.cells.map(self._constructRatioGraph.bind(self))]);
                });
            }),
        ];
    }

    _constructRatioGraph(configurationGroup)
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        var ratioGraph = new RatioBarGraph();

        this._renderQueue.push(function () {
            ratioGraph.update(configurationGroup.ratio(), configurationGroup.label());
            ratioGraph.render();
        });

        var state = ChartsPage.createStateForConfigurationList(configurationGroup.configurationList());
        return element('td', link(ratioGraph, 'Open charts', this.router().url('charts', state)));
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading"><table class="summary-table"></table></section>`;
    }

    static cssTemplate()
    {
        return `
            .summary-table {
                border-collapse: collapse;
                border: none;
                margin: 0 1rem;
                width: calc(100% - 2rem - 2px);
            }

            .summary-table td,
            .summary-table th {
                text-align: center;
                padding: 0px;
            }

            .summary-table .majorHeader {
                width: 5rem;
            }

            .summary-table .minorHeader {
                width: 7rem;
            }

            .summary-table .unifiedHeader {
                padding-left: 5rem;
            }

            .summary-table > tr:nth-child(even) > *:not(.majorHeader) {
                background: #f9f9f9;
            }

            .summary-table th,
            .summary-table thead td {
                color: #333;
                font-weight: inherit;
                font-size: 1rem;
                padding: 0.2rem 0.4rem;
            }

            .summary-table thead td {
                font-size: 1.2rem;
            }

            .summary-table tbody td {
                font-weight: inherit;
                font-size: 0.9rem;
                padding: 0;
            }

            .summary-table td > * {
                height: 100%;
            }
        `;
    }
}

class SummaryPageConfigurationGroup {
    constructor(platforms, metrics)
    {
        this._measurementSets = [];
        this._configurationList = [];
        this._setToRatio = new Map;
        this._ratio = null;
        this._label = null;
        this._changeType = null;
        this._smallerIsBetter = metrics.length ? metrics[0].isSmallerBetter() : null;

        for (var platform of platforms) {
            console.assert(platform instanceof Platform);
            for (var metric of metrics) {
                console.assert(metric instanceof Metric);
                console.assert(this._smallerIsBetter == metric.isSmallerBetter());
                metric.isSmallerBetter();
                if (platform.hasMetric(metric)) {
                    this._measurementSets.push(MeasurementSet.findSet(platform.id(), metric.id(), platform.lastModified(metric)));
                    this._configurationList.push([platform.id(), metric.id()]);
                }
            }
        }
    }

    ratio() { return this._ratio; }
    label() { return this._label; }
    changeType() { return this._changeType; }
    configurationList() { return this._configurationList; }

    fetchAndComputeSummary(timeRange)
    {
        console.assert(timeRange instanceof Array);
        console.assert(typeof(timeRange[0]) == 'number');
        console.assert(typeof(timeRange[1]) == 'number');

        var promises = [];
        for (var set of this._measurementSets)
            promises.push(this._fetchAndComputeRatio(set, timeRange));

        return Promise.all(promises).then(this._computeSummary.bind(this));
    }

    _computeSummary()
    {
        var ratios = [];
        for (var set of this._measurementSets) {
            var ratio = this._setToRatio.get(set);
            if (!isNaN(ratio))
                ratios.push(ratio);
        }

        var averageRatio = Statistics.mean(ratios);
        if (isNaN(averageRatio)) {
            this._summary = '-';
            this._changeType = null;
            return;
        }

        if (Math.abs(averageRatio - 1) < 0.001) { // Less than 0.1% difference.
            this._summary = 'No change';
            this._changeType = null;
            return;
        }

        var currentIsSmallerThanBaseline = averageRatio < 1;
        var changeType = this._smallerIsBetter == currentIsSmallerThanBaseline ? 'better' : 'worse';
        if (currentIsSmallerThanBaseline)
            averageRatio = 1 / averageRatio;

        this._ratio = (averageRatio - 1) * (changeType == 'better' ? 1 : -1);
        this._label = ((averageRatio - 1) * 100).toFixed(1) + '%';
        this._changeType = changeType;
    }

    _fetchAndComputeRatio(set, timeRange)
    {
        var setToRatio = this._setToRatio;
        return SummaryPageConfigurationGroup._fetchData(set, timeRange).then(function () {
            var baselineTimeSeries = set.fetchedTimeSeries('baseline', false, false);
            var currentTimeSeries = set.fetchedTimeSeries('current', false, false);

            var baselineMedian = SummaryPageConfigurationGroup._medianForTimeRange(baselineTimeSeries, timeRange);
            var currentMedian = SummaryPageConfigurationGroup._medianForTimeRange(currentTimeSeries, timeRange);
            setToRatio.set(set, currentMedian / baselineMedian);
        }).catch(function () {
            setToRatio.set(set, NaN);
        });
    }

    static _medianForTimeRange(timeSeries, timeRange)
    {
        if (!timeSeries.firstPoint())
            return NaN;

        var startPoint = timeSeries.findPointAfterTime(timeRange[0]) || timeSeries.lastPoint();
        var afterEndPoint = timeSeries.findPointAfterTime(timeRange[1]) || timeSeries.lastPoint();
        var endPoint = timeSeries.previousPoint(afterEndPoint);
        if (!endPoint || startPoint == afterEndPoint)
            endPoint = afterEndPoint;

        var points = timeSeries.dataBetweenPoints(startPoint, endPoint).map(function (point) { return point.value; });
        return Statistics.median(points);
    }

    static _fetchData(set, timeRange)
    {
        // FIXME: Make fetchBetween return a promise.
        var done = false;
        return new Promise(function (resolve, reject) {
            set.fetchBetween(timeRange[0], timeRange[1], function (error) {
                if (done)
                    return;
                if (error)
                    reject(null);
                else if (set.hasFetchedRange(timeRange[0], timeRange[1]))
                    resolve();
                done = true;
            });
        });
    }
}
