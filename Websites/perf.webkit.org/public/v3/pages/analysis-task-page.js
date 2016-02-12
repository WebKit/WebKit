
class AnalysisTaskChartPane extends ChartPaneBase {
    constructor()
    {
        super('analysis-task-chart-pane');
        this._page = null;
    }

    setPage(page) { this._page = page; }
    router() { return this._page.router(); }
}

ComponentBase.defineElement('analysis-task-chart-pane', AnalysisTaskChartPane);

class AnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Analysis Task');
        this._task = null;
        this._testGroups = null;
        this._renderedTestGroups = null;
        this._testGroupLabelMap = new Map;
        this._renderedCurrentTestGroup = undefined;
        this._analysisResults = null;
        this._measurementSet = null;
        this._startPoint = null;
        this._endPoint = null;
        this._errorMessage = null;
        this._currentTestGroup = null;
        this._chartPane = this.content().querySelector('analysis-task-chart-pane').component();
        this._chartPane.setPage(this);
        this._analysisResultsViewer = this.content().querySelector('analysis-results-viewer').component();
        this._analysisResultsViewer.setTestGroupCallback(this._showTestGroup.bind(this));
        this._testGroupResultsTable = this.content().querySelector('test-group-results-table').component();
        this._taskNameLabel = this.content().querySelector('.analysis-task-name editable-text').component();
        this._taskNameLabel.setStartedEditingCallback(this._didStartEditingTaskName.bind(this));
        this._taskNameLabel.setUpdateCallback(this._updateTaskName.bind(this));

        this.content().querySelector('.test-group-retry-form').onsubmit = this._retryCurrentTestGroup.bind(this);
    }

    title() { return this._task ? this._task.label() : 'Analysis Task'; }
    routeName() { return 'analysis/task'; }

    updateFromSerializedState(state)
    {
        var self = this;
        if (state.remainingRoute) {
            var taskId = parseInt(state.remainingRoute);
            AnalysisTask.fetchById(taskId).then(this._didFetchTask.bind(this), function (error) {
                self._errorMessage = `Failed to fetch the analysis task ${state.remainingRoute}: ${error}`;
                self.render();
            });
            TestGroup.fetchByTask(taskId).then(this._didFetchTestGroups.bind(this));
            AnalysisResults.fetch(taskId).then(this._didFetchAnalysisResults.bind(this));
        } else if (state.buildRequest) {
            var buildRequestId = parseInt(state.buildRequest);
            AnalysisTask.fetchByBuildRequestId(buildRequestId).then(this._didFetchTask.bind(this)).then(function (task) {
                TestGroup.fetchByTask(task.id()).then(self._didFetchTestGroups.bind(self));
                AnalysisResults.fetch(task.id()).then(self._didFetchAnalysisResults.bind(self));
            }, function (error) {
                self._errorMessage = `Failed to fetch the analysis task for the build request ${buildRequestId}: ${error}`;
                self.render();
            });
        }
    }

    _didFetchTask(task)
    {
        console.assert(!this._task);

        this._task = task;
        var platform = task.platform();
        var metric = task.metric();
        var lastModified = platform.lastModified(metric);

        this._measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        this._measurementSet.fetchBetween(task.startTime(), task.endTime(), this._didFetchMeasurement.bind(this));

        var formatter = metric.makeFormatter(4);
        this._analysisResultsViewer.setValueFormatter(formatter);
        this._testGroupResultsTable.setValueFormatter(formatter);

        this._chartPane.configure(platform.id(), metric.id());

        var domain = ChartsPage.createDomainForAnalysisTask(task);
        this._chartPane.setOverviewDomain(domain[0], domain[1]);
        this._chartPane.setMainDomain(domain[0], domain[1]);

        this.render();

        return task;
    }

    _didFetchMeasurement()
    {
        console.assert(this._task);
        console.assert(this._measurementSet);
        var series = this._measurementSet.fetchedTimeSeries('current', false, false);
        var startPoint = series.findById(this._task.startMeasurementId());
        var endPoint = series.findById(this._task.endMeasurementId());
        if (!startPoint || !endPoint || !this._measurementSet.hasFetchedRange(startPoint.time, endPoint.time))
            return;

        this._analysisResultsViewer.setPoints(startPoint, endPoint);

        this._startPoint = startPoint;
        this._endPoint = endPoint;
        this.render();
    }

    _didFetchTestGroups(testGroups)
    {
        this._testGroups = testGroups.sort(function (a, b) { return +a.createdAt() - b.createdAt(); });
        this._currentTestGroup = testGroups.length ? testGroups[testGroups.length - 1] : null;

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

        if (this._task) {
            var v2URL = `/v2/#/analysis/task/${this._task.id()}`;
            this.content().querySelector('.error-message').innerHTML =
                `<p>To schedule a custom A/B testing, use <a href="${v2URL}">v2 UI</a>.</p>`;
        }

        this._chartPane.render();

        if (this._task) {
            this._taskNameLabel.setText(this._task.name());
            var platform = this._task.platform();
            var metric = this._task.metric();
            var anchor = this.content().querySelector('.platform-metric-names a');
            this.renderReplace(anchor, metric.fullName() + ' on ' + platform.label());
            anchor.href = this.router().url('charts', ChartsPage.createStateForAnalysisTask(this._task));
        }
        this.content().querySelector('.overview-chart').style.display = this._task ? null : 'none';
        this.content().querySelector('.test-group-view').style.display = this._task ? null : 'none';
        this._taskNameLabel.render();

        this._analysisResultsViewer.setCurrentTestGroup(this._currentTestGroup);
        this._analysisResultsViewer.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        if (this._testGroups != this._renderedTestGroups) {
            this._renderedTestGroups = this._testGroups;
            this._testGroupLabelMap.clear();

            var self = this;
            var updateTestGroupName = this._updateTestGroupName.bind(this);
            var showTestGroup = this._showTestGroup.bind(this);

            this.renderReplace(this.content().querySelector('.test-group-list'),
                this._testGroups.map(function (group) {
                    var text = new EditableText(group.label());
                    text.setStartedEditingCallback(function () { return text.render(); });
                    text.setUpdateCallback(function () { return updateTestGroupName(group); });

                    self._testGroupLabelMap.set(group, text);
                    return element('li', {class: 'test-group-list-' + group.id()},
                        link(text, group.label(), function () { showTestGroup(group); }));
                }).reverse());

            this._renderedCurrentTestGroup = null;
        }

        if (this._testGroups) {
            for (var testGroup of this._testGroups) {
                var label = this._testGroupLabelMap.get(testGroup);
                label.setText(testGroup.label());
                label.render();
            }
        }

        if (this._renderedCurrentTestGroup !== this._currentTestGroup) {
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

            this._chartPane.setMainSelection(null);
            if (this._currentTestGroup) {
                var rootSetsInTestGroup = this._currentTestGroup.requestedRootSets();
                var startTime = rootSetsInTestGroup[0].latestCommitTime();
                var endTime = rootSetsInTestGroup[rootSetsInTestGroup.length - 1].latestCommitTime();
                if (startTime != endTime)
                    this._chartPane.setMainSelection([startTime, endTime]);
            }

            this.content().querySelector('.test-group-retry-button').textContent = this._currentTestGroup ? 'Retry' : 'Confirm the change';

            var repetitionCount = this._currentTestGroup ? this._currentTestGroup.repetitionCount() : 4;
            var repetitionCountController = this.content().querySelector('.test-group-retry-repetition-count');
            repetitionCountController.value = repetitionCount;

            this._renderedCurrentTestGroup = this._currentTestGroup;
        }

        this.content().querySelector('.test-group-retry-button').disabled = !(this._currentTestGroup || this._startPoint);

        this._testGroupResultsTable.render();

        Instrumentation.endMeasuringTime('AnalysisTaskPage', 'render');
    }

    _showTestGroup(testGroup)
    {
        this._currentTestGroup = testGroup;        
        this._testGroupResultsTable.setTestGroup(this._currentTestGroup);
        this.render();
    }

    _didStartEditingTaskName()
    {
        this._taskNameLabel.render();
    }

    _updateTaskName()
    {
        console.assert(this._task);
        this._taskNameLabel.render();

        var self = this;
        return self._task.updateName(self._taskNameLabel.editedText()).then(function () {
            self.render();
        }, function (error) {
            self.render();
            alert('Failed to update the name: ' + error);
        });
    }

    _updateTestGroupName(testGroup)
    {
        var label = this._testGroupLabelMap.get(testGroup);
        label.render();

        var self = this;
        return testGroup.updateName(label.editedText()).then(function () {
            self.render();
        }, function (error) {
            self.render();
            alert('Failed to update the name: ' + error);
        });
    }

    _retryCurrentTestGroup(event)
    {
        event.preventDefault();
        console.assert(this._currentTestGroup || this._startPoint);

        var testGroupName;
        var rootSetList;
        var rootSetLabels;

        if (this._currentTestGroup) {
            var testGroup = this._currentTestGroup;
            testGroupName = this._createRetryNameForTestGroup(testGroup.name());
            rootSetList = testGroup.requestedRootSets();
            rootSetLabels = rootSetList.map(function (rootSet) { return testGroup.labelForRootSet(rootSet); });
        } else {
            testGroupName = 'Confirming the change';
            rootSetList = [this._startPoint.rootSet(), this._endPoint.rootSet()];
            rootSetLabels = ['Point 0', `Point ${this._endPoint.seriesIndex - this._startPoint.seriesIndex}`];
        }

        var rootSetsByName = {};
        for (var repository of rootSetList[0].repositories())
            rootSetsByName[repository.name()] = [];

        var setIndex = 0;
        for (var rootSet of rootSetList) {
            for (var repository of rootSet.repositories()) {
                var list = rootSetsByName[repository.name()];
                if (!list) {
                    alert(`Set ${rootSetLabels[setIndex]} specifies ${repository.label()} but set ${rootSetLabels[0]} does not.`);
                    return null;
                }
                list.push(rootSet.commitForRepository(repository).revision());
            }
            setIndex++;
            for (var name in rootSetsByName) {
                var list = rootSetsByName[name];
                if (list.length < setIndex) {
                    alert(`Set ${rootSetLabels[0]} specifies ${repository.label()} but set ${rootSetLabels[setIndex]} does not.`);
                    return null;
                }
            }
        }

        var repetitionCount = this.content().querySelector('.test-group-retry-repetition-count').value;

        TestGroup.createAndRefetchTestGroups(this._task, testGroupName, repetitionCount, rootSetsByName)
            .then(this._didFetchTestGroups.bind(this), function (error) {
            alert('Failed to create a new test group: ' + error);
        });
    }

    _createRetryNameForTestGroup(name)
    {
        var nameWithNumberMatch = name.match(/(.+?)\s*\(\s*(\d+)\s*\)\s*$/);
        var number = 1;
        if (nameWithNumberMatch) {
            name = nameWithNumberMatch[1];
            number = parseInt(nameWithNumberMatch[2]);
        }

        var newName;
        do {
            number++;
            newName = `${name} (${number})`;
        } while (this._hasDuplicateTestGroupName(newName));

        return newName;
    }

    _hasDuplicateTestGroupName(name)
    {
        console.assert(this._testGroups);
        for (var group of this._testGroups) {
            if (group.name() == name)
                return true;
        }
        return false;
    }

    static htmlTemplate()
    {
        return `
        <div class="analysis-tasl-page-container">
            <div class="analysis-tasl-page">
                <h2 class="analysis-task-name"><editable-text></editable-text></h2>
                <h3 class="platform-metric-names"><a href=""></a></h3>
                <p class="error-message"></p>
                <div class="overview-chart"><analysis-task-chart-pane></analysis-task-chart-pane></div>
                <section class="analysis-results-view">
                    <analysis-results-viewer></analysis-results-viewer>
                </section>
                <section class="test-group-view">
                    <ul class="test-group-list"></ul>
                    <div class="test-group-details">
                        <test-group-results-table></test-group-results-table>
                        <form class="test-group-retry-form">
                            <button class="test-group-retry-button" type="submit">Retry</button>
                            with
                            <select class="test-group-retry-repetition-count">
                                <option>1</option>
                                <option>2</option>
                                <option>3</option>
                                <option>4</option>
                                <option>5</option>
                                <option>6</option>
                                <option>7</option>
                                <option>8</option>
                                <option>9</option>
                                <option>10</option>
                            </select>
                            iterations per set
                        </form>
                    </div>
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
                margin-bottom: 2rem;
            }

            .test-group-details {
                display: table-cell;
                margin-bottom: 1rem;
                padding: 0;
                margin: 0;
            }

            .test-group-retry-form {
                padding: 0;
                margin: 0.5rem;
            }

            .test-group-list {
                display: table-cell;
                margin: 0;
                padding: 0.2rem 0;
                list-style: none;
                border-right: solid 1px #ccc;
                white-space: nowrap;
            }

            .test-group-list:empty {
                margin: 0;
                padding: 0;
                border-right: none;
            }

            .test-group-list > li {
                display: block;
            }

            .test-group-list > li > a {
                display: block;
                color: inherit;
                text-decoration: none;
                font-size: 0.9rem;
                margin: 0;
                padding: 0.2rem;
            }

            .test-group-list > li.selected > a {
                background: rgba(204, 153, 51, 0.1);
            }

            .test-group-list > li:not(.selected) > a:hover {
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
