
class AnalysisTaskChartPane extends ChartPaneBase {
    constructor() { super('analysis-task-chart-pane'); }
}

ComponentBase.defineElement('analysis-task-chart-pane', AnalysisTaskChartPane);

class AnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Analysis Task');
        this._taskId = null;
        this._task = null;
        this._testGroups = null;
        this._renderedTestGroups = null;
        this._renderedCurrentTestGroup = null;
        this._analysisResults = null;
        this._measurementSet = null;
        this._startPoint = null;
        this._endPoint = null;
        this._errorMessage = null;
        this._currentTestGroup = null;
        this._chartPane = this.content().querySelector('analysis-task-chart-pane').component();
        this._analysisResultsViewer = this.content().querySelector('analysis-results-viewer').component();
        this._analysisResultsViewer.setTestGroupCallback(this._showTestGroup.bind(this));
        this._testGroupResultsTable = this.content().querySelector('test-group-results-table').component();
    }

    title() { return this._task ? this._task.label() : 'Analysis Task'; }
    routeName() { return 'analysis/task'; }

    updateFromSerializedState(state)
    {
        var self = this;
        if (state.remainingRoute) {
            this._taskId = parseInt(state.remainingRoute);
            AnalysisTask.fetchById(this._taskId).then(this._didFetchTask.bind(this), function (error) {
                self._errorMessage = `Failed to fetch the analysis task ${state.remainingRoute}: ${error}`;
                self.render();
            });
            TestGroup.fetchByTask(this._taskId).then(this._didFetchTestGroups.bind(this));
            AnalysisResults.fetch(this._taskId).then(this._didFetchAnalysisResults.bind(this));
        } else if (state.buildRequest) {
            var buildRequestId = parseInt(state.buildRequest);
            AnalysisTask.fetchByBuildRequestId(buildRequestId).then(this._didFetchTask.bind(this)).then(function () {
                if (self._task) {
                    TestGroup.fetchByTask(self._task.id()).then(self._didFetchTestGroups.bind(self));
                    AnalysisResults.fetch(self._task.id()).then(this._didFetchAnalysisResults.bind(this));
                }
            }, function (error) {
                self._errorMessage = `Failed to fetch the analysis task for the build request ${buildRequestId}: ${error}`;
                self.render();
            });
        }
    }

    _didFetchTask(task)
    {
        this._task = task;
        var platform = task.platform();
        var metric = task.metric();
        var lastModified = platform.lastModified(metric);

        this._measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        this._measurementSet.fetchBetween(task.startTime(), task.endTime(), this._didFetchMeasurement.bind(this));

        var formatter = metric.makeFormatter(4);
        this._analysisResultsViewer.setValueFormatter(formatter);
        this._analysisResultsViewer.setSmallerIsBetter(metric.isSmallerBetter());
        this._testGroupResultsTable.setValueFormatter(formatter);

        this._chartPane.configure(platform.id(), metric.id());

        var domain = ChartsPage.createDomainForAnalysisTask(task);
        this._chartPane.setOverviewDomain(domain[0], domain[1]);
        this._chartPane.setMainDomain(domain[0], domain[1]);

        this.render();
    }

    _didFetchMeasurement()
    {
        console.assert(this._task);
        console.assert(this._measurementSet);
        var series = this._measurementSet.fetchedTimeSeries('current', false, false);
        var startPoint = series.findById(this._task.startMeasurementId());
        var endPoint = series.findById(this._task.endMeasurementId());
        if (!startPoint || !endPoint)
            return;

        this._analysisResultsViewer.setPoints(startPoint, endPoint);

        this._startPoint = startPoint;
        this._endPoint = endPoint;
        this.render();
    }

    _didFetchTestGroups(testGroups)
    {
        this._testGroups = testGroups.sort(function (a, b) { return +a.createdAt() - b.createdAt(); });
        this._currentTestGroup = testGroups.length ? testGroups[0] : null;

        this._analysisResultsViewer.setTestGroups(testGroups);
        this._testGroupResultsTable.setTestGroup(this._currentTestGroup);
        this._assignTestResultsIfPossible();
        this.render();
    }

    _didFetchAnalysisResults(results)
    {
        this._analysisResults = results;
        if (this._assignTestResultsIfPossible())
            this.render();
    }

    _assignTestResultsIfPossible()
    {
        if (!this._task || !this._testGroups || !this._analysisResults)
            return false;

        for (var group of this._testGroups) {
            for (var request of group.buildRequests())
                request.setResult(this._analysisResults.find(request.buildId(), this._task.metric()));
        }

        this._analysisResultsViewer.didUpdateResults();
        this._testGroupResultsTable.didUpdateResults();

        return true;
    }

    render()
    {
        super.render();

        Instrumentation.startMeasuringTime('AnalysisTaskPage', 'render');

        this.content().querySelector('.error-message').textContent = this._errorMessage || '';

        var v2URL = `/v2/#/analysis/task/${this._taskId}`;
        this.content().querySelector('.error-message').innerHTML +=
            `<p>This page is read only for now. To schedule a new A/B testing job, use <a href="${v2URL}">v2 page</a>.</p>`;

         this._chartPane.render();

        if (this._task) {
            this.renderReplace(this.content().querySelector('.analysis-task-name'), this._task.name());
            var platform = this._task.platform();
            var metric = this._task.metric();
            var anchor = this.content().querySelector('.platform-metric-names a');
            this.renderReplace(anchor, metric.fullName() + ' on ' + platform.label());
            anchor.href = this.router().url('charts', ChartsPage.createStateForAnalysisTask(this._task));
        }

        this._analysisResultsViewer.setCurrentTestGroup(this._currentTestGroup);
        this._analysisResultsViewer.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        if (this._testGroups != this._renderedTestGroups) {
            this._renderedTestGroups = this._testGroups;
            var self = this;
            this.renderReplace(this.content().querySelector('.test-group-list'),
                this._testGroups.map(function (group) {
                    return element('li', {class: 'test-group-list-' + group.id()}, link(group.label(), function () {
                        self._showTestGroup(group);
                    }));
                }));
            this._renderedCurrentTestGroup = null;
        }
        if (this._renderedCurrentTestGroup != this._currentTestGroup) {
            if (this._renderedCurrentTestGroup) {
                var element = this.content().querySelector('.test-group-list-' + this._renderedCurrentTestGroup.id());
                if (element)
                    element.classList.remove('selected');
            }
            if (this._currentTestGroup) {
                var element = this.content().querySelector('.test-group-list-' + this._currentTestGroup.id());
                if (element)
                    element.classList.add('selected');
            }
            this._renderedCurrentTestGroup = this._currentTestGroup;
        }

        this._testGroupResultsTable.render();

        Instrumentation.endMeasuringTime('AnalysisTaskPage', 'render');
    }

    _showTestGroup(testGroup)
    {
        this._currentTestGroup = testGroup;        
        this._testGroupResultsTable.setTestGroup(this._currentTestGroup);
        this.render();
    }

    static htmlTemplate()
    {
        return `
        <div class="analysis-tasl-page-container">
            <div class="analysis-tasl-page">
                <h2 class="analysis-task-name"></h2>
                <h3 class="platform-metric-names"><a href=""></a></h3>
                <p class="error-message"></p>
                <div class="overview-chart"><analysis-task-chart-pane></analysis-task-chart-pane></div>
                <section class="analysis-results-view">
                    <analysis-results-viewer></analysis-results-viewer>
                </section>
                <section class="test-group-view">
                    <ul class="test-group-list"></ul>
                    <div class="test-group-details"><test-group-results-table></test-group-results-table></div>
                </section>
            </div>
        </div>
`;
    }

    static cssTemplate()
    {
        return `
            .analysis-tasl-page-container {
            }
            .analysis-tasl-page {
            }

            .analysis-task-name {
                font-size: 1.2rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            .platform-metric-names {
                font-size: 1rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            .platform-metric-names a {
                text-decoration: none;
                color: inherit;
            }

            .platform-metric-names:empty {
                margin: 0;
            }

            .error-message:not(:empty) {
                margin: 1rem;
                padding: 0;
            }

            .analysis-results-view {
                margin: 1rem;
            }

            .test-configuration h3 {
                font-size: 1rem;
                font-weight: inherit;
                color: inherit;
                margin: 0 1rem;
                padding: 0;
            }

            .test-group-view {
                display: table;
                margin: 0 1rem;
            }

            .test-group-details {
                display: table-cell;
                margin-bottom: 1rem;
            }

            .test-group-list {
                display: table-cell;
            }

            .test-group-list:not(:empty) {
                margin: 0;
                padding: 0.2rem 0;
                list-style: none;
                border-right: solid 1px #ccc;
                white-space: nowrap;
            }

            .test-group-list li {
                display: block;
            }

            .test-group-list a {
                display: block;
                color: inherit;
                text-decoration: none;
                font-size: 0.9rem;
                margin: 0;
                padding: 0.2rem;
            }

            .test-group-list li.selected a {
                background: rgba(204, 153, 51, 0.1);
            }

            .test-group-list li:not(.selected) a:hover {
                background: #eee;
            }

            .x-overview-chart {
                width: auto;
                height: 10rem;
                margin: 1rem;
                border: solid 0px red;
            }
`;
    }
}
