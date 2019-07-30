
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
        this._currentlyHighlightedIndicator = null;
        this._hoveringTooltip = false;
        this._builderByIndicator = null;
        this._renderTooltipLazily = new LazilyEvaluatedFunction(this._renderTooltip.bind(this));

        this._loadConfig(summaryPageConfiguration);
    }

    didConstructShadowTree()
    {
        const tooltipTable = this.content('tooltip-table');
        tooltipTable.addEventListener('mouseenter', () => {
            this._hoveringTooltip = true;
            this.enqueueToRender();
        });
        tooltipTable.addEventListener('mouseleave', () => {
            this._hoveringTooltip = false;
            this.enqueueToRender();
        });
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
        this._builderByIndicator = new Map;

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

                    let timeForLatestBuild = startTime;
                    let lastBuild = null;
                    let builder = null;
                    let commitSetOfLastPoint = null;
                    const lastPoint = currentTimeSeries.lastPoint();
                    if (lastPoint) {
                        timeForLatestBuild = lastPoint.build().buildTime().getTime();
                        const view = currentTimeSeries.viewBetweenPoints(currentTimeSeries.firstPoint(), lastPoint);
                        for (const point of view) {
                            const build = point.build();
                            if (!build)
                                continue;
                            if (build.buildTime().getTime() >= timeForLatestBuild) {
                                timeForLatestBuild = build.buildTime().getTime();
                                lastBuild = build;
                                builder = build.builder();
                            }
                        }
                        commitSetOfLastPoint = lastPoint.commitSet();
                    }

                    lastDataPointByMetric.set(metric, {time: timeForLatestBuild, hasCurrentDataPoint: !!lastPoint,
                        lastBuild, builder, commitSetOfLastPoint});
                    this.enqueueToRender();
                });
            }
        }
    }

    render()
    {
        super.render();

        this._renderTableLazily.evaluate(this._platforms, this._metrics);

        let buildSummaryForCurrentlyHighlightedIndicator = null;
        let buildForCurrentlyHighlightedIndicator = null;
        let commitSetForCurrentHighlightedIndicator = null;
        const builderForCurrentlyHighlightedIndicator = this._currentlyHighlightedIndicator ? this._builderByIndicator.get(this._currentlyHighlightedIndicator) : null;
        for (const [platform, lastDataPointByMetric] of this._lastDataPointByConfiguration.entries()) {
            for (const [metric, lastDataPoint] of lastDataPointByMetric.entries()) {
                const timeDuration = this._measurementSetFetchTime - lastDataPoint.time;
                const timeDurationSummaryPrefix = lastDataPoint.hasCurrentDataPoint ? '' : 'More than ';
                const timeDurationSummary = BuildRequest.formatTimeInterval(timeDuration);
                const summary = `${timeDurationSummaryPrefix}${timeDurationSummary} since latest data point.`;
                const url = this._router.url('charts', ChartsPage.createStateForDashboardItem(platform.id(), metric.id(),
                    this._measurementSetFetchTime - this._timeDuration));

                const indicator = this._indicatorByConfiguration.get(platform).get(metric);
                if (this._currentlyHighlightedIndicator && this._currentlyHighlightedIndicator === indicator) {
                    buildSummaryForCurrentlyHighlightedIndicator = summary;
                    buildForCurrentlyHighlightedIndicator = lastDataPoint.lastBuild;
                    commitSetForCurrentHighlightedIndicator = lastDataPoint.commitSetOfLastPoint;
                }
                this._builderByIndicator.set(indicator, lastDataPoint.builder);
                indicator.update(timeDuration, this._testAgeTolerance, url, builderForCurrentlyHighlightedIndicator && builderForCurrentlyHighlightedIndicator === lastDataPoint.builder);
            }
        }
        this._renderTooltipLazily.evaluate(this._currentlyHighlightedIndicator, this._hoveringTooltip, buildSummaryForCurrentlyHighlightedIndicator, buildForCurrentlyHighlightedIndicator, commitSetForCurrentHighlightedIndicator);
    }

    _renderTooltip(indicator, hoveringTooltip, buildSummary, build, commitSet)
    {
        if (!indicator || !buildSummary) {
            this.content('tooltip-table').style.display = hoveringTooltip ? null : 'none';
            return;
        }
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;

        const rect = indicator.element().getBoundingClientRect();
        const tooltipTable = this.content('tooltip-table');
        tooltipTable.style.display = null;

        const tooltipContainerComputedStyle = getComputedStyle(tooltipTable);
        const containerMarginTop = parseFloat(tooltipContainerComputedStyle.paddingTop);
        const containerMarginLeft = parseFloat(tooltipContainerComputedStyle.marginLeft);

        tooltipTable.style.position = 'absolute';
        tooltipTable.style.top = rect.top - (tooltipTable.offsetHeight + containerMarginTop)  + 'px';
        tooltipTable.style.left = rect.left + rect.width / 2 - tooltipTable.offsetWidth / 2 + containerMarginLeft + 'px';

        let tableContent = [element('tr', element('td', {colspan: 2}, buildSummary))];

        if (commitSet) {
            if (commitSet.repositories().length)
                tableContent.push(element('tr', element('th', {colspan: 2}, 'Latest build information')));

            tableContent.push(Repository.sortByNamePreferringOnesWithURL(commitSet.repositories()).map((repository) => {
                const commit = commitSet.commitForRepository(repository);
                return element('tr', [
                    element('td', repository.name()),
                    element('td', commit.url() ? link(commit.label(), commit.label(), commit.url(), true) : commit.label())
                ]);
            }));
        }

        if (build) {
            const url = build.url();
            const buildNumber = build.buildNumber();
            tableContent.push(element('tr', [
                element('td', 'Build'),
                element('td', {colspan: 2}, [
                    url ? link(buildNumber, build.label(), url, true) : buildNumber
                ]),
            ]));
        }

        this.renderReplace(tooltipTable,  tableContent);
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
        indicator.listenToAction('select', (originator) => {
            this._currentlyHighlightedIndicator = originator;
            this.enqueueToRender();
        });
        indicator.listenToAction('unselect', () => {
            this._currentlyHighlightedIndicator = null;
            this.enqueueToRender();
        });
        indicatorByMetric.set(metric, indicator);
        return element('td', {class: 'status-cell'}, indicator);
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading"><table id="tooltip-table"></table><table id="test-health"></table></section>`;
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
                height: calc(100vh - 24rem);
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
            #tooltip-table {
                width: 22rem;
                background-color: #34495E;
                opacity: 0.9;
                margin: 0.3rem;
                padding: 0.3rem;
                border-radius: 0.4rem;
                z-index: 1;
                text-align: center;
                display: inline-table;
                color: white;
            }
            #tooltip-table td {
                overflow: hidden;
                max-width: 22rem;
                text-overflow: ellipsis;
            }
            #tooltip-table::after {
                content: " ";
                position: absolute;
                top: 100%;
                left: 50%;
                margin-left: -1rem;
                border-width: 0.2rem;
                border-style: solid;
                border-color: #34495E transparent transparent transparent;
            }
            #tooltip-table a {
                color: #B03A2E;
                font-weight: bold;
            }
        `;
    }

    routeName() { return 'test-freshness'; }
}