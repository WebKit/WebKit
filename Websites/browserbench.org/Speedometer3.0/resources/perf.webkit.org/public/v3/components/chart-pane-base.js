
class ChartPaneBase extends ComponentBase {

    constructor(name)
    {
        super(name);

        this._errorMessage = null;
        this._platformId = null;
        this._metricId = null;
        this._platform = null;
        this._metric = null;
        this._disableSampling = false;
        this._showOutliers = false;
        this._openRepository = null;

        this._overviewChart = null;
        this._mainChart = null;
        this._mainChartStatus = null;
        this._commitLogViewer = null;
        this._tasksForAnnotations = null;
        this._detectedAnnotations = null;
        this._renderAnnotationsLazily = new LazilyEvaluatedFunction(this._renderAnnotations.bind(this));
    }

    configure(platformId, metricId)
    {
        var result = ChartStyles.resolveConfiguration(platformId, metricId);
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

        this._overviewChart = new InteractiveTimeSeriesChart(this._createSourceList(false), ChartStyles.overviewChartOptions(formatter));
        this._overviewChart.listenToAction('selectionChange', this._overviewSelectionDidChange.bind(this));
        this.renderReplace(this.content().querySelector('.chart-pane-overview'), this._overviewChart);

        this._mainChart = new InteractiveTimeSeriesChart(this._createSourceList(true), ChartStyles.mainChartOptions(formatter));
        this._mainChart.listenToAction('dataChange', () => this._didFetchData());
        this._mainChart.listenToAction('indicatorChange', this._indicatorDidChange.bind(this));
        this._mainChart.listenToAction('selectionChange', this._mainSelectionDidChange.bind(this));
        this._mainChart.listenToAction('zoom', this._mainSelectionDidZoom.bind(this));
        this._mainChart.listenToAction('annotationClick', this._didClickAnnotation.bind(this));
        this.renderReplace(this.content().querySelector('.chart-pane-main'), this._mainChart);

        this._revisionRange = new ChartRevisionRange(this._mainChart);

        this._mainChartStatus = new ChartPaneStatusView(result.metric, this._mainChart);
        this._mainChartStatus.setCurrentRepository(this._openRepository);
        this._mainChartStatus.listenToAction('openRepository', this.openNewRepository.bind(this));
        this.renderReplace(this.content().querySelector('.chart-pane-details'), this._mainChartStatus);

        this.content().querySelector('.chart-pane').addEventListener('keydown', this._keyup.bind(this));

        this.fetchAnalysisTasks(false);
    }

    isSamplingEnabled() { return !this._disableSampling; }
    setSamplingEnabled(enabled)
    {
        this._disableSampling = !enabled;
        this._updateSourceList();
    }

    isShowingOutliers() { return this._showOutliers; }
    setShowOutliers(show)
    {
        this._showOutliers = !!show;
        this._updateSourceList();
    }

    _createSourceList(isMainChart)
    {
        return ChartStyles.createSourceList(this._platform, this._metric, this._disableSampling, this._showOutliers, isMainChart);
    }

    _updateSourceList()
    {
        this._mainChart.setSourceList(this._createSourceList(true));
        this._overviewChart.setSourceList(this._createSourceList(false));
    }

    fetchAnalysisTasks(noCache)
    {
        // FIXME: we need to update the annotation bars when the change type of tasks change.
        var self = this;
        AnalysisTask.fetchByPlatformAndMetric(this._platformId, this._metricId, noCache).then(function (tasks) {
            self._tasksForAnnotations = tasks;
            self.enqueueToRender();
        });
    }

    // FIXME: We should have a mechanism to get notified whenever the set of annotations change.
    didUpdateAnnotations()
    {
        this._tasksForAnnotations = [...this._tasksForAnnotations];
        this.enqueueToRender();
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

    setOpenRepository(repository)
    {
        this._openRepository = repository;
        if (this._mainChartStatus)
            this._mainChartStatus.setCurrentRepository(repository);
        this._updateCommitLogViewer();
    }

    _overviewSelectionDidChange(domain, didEndDrag) { }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        this._updateCommitLogViewer();
    }

    _mainSelectionDidZoom(selection)
    {
        this._overviewChart.setSelection(selection, this);
        this._mainChart.setSelection(null);
        this.enqueueToRender();
    }

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._updateCommitLogViewer();
    }

    _didFetchData()
    {
        this._updateCommitLogViewer();
    }

    _updateCommitLogViewer()
    {
        if (!this._revisionRange)
            return;
        const range = this._revisionRange.rangeForRepository(this._openRepository);
        this._commitLogViewer.view(this._openRepository, range.from, range.to);
        this.enqueueToRender();
    }

    _didClickAnnotation(annotation)
    {
        if (annotation.task)
            this._openAnalysisTask(annotation);
        else {
            const newSelection = [annotation.startTime, annotation.endTime];
            this._mainChart.setSelection(newSelection);
            this._overviewChart.setSelection(newSelection, this);
            this.enqueueToRender();
        }
    }

    _openAnalysisTask(annotation)
    {
        var router = this.router();
        console.assert(router);
        window.open(router.url(`analysis/task/${annotation.task.id()}`), '_blank');
    }

    router() { return null; }

    openNewRepository(repository)
    {
        this.content().querySelector('.chart-pane').focus();
        this.setOpenRepository(repository);
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
            if (!this._moveOpenRepository(false))
                return;
            break;
        case 40: // Down
            if (!this._moveOpenRepository(true))
                return;
            break;
        default:
            return;
        }

        this.enqueueToRender();

        event.preventDefault();
        event.stopPropagation();
    }

    _moveOpenRepository(forward)
    {
        const openRepository = this._openRepository;
        if (!openRepository)
            return false;

        const revisionList = this._revisionRange.revisionList();
        if (!revisionList)
            return false;

        const currentIndex = revisionList.findIndex((info) => info.repository == openRepository);
        console.assert(currentIndex >= 0);

        const newIndex = currentIndex + (forward ? 1 : -1);
        if (newIndex < 0 || newIndex >= revisionList.length)
            return false;

        this.openNewRepository(revisionList[newIndex].repository);

        return true;
    }

    render()
    {
        Instrumentation.startMeasuringTime('ChartPane', 'render');

        super.render();

        if (this._overviewChart)
            this._overviewChart.enqueueToRender();

        if (this._mainChart) {
            this._mainChart.enqueueToRender();
            this._renderAnnotationsLazily.evaluate(this._tasksForAnnotations, this._detectedAnnotations);
        }

        if (this._errorMessage) {
            this.renderReplace(this.content().querySelector('.chart-pane-main'), this._errorMessage);
            return;
        }


        if (this._mainChartStatus)
            this._mainChartStatus.enqueueToRender();

        var body = this.content().querySelector('.chart-pane-body');
        if (this._openRepository)
            body.classList.add('has-second-sidebar');
        else
            body.classList.remove('has-second-sidebar');

        Instrumentation.endMeasuringTime('ChartPane', 'render');
    }

    _renderAnnotations(taskForAnnotations, detectedAnnotations)
    {
        let annotations = (taskForAnnotations || []).map((task) => {
            return {
                task,
                fillStyle: ChartStyles.annotationFillStyleForTask(task),
                startTime: task.startTime(),
                endTime: task.endTime(),
                label: task.label()
            };
        });

        annotations = annotations.concat(detectedAnnotations || []);

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
            ${this.paneFooterTemplate()}
        `;
    }

    static paneHeaderTemplate() { return ''; }
    static paneFooterTemplate() { return ''; }

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
