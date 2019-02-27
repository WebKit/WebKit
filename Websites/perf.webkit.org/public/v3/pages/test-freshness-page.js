
class TestFreshnessPage extends PageWithHeading {
    constructor(summaryPageConfiguration, testAgeToleranceInHours)
    {
        super('test-freshness', null);
        this._testAgeTolerance = (testAgeToleranceInHours || 24) * 3600 * 1000;
        this._timeDuration = this._testAgeTolerance * 2;
        this._excludedConfigurations = {};
        this._lastDataPointByConfiguration = null;
        this._indicatorByConfiguration = null;
        this._renderTableLazily = new LazilyEvaluatedFunction(this._renderTable.bind(this));

        this._loadConfig(summaryPageConfiguration);
    }

    name() { return 'Test-Freshness'; }

    _loadConfig(summaryPageConfiguration)
    {
        const platformIdSet = new Set;
        const metricIdSet = new Set;

        for (const config of summaryPageConfiguration) {
            for (const platformGroup of config.platformGroups) {
                for (const platformId of platformGroup.platforms)
                    platformIdSet.add(platformId);
            }

            for (const metricGroup of config.metricGroups) {
                for (const subgroup of metricGroup.subgroups) {
                    for (const metricId of subgroup.metrics)
                        metricIdSet.add(metricId);
                }
            }

            const excludedConfigs = config.excludedConfigurations;
            for (const platform in excludedConfigs) {
                if (platform in this._excludedConfigurations)
                    this._excludedConfigurations[platform] = this._excludedConfigurations[platform].concat(excludedConfigs[platform]);
                else
                    this._excludedConfigurations[platform] = excludedConfigs[platform];
            }
        }
        this._platforms = [...platformIdSet].map((platformId) => Platform.findById(platformId));
        this._metrics = [...metricIdSet].map((metricId) => Metric.findById(metricId));
    }

    open(state)
    {
        this._fetchTestResults();
        super.open(state);
    }

