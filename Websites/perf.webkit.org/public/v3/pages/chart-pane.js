
class ChartPane extends ComponentBase {
    constructor(chartsPage, platformId, metricId)
    {
        super('chart-pane');

        this._chartsPage = chartsPage;
        this._platformId = platformId;
        this._metricId = metricId;

        var result = ChartsPage.createChartSourceList(platformId, metricId);
        this._errorMessage = result.error;
        this._platform = result.platform;
        this._metric = result.metric;

        this._overviewChart = null;
        this._mainChart = null;
        this._mainChartStatus = null;
        this._mainSelection = null;
        this._mainChartIndicatorWasLocked = false;
        this._status = null;
        this._revisions = null;

        this._paneOpenedByClick = null;

        this._commitLogViewer = this.content().querySelector('commit-log-viewer').component();
        this.content().querySelector('close-button').component().setCallback(chartsPage.closePane.bind(chartsPage));

        if (result.error)
            return;

        var formatter = result.metric.makeFormatter(3);
        var self = this;

        var overviewOptions = ChartsPage.overviewChartOptions(formatter);
        overviewOptions.selection.onchange = function (domain, didEndDrag) {
            self._chartsPage.setMainDomainFromOverviewSelection(domain, self, didEndDrag);
        }

        this._overviewChart = new InteractiveTimeSeriesChart(result.sourceList, overviewOptions);
        this.renderReplace(this.content().querySelector('.chart-pane-overview'), this._overviewChart);

        var mainOptions = ChartsPage.mainChartOptions(formatter);
        mainOptions.indicator.onchange = this._indicatorDidChange.bind(this);
        mainOptions.selection.onchange = this._mainSelectionDidChange.bind(this);
        mainOptions.selection.onzoom = this._mainSelectionDidZoom.bind(this);
        mainOptions.annotations.onclick = this._openAnalysisTask.bind(this);
        mainOptions.ondata = this._didFetchData.bind(this);
        this._mainChart = new InteractiveTimeSeriesChart(result.sourceList, mainOptions);
        this.renderReplace(this.content().querySelector('.chart-pane-main'), this._mainChart);

        this._mainChartStatus = new ChartPaneStatusView(result.metric, this._mainChart, chartsPage.router(), this._openCommitViewer.bind(this));
        this.renderReplace(this.content().querySelector('.chart-pane-details'), this._mainChartStatus);

        this.content().querySelector('.chart-pane').addEventListener('keyup', this._keyup.bind(this));
        this._fetchAnalysisTasks();
    }

    _fetchAnalysisTasks()
    {
        var self = this;
        AnalysisTask.fetchByPlatformAndMetric(this._platformId, this._metricId).then(function (tasks) {
            self._mainChart.setAnnotations(tasks.map(function (task) {
                var fillStyle = '#fc6';
                switch (task.changeType()) {
                case 'inconclusive':
                    fillStyle = '#fcc';
                case 'progression':
                    fillStyle = '#39f';
                    break;
                case 'regression':
                    fillStyle = '#c60';
                    break;
                case 'unchanged':
                    fillStyle = '#ccc';
                    break;
                }

                return {
                    task: task,
                    startTime: task.startTime(),
                    endTime: task.endTime(),
                    label: task.label(),
                    fillStyle: fillStyle,
                };
            }));
        });
    }

    platformId() { return this._platformId; }
    metricId() { return this._metricId; }

    serializeState()
    {
        var selection = this._mainChart ? this._mainChart.currentSelection() : null;
        var point = this._mainChart ? this._mainChart.currentPoint() : null;
        return [
            this._platformId,
            this._metricId,
            selection || (point && this._mainChartIndicatorWasLocked ? point.id : null),
        ];
    }

    updateFromSerializedState(state, isOpen)
    {
        if (!this._mainChart)
            return;

        var selectionOrIndicatedPoint = state[2];
        if (selectionOrIndicatedPoint instanceof Array)
            this._mainChart.setSelection([parseFloat(selectionOrIndicatedPoint[0]), parseFloat(selectionOrIndicatedPoint[1])]);
        else if (typeof(selectionOrIndicatedPoint) == 'number') {
            this._mainChart.setIndicator(selectionOrIndicatedPoint, true);
            this._mainChartIndicatorWasLocked = true;
        } else
            this._mainChart.setIndicator(null, false);
    }

