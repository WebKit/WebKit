
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
        this._hoveringIndicator = null;
        this._indicatorForTooltip = null;
        this._firstIndicatorAnchor = null;
        this._showTooltip = false;
        this._builderByIndicator = null;
        this._tabIndexForIndicator = null;
        this._coordinateForIndicator = null;
        this._indicatorAnchorGrid = null;
        this._skipNextClick = false;
        this._skipNextStateCleanOnScroll = false;
        this._lastFocusedCell = null;
        this._renderTooltipLazily = new LazilyEvaluatedFunction(this._renderTooltip.bind(this));

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

    didConstructShadowTree()
    {
        super.didConstructShadowTree();

        const tooltipTable = this.content('tooltip-table');
        this.content().addEventListener('click', (event) => {
            if (!tooltipTable.contains(event.target))
                this._clearIndicatorState(false);
        });

        tooltipTable.onkeydown = this.createEventHandler((event) => {
            if (event.code == 'Escape') {
                event.preventDefault();
                event.stopPropagation();
                this._lastFocusedCell.focus({preventScroll: true});
            }
        }, {preventDefault: false, stopPropagation: false});

        window.addEventListener('keydown', (event) => {
            if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.code))
                return;

            event.preventDefault();
            if (!this._indicatorForTooltip && !this._hoveringIndicator) {
                if (this._firstIndicatorAnchor)
                    this._firstIndicatorAnchor.focus({preventScroll: true});
                return;
            }

            let [row, column] = this._coordinateForIndicator.get(this._indicatorForTooltip || this._hoveringIndicator);
            let direction = null;

            switch (event.code) {
                case 'ArrowUp':
                    row -= 1;
                    break;
                case 'ArrowDown':
                    row += 1;
                    break;
                case 'ArrowLeft':
                    column -= 1;
                    direction = 'leftOnly'
                    break;
                case 'ArrowRight':
                    column += 1;
                    direction = 'rightOnly'
            }

            const closestIndicatorAnchor = this._findClosestIndicatorAnchorForCoordinate(column, row, this._indicatorAnchorGrid, direction);
            if (closestIndicatorAnchor)
                closestIndicatorAnchor.focus({preventScroll: true});
        });
    }

    _findClosestIndicatorAnchorForCoordinate(columnIndex, rowIndex, grid, direction)
    {
        rowIndex = Math.min(Math.max(rowIndex, 0), grid.length - 1);
        const row = grid[rowIndex];
        if (!row.length)
            return null;

        const start = Math.min(Math.max(columnIndex, 0), row.length - 1);
        if (row[start])
            return row[start];

        let offset = 1;
        while (true) {
            const leftIndex = start - offset;
            if (leftIndex >= 0 && row[leftIndex] && direction != 'rightOnly')
                return row[leftIndex];
            const rightIndex = start + offset;
            if (rightIndex < row.length && row[rightIndex] && direction != 'leftOnly')
                return row[rightIndex];
            if (leftIndex < 0 && rightIndex >= row.length)
                break;
            offset += 1;
        }
        return null;
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

        let buildSummaryForFocusingIndicator = null;
        let buildForFocusingIndicator = null;
        let commitSetForFocusingdIndicator = null;
        let chartURLForFocusingIndicator = null;
        let platformForFocusingIndicator = null;
        let metricForFocusingIndicator = null;
        const builderForFocusingIndicator = this._indicatorForTooltip ? this._builderByIndicator.get(this._indicatorForTooltip) : null;
        const builderForHoveringIndicator = this._hoveringIndicator ? this._builderByIndicator.get(this._hoveringIndicator) : null;
        for (const [platform, lastDataPointByMetric] of this._lastDataPointByConfiguration.entries()) {
            for (const [metric, lastDataPoint] of lastDataPointByMetric.entries()) {
                const timeDuration = this._measurementSetFetchTime - lastDataPoint.time;
                const timeDurationSummaryPrefix = lastDataPoint.hasCurrentDataPoint ? '' : 'More than ';
                const timeDurationSummary = BuildRequest.formatTimeInterval(timeDuration);
                const summary = `${timeDurationSummaryPrefix}${timeDurationSummary} since latest data point.`;

                const indicator = this._indicatorByConfiguration.get(platform).get(metric);
                if (this._indicatorForTooltip && this._indicatorForTooltip === indicator) {
                    buildSummaryForFocusingIndicator = summary;
                    buildForFocusingIndicator = lastDataPoint.lastBuild;
                    commitSetForFocusingdIndicator = lastDataPoint.commitSetOfLastPoint;
                    chartURLForFocusingIndicator =  this._router.url('charts', ChartsPage.createStateForDashboardItem(platform.id(), metric.id(),
                        this._measurementSetFetchTime - this._timeDuration));
                    platformForFocusingIndicator = platform;
                    metricForFocusingIndicator = metric;
                }
                this._builderByIndicator.set(indicator, lastDataPoint.builder);
                const highlighted = builderForFocusingIndicator && builderForFocusingIndicator == lastDataPoint.builder
                    || builderForHoveringIndicator && builderForHoveringIndicator === lastDataPoint.builder;
                indicator.update(timeDuration, this._testAgeTolerance, highlighted);
            }
        }
        this._renderTooltipLazily.evaluate(this._indicatorForTooltip, this._showTooltip, buildSummaryForFocusingIndicator, buildForFocusingIndicator,
            commitSetForFocusingdIndicator, chartURLForFocusingIndicator, platformForFocusingIndicator, metricForFocusingIndicator, this._tabIndexForIndicator.get(this._indicatorForTooltip));
    }

    _renderTooltip(indicator, showTooltip, buildSummary, build, commitSet, chartURL, platform, metric, tabIndex)
    {
        if (!indicator || !buildSummary || !showTooltip) {
            this.content('tooltip-anchor').style.display =  showTooltip ? null : 'none';
            return;
        }
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;

        const rect = indicator.element().getBoundingClientRect();
        const tooltipAnchor = this.content('tooltip-anchor');
        tooltipAnchor.style.display = null;
        tooltipAnchor.style.top = rect.top + 'px';
        tooltipAnchor.style.left = rect.left + rect.width / 2 + 'px';

        let tableContent = [element('tr', element('td', {colspan: 2}, buildSummary))];

        if (chartURL) {
            const linkDescription = `${metric.test().name()} on ${platform.label()}`;
            tableContent.push(element('tr', [
                element('td', 'Chart'),
                element('td', {colspan: 2}, link(linkDescription, linkDescription, chartURL, true, tabIndex))
            ]));
        }

        if (commitSet) {
            if (commitSet.repositories().length)
                tableContent.push(element('tr', element('th', {colspan: 2}, 'Latest build information')));

            tableContent.push(Repository.sortByNamePreferringOnesWithURL(commitSet.repositories()).map((repository) => {
                const commit = commitSet.commitForRepository(repository);
                return element('tr', [
                    element('td', repository.name()),
                    element('td', commit.url() ? link(commit.label(), commit.label(), commit.url(), true, tabIndex) : commit.label())
                ]);
            }));
        }

        if (build) {
            const url = build.url();
            const buildTag = build.buildTag();
            tableContent.push(element('tr', [
                element('td', 'Build'),
                element('td', {colspan: 2}, [
                    url ? link(buildTag, build.label(), url, true, tabIndex) : buildTag
                ]),
            ]));
        }

        this.renderReplace(this.content("tooltip-table"),  tableContent);
    }

    _renderTable(platforms, metrics)
    {
        const element = ComponentBase.createElement;
        const tableHeadElements = [element('th',  {class: 'table-corner row-head'}, 'Platform \\ Test')];

        for (const metric of metrics)
            tableHeadElements.push(element('th', {class: 'diagonal-head'}, element('div', metric.test().fullName())));

        this._indicatorByConfiguration = new Map;
        this._coordinateForIndicator = new Map;
        this._tabIndexForIndicator = new Map;
        this._indicatorAnchorGrid = [];
        this._firstIndicatorAnchor = null;
        let currentTabIndex = 1;

        const tableBodyElement = platforms.map((platform, rowIndex) =>  {
            const indicatorByMetric = new Map;
            this._indicatorByConfiguration.set(platform, indicatorByMetric);

            let indicatorAnchorsInCurrentRow = [];

            const cells = metrics.map((metric, columnIndex) => {
                const [cell, anchor, indicator] = this._constructTableCell(platform, metric, currentTabIndex);

                indicatorAnchorsInCurrentRow.push(anchor);
                if (!indicator)
                    return cell;

                indicatorByMetric.set(metric, indicator);
                this._tabIndexForIndicator.set(indicator, currentTabIndex);
                this._coordinateForIndicator.set(indicator, [rowIndex, columnIndex]);

                ++currentTabIndex;
                if (!this._firstIndicatorAnchor)
                    this._firstIndicatorAnchor = anchor;
                return cell;
            });
            this._indicatorAnchorGrid.push(indicatorAnchorsInCurrentRow);

            const row = element('tr', [element('th', {class: 'row-head'}, platform.label()), ...cells]);
            return row;
        });

        const tableBody = element('tbody', tableBodyElement);
        tableBody.onscroll = this.createEventHandler(() => this._clearIndicatorState(true));

        this.renderReplace(this.content('test-health'), [element('thead', tableHeadElements), tableBody]);
    }

    _isValidPlatformMetricCombination(platform, metric)
    {
        return !(this._excludedConfigurations && this._excludedConfigurations[platform.id()]
            && this._excludedConfigurations[platform.id()].some((metricId) => metricId == metric.id()))
            && platform.hasMetric(metric);
    }

    _constructTableCell(platform, metric, tabIndex)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        if (!this._isValidPlatformMetricCombination(platform, metric))
            return [element('td', {class: 'blank-cell'}, element('div')), null, null];

        const indicator = new FreshnessIndicator;
        const anchor = link(indicator, '', () => {
            if (this._skipNextClick) {
                this._skipNextClick = false;
                return;
            }
            this._showTooltip = !this._showTooltip;
            this.enqueueToRender();
        }, false, tabIndex);

        const cell = element('td', {class: 'status-cell'}, anchor);
        this._configureAnchorForIndicator(anchor, indicator);
        return [cell, anchor, indicator];
    }

    _configureAnchorForIndicator(anchor, indicator)
    {
        anchor.onmouseover = this.createEventHandler(() => {
            this._hoveringIndicator = indicator;
            this.enqueueToRender();
        });
        anchor.onmousedown = this.createEventHandler(() =>
            this._skipNextClick = this._indicatorForTooltip != indicator, {preventDefault: false, stopPropagation: false});
        anchor.onfocus = this.createEventHandler(() => {
            this._showTooltip = this._indicatorForTooltip != indicator;
            this._hoveringIndicator = indicator;
            this._indicatorForTooltip = indicator;
            this._lastFocusedCell = anchor;
            this._skipNextStateCleanOnScroll = true;
            this.enqueueToRender();
        });
        anchor.onkeydown = this.createEventHandler((event) => {
            if (event.code == 'Escape') {
                event.preventDefault();
                event.stopPropagation();
                this._showTooltip = event.code == 'Enter' ? !this._showTooltip : false;
                this.enqueueToRender();
            }
        }, {preventDefault: false, stopPropagation: false});
    }

    _clearIndicatorState(dueToScroll)
    {
        if (this._skipNextStateCleanOnScroll) {
            this._skipNextStateCleanOnScroll = false;
            if (dueToScroll)
                return;
        }
        this._showTooltip = false;
        this._indicatorForTooltip = null;
        this._hoveringIndicator = null;
        this.enqueueToRender();
    }

    static htmlTemplate()
    {
        return `<section class="page-with-heading">
            <table id="test-health"></table>
            <div id="tooltip-anchor">
                <table id="tooltip-table"></table>
            </div>
        </section>`;
    }

    static cssTemplate()
    {
        return `
            .page-with-heading {
                display: grid;
                grid-template-columns: 1fr [content] min-content 1fr;
            }
            #test-health {
                font-size: 1rem;
                grid-column: content;
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
                min-width: 24rem;
            }
            #test-health th {
                text-align: left;
                border-bottom: 0.1rem solid #ccc;
                font-weight: normal;
            }
            #test-health th.diagonal-head {
                white-space: nowrap;
                height: 16rem;
                width: 2.2rem;
                border-bottom: 0rem;
                padding: 0;
            }
            #test-health th.diagonal-head > div {
                transform: translate(1.1rem, 7.5rem) rotate(315deg);
                transform-origin: center left;
                width: 2.2rem;
                border: 0rem;
            }
            #test-health tbody {
                display: block;
                overflow: auto;
                height: calc(100vh - 24rem);
            }
            #test-health td.status-cell {
                position: relative;
                margin: 0;
                padding: 0;
                max-width: 2.2rem;
                max-height: 2.2rem;
                min-width: 2.2rem;
                min-height: 2.2rem;
            }
            #test-health td.status-cell>a {
                display: block;
            }
            #test-health td.status-cell>a:focus {
                outline: none;
            }
            #test-health td.status-cell>a:focus::after {
                position: absolute;
                content: "";
                bottom: -0.1rem;
                left: 50%;
                margin-left: -0.2rem;
                height: 0rem;
                border-width: 0.2rem;
                border-style: solid;
                border-color: transparent transparent red transparent;
                outline: none;
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
                margin: auto;
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
            #tooltip-anchor {
                width: 0;
                height: 0;
                position: absolute;
            }
            #tooltip-table {
                position: absolute;
                width: 22rem;
                background-color: #696969;
                opacity: 0.96;
                margin: 0.3rem;
                padding: 0.3rem;
                border-radius: 0.4rem;
                z-index: 1;
                text-align: center;
                display: inline-table;
                color: white;
                bottom: 0;
                left: -11.3rem;
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
                margin-left: -0.3rem;
                border-width: 0.3rem;
                border-style: solid;
                border-color: #696969 transparent transparent transparent;
            }
            #tooltip-table a {
                color: white;
                font-weight: bold;
            }
            #tooltip-table a:focus {
                background-color: #AAB7B8;
                outline: none;
            }
        `;
    }

    routeName() { return 'test-freshness'; }
}