    _fetchTestResults()
    {
        this._measurementSetFetchTime = Date.now();
        this._lastDataPointByConfiguration = new Map;

        const startTime = this._measurementSetFetchTime - this._timeDuration;

        for (const platform of this._platforms) {
            const lastDataPointByMetric = new Map;
            this._lastDataPointByConfiguration.set(platform, lastDataPointByMetric);

            for (const metric of this._metrics) {
                if (!this._isValidPlatformMetricCombination(platform, metric, this._excludedConfigurations))
                    continue;

                const measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), platform.lastModified(metric));
                measurementSet.fetchBetween(startTime, this._measurementSetFetchTime).then(() => {
                    const currentTimeSeries = measurementSet.fetchedTimeSeries('current', false, false);

                    let timeForLastDataPoint = startTime;
                    if (currentTimeSeries.lastPoint())
                        timeForLastDataPoint = currentTimeSeries.lastPoint().build().buildTime();

                    lastDataPointByMetric.set(metric, {time: timeForLastDataPoint, hasCurrentDataPoint: !!currentTimeSeries.lastPoint()});
                    this.enqueueToRender();
                });
            }
        }
    }

    render()
    {
        super.render();

        this._renderTableLazily.evaluate(this._platforms, this._metrics);

        for (const [platform, lastDataPointByMetric] of this._lastDataPointByConfiguration.entries()) {
            for (const [metric, lastDataPoint] of lastDataPointByMetric.entries()) {
                const timeDuration = this._measurementSetFetchTime - lastDataPoint.time;
                const timeDurationSummaryPrefix = lastDataPoint.hasCurrentDataPoint ? '' : 'More than ';
                const timeDurationSummary = BuildRequest.formatTimeInterval(timeDuration);
                const testLabel = `"${metric.test().fullName()}" for "${platform.name()}"`;
                const summary = `${timeDurationSummaryPrefix}${timeDurationSummary} since last data point on ${testLabel}`;
                const url = this._router.url('charts', ChartsPage.createStateForDashboardItem(platform.id(), metric.id(),
                    this._measurementSetFetchTime - this._timeDuration));

                const indicator = this._indicatorByConfiguration.get(platform).get(metric);
                indicator.update(timeDuration, this._testAgeTolerance, summary, url);
            }
        }
    }

    _renderTable(platforms, metrics)
    {
        const element = ComponentBase.createElement;
        const tableBodyElement = [];
        const tableHeadElements = [element('th',  {class: 'table-corner row-head'}, 'Platform \\ Test')];

        for (const metric of metrics)
            tableHeadElements.push(element('th', {class: 'diagonal-head'}, element('div', metric.test().fullName())));

        this._indicatorByConfiguration = new Map;
        for (const platform of platforms) {
            const indicatorByMetric = new Map;
            this._indicatorByConfiguration.set(platform, indicatorByMetric);
            tableBodyElement.push(element('tr',
                [element('th', {class: 'row-head'}, platform.label()), ...metrics.map((metric) => this._constructTableCell(platform, metric, indicatorByMetric))]));
        }

        this.renderReplace(this.content('test-health'), [element('thead', tableHeadElements), element('tbody', tableBodyElement)]);
    }

    _isValidPlatformMetricCombination(platform, metric)
    {
        return !(this._excludedConfigurations && this._excludedConfigurations[platform.id()]
            && this._excludedConfigurations[platform.id()].some((metricId) => metricId == metric.id()))
            && platform.hasMetric(metric);
    }

    _constructTableCell(platform, metric, indicatorByMetric)
    {
        const element = ComponentBase.createElement;

        if (!this._isValidPlatformMetricCombination(platform, metric))
            return element('td', {class: 'blank-cell'}, element('div'));

        const indicator = new FreshnessIndicator;
        indicatorByMetric.set(metric, indicator);
        return element('td', {class: 'status-cell'}, indicator);
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading"><table id="test-health"></table></section>`;
    }

    static cssTemplate()
    {
        return `
            .page-with-heading {
                display: flex;
                justify-content: center;
            }
            #test-health {
                font-size: 1rem;
            }
            #test-health thead {
                display: block;
                align: right;
            }
            #test-health th.table-corner {
                text-align: right;
                vertical-align: bottom;
            }
            #test-health .row-head {
                min-width: 15.5rem;
            }
            #test-health th {
                text-align: left;
                border-bottom: 0.1rem solid #ccc;
                font-weight: normal;
            }
            #test-health th.diagonal-head {
                white-space: nowrap;
                height: 16rem;
                border-bottom: 0rem;
            }
            #test-health th.diagonal-head > div {
                transform: translate(1rem, 7rem) rotate(315deg);
                width: 2rem;
                border: 0rem;
            }
            #test-health tbody {
                display: block;
                overflow: auto;
                height: 75vh;
            }
            #test-health td.status-cell {
                margin: 0;
                padding: 0;
                max-width: 2.2rem;
                max-height: 2.2rem;
                min-width: 2.2rem;
                min-height: 2.2rem;
            }
            #test-health td.blank-cell {
                margin: 0;
                padding: 0;
                max-width: 2.2rem;
                max-height: 2.2rem;
                min-width: 2.2rem;
                min-height: 2.2rem;
            }
            #test-health td.blank-cell > div  {
                background-color: #F9F9F9;
                height: 1.6rem;
                width: 1.6rem;
                margin: 0.1rem;
                padding: 0;
                position: relative;
                overflow: hidden;
            }
            #test-health td.blank-cell > div::before {
              content: "";
              position: absolute;
              top: -1px;
              left: -1px;
              display: block;
              width: 0px;
              height: 0px;
              border-right: calc(1.6rem + 1px) solid #ddd;
              border-top: calc(1.6rem + 1px) solid transparent;
            }
            #test-health td.blank-cell > div::after {
              content: "";
              display: block;
              position: absolute;
              top: 1px;
              left: 1px;
              width: 0px;
              height: 0px;
              border-right: calc(1.6rem - 1px) solid #F9F9F9;
              border-top: calc(1.6rem - 1px) solid transparent;
            }
        `;
    }

    routeName() { return 'test-freshness'; }
}