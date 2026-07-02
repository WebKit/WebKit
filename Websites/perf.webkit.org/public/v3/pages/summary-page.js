
class SummaryPage extends PageWithHeading {

    constructor(summarySettings)
    {
        super(summarySettings.name, null);

        this._route = summarySettings.route;
        this._table = {
            heading: summarySettings.platformGroups,
            groups: [],
        };
        this._shouldConstructTable = true;
        this._renderQueue = [];
        this._configGroups = [];
        this._excludedConfigurations = summarySettings.excludedConfigurations;
        this._weightedConfigurations = summarySettings.weightedConfigurations;

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

    routeName() { return `summary/${this._route}`; }

    open(state)
    {
        super.open(state);

        var current = Date.now();
        var timeRange = [current - 24 * 3600 * 1000, current];
        for (var group of this._configGroups)
            group.fetchAndComputeSummary(timeRange).then(() => { this.enqueueToRender(); });
    }

    render()
    {
        Instrumentation.startMeasuringTime('SummaryPage', 'render');
        super.render();

        if (this._shouldConstructTable) {
            Instrumentation.startMeasuringTime('SummaryPage', '_constructTable');
            this.renderReplace(this.content().querySelector('.summary-table'), this._constructTable());
            Instrumentation.endMeasuringTime('SummaryPage', '_constructTable');
        }

        for (var render of this._renderQueue)
            render();
        Instrumentation.endMeasuringTime('SummaryPage', 'render');
    }

    _createConfigurationGroup(platformIdList, metricIdList)
    {
        var platforms = platformIdList.map(function (id) { return Platform.findById(id); }).filter(function (obj) { return !!obj; });
        var metrics = metricIdList.map(function (id) { return Metric.findById(id); }).filter(function (obj) { return !!obj; });
        var configGroup = new SummaryPageConfigurationGroup(platforms, metrics, this._excludedConfigurations, this._weightedConfigurations);
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
                    this._table.heading.map(function (group) {
                        var nodes = [group.name];
                        if (group.subtitle) {
                            nodes.push(element('br'));
                            nodes.push(element('span', {class: 'subtitle'}, group.subtitle));
                        }
                        return element('td', nodes);
                    }),
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

        if (configurationList.length == 0) {
            this._renderQueue.push(() => { ratioGraph.enqueueToRender(); });
            return element('td', ratioGraph);
        }

        var state = ChartsPage.createStateForConfigurationList(configurationList);
        var anchor = link(ratioGraph, this.router().url('charts', state));
        var spinner = new SpinnerIcon;
        var cell = element('td', [anchor, spinner]);

        this._renderQueue.push(this._renderCell.bind(this, cell, spinner, anchor, ratioGraph, configurationGroup));
        return cell;
    }

    _renderCell(cell, spinner, anchor, ratioGraph, configurationGroup)
    {
        if (configurationGroup.isFetching())
            cell.classList.add('fetching');
        else
            cell.classList.remove('fetching');

        var warningText = this._warningTextForGroup(configurationGroup);
        anchor.title = warningText || 'Open charts';
        ratioGraph.update(configurationGroup.ratio(), configurationGroup.label(), !!warningText);
        ratioGraph.enqueueToRender();
    }

    _warningTextForGroup(configurationGroup)
    {
        function mapAndSortByName(platforms)
        {
            return platforms && platforms.map(function (platform) { return platform.label(); }).sort();
        }

        function pluralizeIfNeeded(singularWord, platforms) { return singularWord + (platforms.length > 1 ? 's' : ''); }

        var warnings = [];

        var missingPlatforms = mapAndSortByName(configurationGroup.missingPlatforms());
        if (missingPlatforms)
            warnings.push(`Missing ${pluralizeIfNeeded('platform', missingPlatforms)}: ${missingPlatforms.join(', ')}`);

        var platformsWithoutBaselines = mapAndSortByName(configurationGroup.platformsWithoutBaseline());
        if (platformsWithoutBaselines)
            warnings.push(`Need ${pluralizeIfNeeded('baseline', platformsWithoutBaselines)}: ${platformsWithoutBaselines.join(', ')}`);

        return warnings.length ? warnings.join('\n') : null;
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
                line-height: 1.3rem;
            }

            .summary-table .subtitle {
                display: block;
                font-size: 0.9rem;
                line-height: 1.2rem;
                color: #666;
            }

            .summary-table tbody td {
                position: relative;
                font-weight: inherit;
                font-size: 0.9rem;
                height: 2.5rem;
                padding: 0;
            }

            .summary-table td > * {
                height: 100%;
            }

            .summary-table td spinner-icon {
                display: block;
                position: absolute;
                top: 0.25rem;
                left: calc(50% - 1rem);
                z-index: 100;
            }

            .summary-table td.fetching a {
                display: none;
            }

            .summary-table td:not(.fetching) spinner-icon {
                display: none;
            }
        `;
    }
}

class SummaryPageConfigurationGroup {
    constructor(platforms, metrics, excludedConfigurations, weightedConfigurations)
    {
        this._measurementSets = [];
        this._configurationList = [];
        this._setToRatio = new Map;
        this._ratio = NaN;
        this._label = null;
        this._missingPlatforms = new Set;
        this._platformsWithoutBaseline = new Set;
        this._isFetching = false;
        this._smallerIsBetter = metrics.length ? metrics[0].isSmallerBetter() : null;
        this._weightedConfigurations = weightedConfigurations;

        for (const platform of platforms) {
            console.assert(platform instanceof Platform);
            let foundInSomeMetric = false;
            let excludedMerticCount = 0;
            for (const metric of metrics) {
                console.assert(metric instanceof Metric);
                console.assert(this._smallerIsBetter == metric.isSmallerBetter());
                metric.isSmallerBetter();

                if (excludedConfigurations && platform.id() in excludedConfigurations && excludedConfigurations[platform.id()].includes(+metric.id())) {
                    excludedMerticCount += 1;
                    continue;
                }
                if (!platform.hasMetric(metric))
                    continue;
                foundInSomeMetric = true;
                this._measurementSets.push(MeasurementSet.findSet(platform.id(), metric.id(), platform.lastModified(metric)));
                this._configurationList.push([platform.id(), metric.id()]);
            }
            if (!foundInSomeMetric && excludedMerticCount < metrics.length)
                this._missingPlatforms.add(platform);
        }
    }

    ratio() { return this._ratio; }
    label() { return this._label; }
    changeType() { return this._changeType; }
    configurationList() { return this._configurationList; }
    isFetching() { return this._isFetching; }
    missingPlatforms() { return this._missingPlatforms.size ? Array.from(this._missingPlatforms) : null; }
    platformsWithoutBaseline() { return this._platformsWithoutBaseline.size ? Array.from(this._platformsWithoutBaseline) : null; }

    fetchAndComputeSummary(timeRange)
    {
        console.assert(timeRange instanceof Array);
        console.assert(typeof(timeRange[0]) == 'number');
        console.assert(typeof(timeRange[1]) == 'number');

        var promises = [];
        for (var set of this._measurementSets)
            promises.push(this._fetchAndComputeRatio(set, timeRange));

        var self = this;
        var fetched = false;
        setTimeout(function () {
            // Don't set _isFetching to true if all promises were to resolve immediately (cached).
            if (!fetched)
                self._isFetching = true;
        }, 50);

        return Promise.all(promises).then(function () {
            fetched = true;
            self._isFetching = false;
            self._computeSummary();
        });
    }

    _computeSummary()
    {
        var ratios = [];
        for (var set of this._measurementSets) {
            const ratio = this._setToRatio.get(set);
            const weight = this._weightForMeasurementSet(set);
            if (!isNaN(ratio))
                ratios.push({value: ratio, weight});
        }

        var averageRatio = Statistics.weightedMean(ratios);
        if (isNaN(averageRatio))
            return;

        var currentIsSmallerThanBaseline = averageRatio < 1;
        var changeType = this._smallerIsBetter == currentIsSmallerThanBaseline ? 'better' : 'worse';
        averageRatio = Math.abs(averageRatio - 1);

        this._ratio = averageRatio * (changeType == 'better' ? 1 : -1);
        this._label = (averageRatio * 100).toFixed(1) + '%';
        this._changeType = changeType;
    }

    _weightForMeasurementSet(measurementSet)
    {
        if (!this._weightedConfigurations)
            return 1;
        const platformId = measurementSet.platformId();
        const weightForPlatform = this._weightedConfigurations[platformId];
        if (!weightForPlatform)
            return 1;
        if (!isNaN(+weightForPlatform))
            return +weightForPlatform;
        const metricId = measurementSet.metricId();
        if (typeof weightForPlatform != 'object' || isNaN(+weightForPlatform[metricId]))
            return 1
        return +weightForPlatform[metricId];
    }

    _fetchAndComputeRatio(set, timeRange)
    {
        var setToRatio = this._setToRatio;
        var self = this;
        return set.fetchBetween(timeRange[0], timeRange[1]).then(function () {
            var baselineTimeSeries = set.fetchedTimeSeries('baseline', false, false);
            var currentTimeSeries = set.fetchedTimeSeries('current', false, false);

            const baselineMean = SummaryPageConfigurationGroup._meanForTimeRange(baselineTimeSeries, timeRange);
            const currentMean = SummaryPageConfigurationGroup._meanForTimeRange(currentTimeSeries, timeRange);
            var platform = Platform.findById(set.platformId());
            if (!currentMean)
                self._missingPlatforms.add(platform);
            else if (!baselineMean)
                self._platformsWithoutBaseline.add(platform);

            setToRatio.set(set, currentMean / baselineMean);
        }).catch(function () {
            setToRatio.set(set, NaN);
        });
    }

    static _startAndEndPointForTimeRange(timeSeries, timeRange)
    {
        if (!timeSeries.firstPoint())
            return NaN;

        const startPoint = timeSeries.findPointAfterTime(timeRange[0]) || timeSeries.lastPoint();
        const afterEndPoint = timeSeries.findPointAfterTime(timeRange[1]) || timeSeries.lastPoint();
        let endPoint = timeSeries.previousPoint(afterEndPoint);
        if (!endPoint || startPoint == afterEndPoint)
            endPoint = afterEndPoint;

        return [startPoint, endPoint];
    }

    static _meanForTimeRange(timeSeries, timeRange)
    {
        const [startPoint, endPoint] = SummaryPageConfigurationGroup._startAndEndPointForTimeRange(timeSeries, timeRange);
        return Statistics.mean(timeSeries.viewBetweenPoints(startPoint, endPoint).values());
    }
}