    setOverviewDomain(startTime, endTime)
    {
        if (this._overviewChart)
            this._overviewChart.setDomain(startTime, endTime);
    }

    setOverviewSelection(selection)
    {
        if (this._overviewChart)
            this._overviewChart.setSelection(selection);
    }

    setMainDomain(startTime, endTime)
    {
        if (this._mainChart)
            this._mainChart.setDomain(startTime, endTime);
    }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        this._chartsPage.mainChartSelectionDidChange(this, didEndDrag);
        this.render();
    }

    _mainSelectionDidZoom(selection)
    {
        this._overviewChart.setSelection(selection, this);
        this._mainChart.setSelection(null);
        this._chartsPage.setMainDomainFromZoom(selection, this);
        this.render();
    }

    _openAnalysisTask(annotation)
    {
        window.open(this._chartsPage.router().url(`analysis/task/${annotation.task.id()}`), '_blank');
    }

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._chartsPage.mainChartIndicatorDidChange(this, isLocked || this._mainChartIndicatorWasLocked);
        this._mainChartIndicatorWasLocked = isLocked;
        this._mainChartStatus.updateRevisionListWithNotification();
        this.render();
    }

    _openCommitViewer(repository, from, to)
    {
        var self = this;
        this._commitLogViewer.view(repository, from, to).then(function () {
            self._mainChartStatus.setCurrentRepository(self._commitLogViewer.currentRepository());
            self.render();
        });
    }
    
    _didFetchData()
    {
        this._mainChartStatus.updateRevisionListWithNotification();
        this.render();
    }

    _keyup(event)
    {
        switch (event.keyCode) {
        case 37: // Left
            if (!this._mainChart.moveLockedIndicatorWithNotification(false))
                return;
            break;
        case 39: // Right
            if (!this._mainChart.moveLockedIndicatorWithNotification(true))
                return;
            break;
        case 38: // Up
            if (!this._mainChartStatus.moveRepositoryWithNotification(false))
                return;
        case 40: // Down
            if (!this._mainChartStatus.moveRepositoryWithNotification(true))
                return;
        default:
            return;
        }

        event.preventDefault();
        event.stopPropagation();
    }

    render()
    {
        Instrumentation.startMeasuringTime('ChartPane', 'render');

        super.render();

        if (this._platform && this._metric) {
            var metric = this._metric;
            var platform = this._platform;

            this.renderReplace(this.content().querySelector('.chart-pane-title'),
                metric.fullName() + ' on ' + platform.name());
        }

        if (this._errorMessage) {
            this.renderReplace(this.content().querySelector('.chart-pane-main'), this._errorMessage);
            return;
        }

        if (this._mainChartStatus) {
            this._mainChartStatus.render();
            this._renderActionToolbar(this._mainChartStatus.analyzeData());
        }

        var body = this.content().querySelector('.chart-pane-body');
        if (this._commitLogViewer.currentRepository()) {
            body.classList.add('has-second-sidebar');
            this._commitLogViewer.render();
        } else
            body.classList.remove('has-second-sidebar');

        Instrumentation.endMeasuringTime('ChartPane', 'render');
    }

    _renderActionToolbar(analyzeData)
    {
        var actions = [];
        var platform = this._platform;
        var metric = this._metric;

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var self = this;

        if (this._chartsPage.canBreakdown(platform, metric)) {
            actions.push(element('li', link('Breakdown', function () {
                self._chartsPage.insertBreakdownPanesAfter(platform, metric, self);
            })));
        }

        var platformPane = this.content().querySelector('.chart-pane-alternative-platforms');
        var alternativePlatforms = this._chartsPage.alternatePlatforms(platform, metric);
        if (alternativePlatforms.length) {
            this.renderReplace(platformPane, Platform.sortByName(alternativePlatforms).map(function (platform) {
                return element('li', link(platform.label(), function () {
                    self._chartsPage.insertPaneAfter(platform, metric, self);
                }));
            }));

            actions.push(element('li', {class: this._paneOpenedByClick == platformPane ? 'selected' : ''},
                this._makeAnchorToOpenPane(platformPane, 'Other Platforms', true)));
        } else {
            platformPane.style.display = 'none';
        }

        var analyzePane = this.content().querySelector('.chart-pane-analyze-pane');
        if (analyzeData) {
            actions.push(element('li', {class: this._paneOpenedByClick == analyzePane ? 'selected' : ''},
                this._makeAnchorToOpenPane(analyzePane, 'Analyze', false)));

            var router = this._chartsPage.router();
            analyzePane.onsubmit = function (event) {
                event.preventDefault();
                var newWindow = window.open(router.url('analysis/task/create'), '_blank');

                var name = analyzePane.querySelector('input').value;
                AnalysisTask.create(name, analyzeData.startPointId, analyzeData.endPointId).then(function (data) {
                    newWindow.location.href = router.url('analysis/task/' + data['taskId']);
                    // FIXME: Refetch the list of analysis tasks.
                }, function (error) {
                    newWindow.location.href = router.url('analysis/task/create', {error: error});
                });
            }
        } else {
            analyzePane.style.display = 'none';
            analyzePane.onsubmit = function (event) { event.preventDefault(); }
        }

        this._paneOpenedByClick = null;
        this.renderReplace(this.content().querySelector('.chart-pane-action-buttons'), actions);
    }

    _makeAnchorToOpenPane(pane, label, shouldRespondToHover)
    {
        var anchor = null;
        var ignoreMouseLeave = false;
        var self = this;
        var setPaneVisibility = function (pane, shouldShow) {
            var anchor = pane.anchor;
            if (shouldShow) {
                var width = anchor.offsetParent.offsetWidth;
                pane.style.top = anchor.offsetTop + anchor.offsetHeight + 'px';
                pane.style.right = (width - anchor.offsetLeft - anchor.offsetWidth) + 'px';
            }
            pane.style.display = shouldShow ? null : 'none';
            anchor.parentNode.className = shouldShow ? 'selected' : '';
            if (self._paneOpenedByClick == pane && !shouldShow)
                self._paneOpenedByClick = null;
        }

        var attributes = {
            href: '#',
            onclick: function (event) {
                event.preventDefault();
                var shouldShowPane = pane.style.display == 'none';
                if (shouldShowPane) {
                    if (self._paneOpenedByClick)
                        setPaneVisibility(self._paneOpenedByClick, false);
                    self._paneOpenedByClick = pane;
                }
                setPaneVisibility(pane, shouldShowPane);
            },
        };
        if (shouldRespondToHover) {
            var mouseIsInAnchor = false;
            var mouseIsInPane = false;

            attributes.onmouseenter = function () {
                if (self._paneOpenedByClick)
                    return;
                mouseIsInAnchor = true;
                setPaneVisibility(pane, true);
            }
            attributes.onmouseleave = function () {
                setTimeout(function () {
                    if (!mouseIsInPane)
                        setPaneVisibility(pane, false);
                }, 0);
                mouseIsInAnchor = false;                
            }

            pane.onmouseleave = function () {
                setTimeout(function () {
                    if (!mouseIsInAnchor)
                        setPaneVisibility(pane, false);
                }, 0);
                mouseIsInPane = false;
            }
            pane.onmouseenter = function () {
                mouseIsInPane = true;
            }
        }

        var anchor = ComponentBase.createElement('a', attributes, label);
        pane.anchor = anchor;
        return anchor;
    }

    static htmlTemplate()
    {
        return `
            <section class="chart-pane" tabindex="0">
                <header class="chart-pane-header">
                    <h2 class="chart-pane-title">-</h2>
                    <nav class="chart-pane-actions">
                        <ul>
                            <li class="close"><close-button></close-button></li>
                        </ul>
                        <ul class="chart-pane-action-buttons buttoned-toolbar"></ul>
                        <ul class="chart-pane-alternative-platforms" style="display:none"></ul>
                        <form class="chart-pane-analyze-pane" style="display:none">
                            <input type="text" required>
                            <button>Create</button>
                        </form>
                    </nav>
                </header>
                <div class="chart-pane-body">
                    <div class="chart-pane-main"></div>
                    <div class="chart-pane-sidebar">
                        <div class="chart-pane-overview"></div>
                        <div class="chart-pane-details"></div>
                    </div>
                    <div class="chart-pane-second-sidebar">
                        <commit-log-viewer></commit-log-viewer>
                    </div>
                </div>
            </section>
`;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `
            .chart-pane {
                margin: 1rem;
                margin-bottom: 2rem;
                padding: 0rem;
                height: 18rem;
                border: solid 1px #ccc;
                border-radius: 0.5rem;
                outline: none;
            }

            .chart-pane:focus .chart-pane-header {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-header {
                position: relative;
                left: 0;
                top: 0;
                width: 100%;
                height: 2rem;
                line-height: 2rem;
                border-bottom: solid 1px #ccc;
            }

            .chart-pane-title {
                margin: 0 0.5rem;
                padding: 0;
                padding-left: 1.5rem;
                font-size: 1rem;
                font-weight: inherit;
            }

            .chart-pane-actions {
                position: absolute;
                display: flex;
                flex-direction: row;
                justify-content: space-between;
                align-items: center;
                width: 100%;
                height: 2rem;
                top: 0;
                padding: 0 0;
            }

            .chart-pane-actions ul {
                display: block;
                padding: 0;
                margin: 0 0.5rem;
                font-size: 1rem;
                line-height: 1rem;
                list-style: none;
            }

            .chart-pane-actions .chart-pane-action-buttons {
                font-size: 0.9rem;
                line-height: 0.9rem;
            }

            .chart-pane-actions .chart-pane-alternative-platforms,
            .chart-pane-analyze-pane {
                position: absolute;
                top: 0;
                right: 0;
                border: solid 1px #ccc;
                border-radius: 0.2rem;
                z-index: 10;
                background: rgba(255, 255, 255, 0.8);
                -webkit-backdrop-filter: blur(0.5rem);
                padding: 0.2rem 0;
                margin: 0;
                margin-top: -0.2rem;
                margin-right: -0.2rem;
            }

            .chart-pane-alternative-platforms li {
            }

            .chart-pane-alternative-platforms li a {
                display: block;
                text-decoration: none;
                color: inherit;
                font-size: 0.9rem;
                padding: 0.2rem 0.5rem;
            }

            .chart-pane-alternative-platforms a:hover,
            .chart-pane-analyze-pane input:focus {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-analyze-pane {
                padding: 0.5rem;
            }

            .chart-pane-analyze-pane input {
                font-size: 1rem;
                width: 15rem;
                outline: none;
                border: solid 1px #ccc;
            }

            .chart-pane-body {
                position: relative;
                width: 100%;
                height: calc(100% - 2rem);
            }

            .chart-pane-main {
                padding-right: 20rem;
                height: 100%;
                margin: 0;
                vertical-align: middle;
                text-align: center;
            }

            .has-second-sidebar .chart-pane-main {
                padding-right: 40rem;
            }

            .chart-pane-main > * {
                width: 100%;
                height: 100%;
            }

            .chart-pane-sidebar,
            .chart-pane-second-sidebar {
                position: absolute;
                right: 0;
                top: 0;
                width: 0;
                border-left: solid 1px #ccc;
                height: 100%;
            }

            :not(.has-second-sidebar) > .chart-pane-second-sidebar {
                border-left: 0;
            }

            .chart-pane-sidebar {
                width: 20rem;
            }

            .has-second-sidebar .chart-pane-sidebar {
                right: 20rem;
            }

            .has-second-sidebar .chart-pane-second-sidebar {
                width: 20rem;
            }

            .chart-pane-overview {
                width: 100%;
                height: 5rem;
                border-bottom: solid 1px #ccc;
            }

            .chart-pane-overview > * {
                display: block;
                width: 100%;
                height: 100%;
            }

            .chart-pane-details {
                position: relative;
                display: block;
                height: calc(100% - 5.5rem - 2px);
                overflow-y: scroll;
                padding-top: 0.5rem;
            }
`;
    }
}
