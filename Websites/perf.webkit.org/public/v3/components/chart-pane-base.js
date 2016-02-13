
class ChartPaneBase extends ComponentBase {

    constructor(name)
    {
        super(name);

        this._errorMessage = null;
        this._platformId = null;
        this._metricId = null;
        this._platform = null;
        this._metric = null;

        this._overviewChart = null;
        this._mainChart = null;
        this._mainChartStatus = null;
        this._commitLogViewer = null;
        this._tasksForAnnotations = null;
    }

    configure(platformId, metricId)
    {
        var result = ChartStyles.createChartSourceList(platformId, metricId);
        this._errorMessage = result.error;
        this._platformId = platformId;
        this._metricId = metricId;
        this._platform = result.platform;
        this._metric = result.metric;

        this._overviewChart = null;
        this._mainChart = null;
        this._mainChartStatus = null;

        this._commitLogViewer = this.content().querySelector('commit-log-viewer').component();

        if (result.error)
            return;

        var formatter = result.metric.makeFormatter(4);
        var self = this;

        var overviewOptions = ChartStyles.overviewChartOptions(formatter);
        overviewOptions.selection.onchange = this._overviewSelectionDidChange.bind(this);

        this._overviewChart = new InteractiveTimeSeriesChart(result.sourceList, overviewOptions);
        this.renderReplace(this.content().querySelector('.chart-pane-overview'), this._overviewChart);

        var mainOptions = ChartStyles.mainChartOptions(formatter);
        mainOptions.indicator.onchange = this._indicatorDidChange.bind(this);
        mainOptions.selection.onchange = this._mainSelectionDidChange.bind(this);
        mainOptions.selection.onzoom = this._mainSelectionDidZoom.bind(this);
        mainOptions.annotations.onclick = this._openAnalysisTask.bind(this);
        mainOptions.ondata = this._didFetchData.bind(this);
        this._mainChart = new InteractiveTimeSeriesChart(result.sourceList, mainOptions);
        this.renderReplace(this.content().querySelector('.chart-pane-main'), this._mainChart);

        this._mainChartStatus = new ChartPaneStatusView(result.metric, this._mainChart, this._openCommitViewer.bind(this));
        this.renderReplace(this.content().querySelector('.chart-pane-details'), this._mainChartStatus);

        this.content().querySelector('.chart-pane').addEventListener('keyup', this._keyup.bind(this));

        this._fetchAnalysisTasks(platformId, metricId);
    }

    _fetchAnalysisTasks(platformId, metricId)
    {
        // FIXME: we need to update the annotation bars when the change type of tasks change.
        var self = this;
        AnalysisTask.fetchByPlatformAndMetric(platformId, metricId).then(function (tasks) {
            self._tasksForAnnotations = tasks;
            self.render();
        });
    }

    platformId() { return this._platformId; }
    metricId() { return this._metricId; }

    setOverviewDomain(startTime, endTime)
    {
        if (this._overviewChart)
            this._overviewChart.setDomain(startTime, endTime);
    }

    setMainDomain(startTime, endTime)
    {
        if (this._mainChart)
            this._mainChart.setDomain(startTime, endTime);
    }

    setMainSelection(selection)
    {
        if (this._mainChart)
            this._mainChart.setSelection(selection);
    }

    _overviewSelectionDidChange(domain, didEndDrag) { }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        this.render();
    }

    _mainSelectionDidZoom(selection)
    {
        this._overviewChart.setSelection(selection, this);
        this._mainChart.setSelection(null);
        this.render();
    }

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._mainChartStatus.updateRevisionListWithNotification();
        this.render();
    }

    _didFetchData()
    {
        this._mainChartStatus.updateRevisionListWithNotification();
        this.render();
    }

    _openAnalysisTask(annotation)
    {
        var router = this.router();
        console.assert(router);
        window.open(router.url(`analysis/task/${annotation.task.id()}`), '_blank');
    }

    router() { return null; }

    _openCommitViewer(repository, from, to)
    {
        this._mainChartStatus.setCurrentRepository(repository);
        this._commitLogViewer.view(repository, from, to).then(this.render.bind(this));
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

        this.render();

        event.preventDefault();
        event.stopPropagation();
    }

    render()
    {
        Instrumentation.startMeasuringTime('ChartPane', 'render');

        super.render();

        if (this._errorMessage) {
            this.renderReplace(this.content().querySelector('.chart-pane-main'), this._errorMessage);
            return;
        }

        this._renderAnnotations();

        if (this._mainChartStatus)
            this._mainChartStatus.render();

        var body = this.content().querySelector('.chart-pane-body');
        if (this._commitLogViewer && this._commitLogViewer.currentRepository()) {
            body.classList.add('has-second-sidebar');
            this._commitLogViewer.render();
        } else
            body.classList.remove('has-second-sidebar');

        Instrumentation.endMeasuringTime('ChartPane', 'render');
    }

    _renderAnnotations()
    {
        if (!this._tasksForAnnotations)
            return;

        var annotations = this._tasksForAnnotations.map(function (task) {
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
        });
        this._mainChart.setAnnotations(annotations);
    }

    static htmlTemplate()
    {
        return `
            <section class="chart-pane" tabindex="0">
                ${this.paneHeaderTemplate()}
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

    static paneHeaderTemplate() { return ''; }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `
            .chart-pane {
                padding: 0rem;
                height: 18rem;
                outline: none;
            }

            .chart-pane:focus .chart-pane-header {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-body {
                position: relative;
                width: 100%;
                height: 100%;
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
