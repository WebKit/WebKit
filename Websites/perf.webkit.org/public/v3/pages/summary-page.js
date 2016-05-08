
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
        this._configGroups = [];
        this._excludedConfigurations = summarySettings.excludedConfigurations;

        for (var metricGroup of summarySettings.metricGroups) {
            var group = {name: metricGroup.name, rows: []};
            this._table.groups.push(group);
            for (var subMetricGroup of metricGroup.subgroups) {
                var row = {name: subMetricGroup.name, cells: []};
                group.rows.push(row);
                for (var platformGroup of summarySettings.platformGroups)
                    row.cells.push(this._createConfigurationGroup(platformGroup.platforms, subMetricGroup.metrics));
            }
        }
    }

    routeName() { return 'summary'; }

    open(state)
    {
        super.open(state);

        var current = Date.now();
        var timeRange = [current - 7 * 24 * 3600 * 1000, current];
        for (var group of this._configGroups)
            group.fetchAndComputeSummary(timeRange).then(this.render.bind(this));
    }
    
    render()
    {
        super.render();

        if (this._shouldConstructTable)
            this.renderReplace(this.content().querySelector('.summary-table'), this._constructTable());

        for (var render of this._renderQueue)
            render();
    }

    _createConfigurationGroup(platformIdList, metricIdList)
    {
        var platforms = platformIdList.map(function (id) { return Platform.findById(id); }).filter(function (obj) { return !!obj; });
        var metrics = metricIdList.map(function (id) { return Metric.findById(id); }).filter(function (obj) { return !!obj; });
        var configGroup = new SummaryPageConfigurationGroup(platforms, metrics, this._excludedConfigurations);
        this._configGroups.push(configGroup);
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
                return element('tbody', rowGroup.rows.map(function (row, rowIndex) {
                    var headings;
                    headings = [element('th', {class: 'minorHeader'}, row.name)];
                    if (!rowIndex)
                        headings.unshift(element('th', {class: 'majorHeader', rowspan: rowGroup.rows.length}, rowGroup.name));
                    return element('tr', [headings, row.cells.map(self._constructRatioGraph.bind(self))]);
                }));
            }),
        ];
    }

    _constructRatioGraph(configurationGroup)
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var configurationList = configurationGroup.configurationList();

        var ratioGraph = new RatioBarGraph();

        var state = ChartsPage.createStateForConfigurationList(configurationList);
        var anchor = link(ratioGraph, this.router().url('charts', state));
        this._renderQueue.push(function () {
            var warnings = configurationGroup.warnings();
            var warningText = '';
            for (var type in warnings) {
                var platformString = Array.from(warnings[type]).map(function (platform) { return platform.name(); }).join(', ');
                warningText += `Missing ${type} for following platform(s): ${platformString}`;
            }

            anchor.title = warningText || 'Open charts';
            ratioGraph.update(configurationGroup.ratio(), configurationGroup.label(), !!warningText);
            ratioGraph.render();
        });
        if (configurationList.length == 0)
            return element('td', ratioGraph);

        return element('td', anchor);
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
                margin: 0;
                width: 100%;
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

            .summary-table tbody tr:first-child > * {
                border-top: solid 1px #ddd;
            }

            .summary-table tbody tr:nth-child(even) > *:not(.majorHeader) {
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
    constructor(platforms, metrics, excludedConfigurations)
    {
        this._measurementSets = [];
        this._configurationList = [];
        this._setToRatio = new Map;
        this._ratio = null;
        this._label = null;
        this._warnings = {};
        this._smallerIsBetter = metrics.length ? metrics[0].isSmallerBetter() : null;

        for (var platform of platforms) {
            console.assert(platform instanceof Platform);
            for (var metric of metrics) {
                console.assert(metric instanceof Metric);
                console.assert(this._smallerIsBetter == metric.isSmallerBetter());
                metric.isSmallerBetter();

                if (excludedConfigurations && platform.id() in excludedConfigurations && excludedConfigurations[platform.id()].includes(+metric.id()))
                    continue;
                if (platform.hasMetric(metric)) {
                    this._measurementSets.push(MeasurementSet.findSet(platform.id(), metric.id(), platform.lastModified(metric)));
                    this._configurationList.push([platform.id(), metric.id()]);
                }
            }
        }
    }

    ratio() { return this._ratio; }
    label() { return this._label; }
    warnings() { return this._warnings; }
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
        if (isNaN(averageRatio))
            return;

        var currentIsSmallerThanBaseline = averageRatio < 1;
        var changeType = this._smallerIsBetter == currentIsSmallerThanBaseline ? 'better' : 'worse';
        averageRatio = Math.abs(averageRatio - 1);

        this._ratio = averageRatio * (changeType == 'better' ? 1 : -1);
        this._label = (averageRatio * 100).toFixed(1) + '%';
        this._changeType = changeType;
    }

    _fetchAndComputeRatio(set, timeRange)
    {
        var setToRatio = this._setToRatio;
        var self = this;
        return set.fetchBetween(timeRange[0], timeRange[1]).then(function () {
            var baselineTimeSeries = set.fetchedTimeSeries('baseline', false, false);
            var currentTimeSeries = set.fetchedTimeSeries('current', false, false);

            var baselineMedian = SummaryPageConfigurationGroup._medianForTimeRange(baselineTimeSeries, timeRange);
            var currentMedian = SummaryPageConfigurationGroup._medianForTimeRange(currentTimeSeries, timeRange);
            var platform = Platform.findById(set.platformId());
            if (!baselineMedian) {
                if(!('baseline' in self._warnings))
                    self._warnings['baseline'] = new Set;
                self._warnings['baseline'].add(platform);
            }
            if (!currentMedian) {
                if(!('current' in self._warnings))
                    self._warnings['current'] = new Set;
                self._warnings['current'].add(platform);
            }

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
}
