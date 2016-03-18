
class AnalysisTaskChartPane extends ChartPaneBase {
    constructor()
    {
        super('analysis-task-chart-pane');
        this._page = null;
    }

    setPage(page) { this._page = page; }
    router() { return this._page.router(); }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        super._mainSelectionDidChange(selection);
        if (didEndDrag)
            this._page._chartSelectionDidChange();
    }

    selectedPoints()
    {
        var selection = this._mainChart ? this._mainChart.currentSelection() : null;
        if (!selection)
            return null;

        return this._mainChart.sampledDataBetween('current', selection[0], selection[1]);
    }
}

ComponentBase.defineElement('analysis-task-chart-pane', AnalysisTaskChartPane);

class AnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Analysis Task');
        this._task = null;
        this._relatedTasks = null;
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
        this._filteredTestGroups = null;
        this._showHiddenTestGroups = false;

        this._chartPane = this.content().querySelector('analysis-task-chart-pane').component();
        this._chartPane.setPage(this);
        this._analysisResultsViewer = this.content().querySelector('analysis-results-viewer').component();
        this._analysisResultsViewer.setTestGroupCallback(this._showTestGroup.bind(this));
        this._analysisResultsViewer.setRangeSelectorLabels(['A', 'B']);
        this._analysisResultsViewer.setRangeSelectorCallback(this._selectedRowInAnalysisResultsViewer.bind(this));
        this._testGroupResultsTable = this.content().querySelector('test-group-results-table').component();
        this._taskNameLabel = this.content().querySelector('.analysis-task-name editable-text').component();
        this._taskNameLabel.setStartedEditingCallback(this._didStartEditingTaskName.bind(this));
        this._taskNameLabel.setUpdateCallback(this._updateTaskName.bind(this));

        this.content().querySelector('.change-type-form').onsubmit = this._updateChangeType.bind(this);
        this._taskStatusControl = this.content().querySelector('.change-type-form select');

        this._bugList = this.content().querySelector('.associated-bugs mutable-list-view').component();
        this._bugList.setKindList(BugTracker.all());
        this._bugList.setAddCallback(this._associateBug.bind(this));

        this._causeList = this.content().querySelector('.cause-list mutable-list-view').component();
        this._causeList.setAddCallback(this._associateCommit.bind(this, 'cause'));

        this._fixList = this.content().querySelector('.fix-list mutable-list-view').component();
        this._fixList.setAddCallback(this._associateCommit.bind(this, 'fix'));

        this._newTestGroupFormForChart = this.content().querySelector('.overview-chart customizable-test-group-form').component();
        this._newTestGroupFormForChart.setStartCallback(this._createNewTestGroupFromChart.bind(this));

        this._newTestGroupFormForViewer = this.content().querySelector('.analysis-results-view customizable-test-group-form').component();
        this._newTestGroupFormForViewer.setStartCallback(this._createNewTestGroupFromViewer.bind(this));

        this._retryForm = this.content().querySelector('.test-group-retry-form test-group-form').component();
        this._retryForm.setStartCallback(this._retryCurrentTestGroup.bind(this));
        this._hideButton = this.content().querySelector('.test-group-hide-button');
        this._hideButton.onclick = this._hideCurrentTestGroup.bind(this);
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
            this._fetchRelatedInfoForTaskId(taskId);
        } else if (state.buildRequest) {
            var buildRequestId = parseInt(state.buildRequest);
            AnalysisTask.fetchByBuildRequestId(buildRequestId).then(this._didFetchTask.bind(this)).then(function (task) {
                self._fetchRelatedInfoForTaskId(task.id());
            }, function (error) {
                self._errorMessage = `Failed to fetch the analysis task for the build request ${buildRequestId}: ${error}`;
                self.render();
            });
        }
    }

    _fetchRelatedInfoForTaskId(taskId)
    {
        TestGroup.fetchByTask(taskId).then(this._didFetchTestGroups.bind(this));
        AnalysisResults.fetch(taskId).then(this._didFetchAnalysisResults.bind(this));
        AnalysisTask.fetchRelatedTasks(taskId).then(this._didFetchRelatedAnalysisTasks.bind(this));
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

    _didFetchRelatedAnalysisTasks(relatedTasks)
    {
        this._relatedTasks = relatedTasks;
        this.render();
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
        this._didUpdateTestGroupHiddenState();
        this._assignTestResultsIfPossible();
        this.render();
    }

    _showAllTestGroups()
    {
        this._showHiddenTestGroups = true;
        this._didUpdateTestGroupHiddenState();
        this.render();
    }

    _didUpdateTestGroupHiddenState()
    {
        this._renderedCurrentTestGroup = null;
        this._renderedTestGroups = null;
        if (!this._showHiddenTestGroups)
            this._filteredTestGroups = this._testGroups.filter(function (group) { return !group.isHidden(); });
        else
            this._filteredTestGroups = this._testGroups;
        this._currentTestGroup = this._filteredTestGroups ? this._filteredTestGroups[this._filteredTestGroups.length - 1] : null;
        this._analysisResultsViewer.setTestGroups(this._filteredTestGroups);
        this._testGroupResultsTable.setTestGroup(this._currentTestGroup);
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

        this._chartPane.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        if (this._task) {
            this._taskNameLabel.setText(this._task.name());
            var platform = this._task.platform();
            var metric = this._task.metric();
            var anchor = this.content().querySelector('.platform-metric-names a');
            this.renderReplace(anchor, metric.fullName() + ' on ' + platform.label());
            anchor.href = this.router().url('charts', ChartsPage.createStateForAnalysisTask(this._task));

            var self = this;
            this._bugList.setList(this._task.bugs().map(function (bug) {
                return new MutableListItem(bug.bugTracker(), bug.label(), bug.title(), bug.url(),
                    'Dissociate this bug', self._dissociateBug.bind(self, bug));
            }));

            this._causeList.setList(this._task.causes().map(this._makeCommitListItem.bind(this)));
            this._fixList.setList(this._task.fixes().map(this._makeCommitListItem.bind(this)));

            this._taskStatusControl.value = this._task.changeType() || 'unconfirmed';
        }

        var repositoryList;
        if (this._startPoint) {
            var rootSet = this._startPoint.rootSet();
            repositoryList = Repository.sortByNamePreferringOnesWithURL(rootSet.repositories());
        } else
            repositoryList = Repository.sortByNamePreferringOnesWithURL(Repository.all());

        this._bugList.render();

        this._causeList.setKindList(repositoryList);
        this._causeList.render();

        this._fixList.setKindList(repositoryList);
        this._fixList.render();

        this.content().querySelector('.analysis-task-status').style.display = this._task ? null : 'none';
        this.content().querySelector('.overview-chart').style.display = this._task ? null : 'none';
        this.content().querySelector('.test-group-view').style.display = this._task ? null : 'none';
        this._taskNameLabel.render();

        if (this._relatedTasks && this._task) {
            var router = this.router();
            var link = ComponentBase.createLink;
            var thisTask = this._task;
            this.renderReplace(this.content().querySelector('.related-tasks-list'),
                this._relatedTasks.map(function (otherTask) {
                    console.assert(otherTask.metric() == thisTask.metric());
                    var suffix = '';
                    var taskLabel = otherTask.label();
                    if (otherTask.platform() != thisTask.platform() && taskLabel.indexOf(otherTask.platform().label()) < 0)
                        suffix = ` on ${otherTask.platform().label()}`;
                    return element('li', [link(taskLabel, router.url(`analysis/task/${otherTask.id()}`)), suffix]);
                }));
        }

        var selectedRange = this._analysisResultsViewer.selectedRange();
        var a = selectedRange['A'];
        var b = selectedRange['B'];
        this._newTestGroupFormForViewer.setRootSetMap(a && b ? {'A': a.rootSet(), 'B': b.rootSet()} : null);
        this._newTestGroupFormForViewer.render();

        this._renderTestGroupList();
        this._renderTestGroupDetails();

        var points = this._chartPane.selectedPoints();
        this._newTestGroupFormForChart.setRootSetMap(points && points.length >= 2 ?
                {'A': points[0].rootSet(), 'B': points[points.length - 1].rootSet()} : null);
        this._newTestGroupFormForChart.render();

        this._analysisResultsViewer.setCurrentTestGroup(this._currentTestGroup);
        this._analysisResultsViewer.render();

        this._testGroupResultsTable.render();

        Instrumentation.endMeasuringTime('AnalysisTaskPage', 'render');
    }

    _makeCommitListItem(commit)
    {
        return new MutableListItem(commit.repository(), commit.label(), commit.title(), commit.url(),
            'Disassociate this commit', this._dissociateCommit.bind(this, commit));
    }

    _renderTestGroupList()
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        if (this._testGroups != this._renderedTestGroups) {
            this._renderedTestGroups = this._testGroups;
            this._testGroupLabelMap.clear();

            var unhiddenTestGroups = this._filteredTestGroups.filter(function (group) { return !group.isHidden(); });
            var hiddenTestGroups = this._filteredTestGroups.filter(function (group) { return group.isHidden(); });

            var listItems = [];
            for (var group of hiddenTestGroups)
                listItems.unshift(this._createTestGroupListItem(group));
            for (var group of unhiddenTestGroups)
                listItems.unshift(this._createTestGroupListItem(group));

            if (this._testGroups.length != this._filteredTestGroups.length) {
                listItems.push(element('li', {class: 'test-group-list-show-all'},
                    link('Show hidden tests', this._showAllTestGroups.bind(this))));
            }

            this.renderReplace(this.content().querySelector('.test-group-list'), listItems);

            this._renderedCurrentTestGroup = null;
        }

        if (this._testGroups) {
            for (var testGroup of this._filteredTestGroups) {
                var label = this._testGroupLabelMap.get(testGroup);
                label.setText(testGroup.label());
                label.render();
            }
        }
    }

    _createTestGroupListItem(group)
    {
        var text = new EditableText(group.label());
        text.setStartedEditingCallback(function () { return text.render(); });
        text.setUpdateCallback(this._updateTestGroupName.bind(this, group));

        this._testGroupLabelMap.set(group, text);
        return ComponentBase.createElement('li', {class: 'test-group-list-' + group.id()},
            ComponentBase.createLink(text, group.label(), this._showTestGroup.bind(this, group)));
    }

    _renderTestGroupDetails()
    {
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

            this._retryForm.setLabel('Retry');
            if (this._currentTestGroup)
                this._retryForm.setRepetitionCount(this._currentTestGroup.repetitionCount());
            this._retryForm.element().style.display = this._currentTestGroup ? null : 'none';

            this.content().querySelector('.test-group-hide-button').textContent
                = this._currentTestGroup && this._currentTestGroup.isHidden() ? 'Unhide' : 'Hide';

            this.content().querySelector('.pending-request-cancel-warning').style.display
                = this._currentTestGroup && this._currentTestGroup.hasPending() ? null : 'none';

            this._renderedCurrentTestGroup = this._currentTestGroup;
        }
        this._retryForm.render();
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
            alert('Failed to hide the test name: ' + error);
        });
    }

    _hideCurrentTestGroup()
    {
        var self = this;
        console.assert(this._currentTestGroup);
        return this._currentTestGroup.updateHiddenFlag(!this._currentTestGroup.isHidden()).then(function () {
            self._didUpdateTestGroupHiddenState();
            self.render();
        }, function (error) {
            self._mayHaveMutatedTestGroupHiddenState();
            self.render();
            alert('Failed to update the group: ' + error);
        });
    }

    _updateChangeType(event)
    {
        event.preventDefault();
        console.assert(this._task);

        var newChangeType = this._taskStatusControl.value;
        if (newChangeType == 'unconfirmed')
            newChangeType = null;

        var render = this.render.bind(this);
        return this._task.updateChangeType(newChangeType).then(render, function (error) {
            render();
            alert('Failed to update the status: ' + error);
        });
    }

    _associateBug(tracker, bugNumber)
    {
        console.assert(tracker instanceof BugTracker);
        bugNumber = parseInt(bugNumber);

        var render = this.render.bind(this);
        return this._task.associateBug(tracker, bugNumber).then(render, function (error) {
            render();
            alert('Failed to associate the bug: ' + error);
        });
    }

    _dissociateBug(bug)
    {
        var render = this.render.bind(this);
        return this._task.dissociateBug(bug).then(render, function (error) {
            render();
            alert('Failed to dissociate the bug: ' + error);
        });
    }

    _associateCommit(kind, repository, revision)
    {
        var render = this.render.bind(this);
        return this._task.associateCommit(kind, repository, revision).then(render, function (error) {
            render();
            alert('Failed to associate the commit: ' + error);
        });
    }

    _dissociateCommit(commit)
    {
        var render = this.render.bind(this);
        return this._task.dissociateCommit(commit).then(render, function (error) {
            render();
            alert('Failed to dissociate the commit: ' + error);
        });
    }

    _retryCurrentTestGroup(repetitionCount)
    {
        console.assert(this._currentTestGroup);
        var testGroup = this._currentTestGroup;
        var newName = this._createRetryNameForTestGroup(testGroup.name());
        var rootSetList = testGroup.requestedRootSets();

        var rootSetMap = {};
        for (var rootSet of rootSetList)
            rootSetMap[testGroup.labelForRootSet(rootSet)] = rootSet;

        return this._createTestGroupAfterVerifyingRootSetList(newName, repetitionCount, rootSetMap);
    }

    _chartSelectionDidChange()
    {
        this.render();
    }

    _createNewTestGroupFromChart(name, repetitionCount, rootSetMap)
    {
        return this._createTestGroupAfterVerifyingRootSetList(name, repetitionCount, rootSetMap);
    }

    _selectedRowInAnalysisResultsViewer()
    {
        this.render();
    }

    _createNewTestGroupFromViewer(name, repetitionCount, rootSetMap)
    {
        return this._createTestGroupAfterVerifyingRootSetList(name, repetitionCount, rootSetMap);
    }

    _createTestGroupAfterVerifyingRootSetList(testGroupName, repetitionCount, rootSetMap)
    {
        if (this._hasDuplicateTestGroupName(testGroupName))
            alert(`There is already a test group named "${testGroupName}"`);

        var rootSetsByName = {};
        var firstLabel;
        for (firstLabel in rootSetMap) {
            var rootSet = rootSetMap[firstLabel];
            for (var repository of rootSet.repositories())
                rootSetsByName[repository.name()] = [];
            break;
        }

        var setIndex = 0;
        for (var label in rootSetMap) {
            var rootSet = rootSetMap[label];
            for (var repository of rootSet.repositories()) {
                var list = rootSetsByName[repository.name()];
                if (!list) {
                    alert(`Set ${label} specifies ${repository.label()} but set ${firstLabel} does not.`);
                    return null;
                }
                list.push(rootSet.revisionForRepository(repository));
            }
            setIndex++;
            for (var name in rootSetsByName) {
                var list = rootSetsByName[name];
                if (list.length < setIndex) {
                    alert(`Set ${firstLabel} specifies ${name} but set ${label} does not.`);
                    return null;
                }
            }
        }

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
            <div class="analysis-task-page">
                <h2 class="analysis-task-name"><editable-text></editable-text></h2>
                <h3 class="platform-metric-names"><a href=""></a></h3>
                <p class="error-message"></p>
                <div class="analysis-task-status">
                    <section>
                        <h3>Status</h3>
                        <form class="change-type-form">
                            <select>
                                <option value="unconfirmed">Unconfirmed</option>
                                <option value="regression">Definite regression</option>
                                <option value="progression">Definite progression</option>
                                <option value="inconclusive">Inconclusive (Closed)</option>
                                <option value="unchanged">No change (Closed)</option>
                            </select>
                            <button type="submit">Save</button>
                        </form>
                    </section>
                    <section class="associated-bugs">
                        <h3>Associated Bugs</h3>
                        <mutable-list-view></mutable-list-view>
                    </section>
                    <section class="cause-fix">
                        <h3>Caused by</h3>
                        <span class="cause-list"><mutable-list-view></mutable-list-view></span>
                        <h3>Fixed by</h3>
                        <span class="fix-list"><mutable-list-view></mutable-list-view></span>
                    </section>
                    <section class="related-tasks">
                        <h3>Related Tasks</h3>
                        <ul class="related-tasks-list"></ul>
                    </section>
                </div>
                <section class="overview-chart">
                    <analysis-task-chart-pane></analysis-task-chart-pane>
                    <div class="new-test-group-form"><customizable-test-group-form></customizable-test-group-form></div>
                </section>
                <section class="analysis-results-view">
                    <analysis-results-viewer></analysis-results-viewer>
                    <div class="new-test-group-form"><customizable-test-group-form></customizable-test-group-form></div>
                </section>
                <section class="test-group-view">
                    <ul class="test-group-list"></ul>
                    <div class="test-group-details">
                        <test-group-results-table></test-group-results-table>
                        <div class="test-group-retry-form"><test-group-form></test-group-form></div>
                        <button class="test-group-hide-button">Hide</button>
                        <span class="pending-request-cancel-warning">(cancels pending requests)</span>
                    </div>
                </section>
            </div>
`;
    }

    static cssTemplate()
    {
        return `
            .analysis-task-page {
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

            .overview-chart {
                margin: 0 1rem;
            }

            .analysis-task-status {
                margin: 0;
                display: flex;
                padding-bottom: 1rem;
                margin-bottom: 1rem;
                border-bottom: solid 1px #ccc;
            }

            .analysis-task-status > section {
                flex-grow: 1;
                flex-shrink: 0;
                border-left: solid 1px #eee;
                padding-left: 1rem;
                padding-right: 1rem;
            }

            .analysis-task-status > section.related-tasks {
                flex-shrink: 1;
            }

            .analysis-task-status > section:first-child {
                border-left: none;
            }

            .analysis-task-status h3 {
                font-size: 1rem;
                font-weight: inherit;
                color: #c93;
            }

            .analysis-task-status ul,
            .analysis-task-status li {
                list-style: none;
                padding: 0;
                margin: 0;
            }

            .related-tasks-list {
                max-height: 10rem;
                overflow-y: scroll;
            }


            .analysis-results-view {
                border-top: solid 1px #ccc;
                border-bottom: solid 1px #ccc;
                margin: 1rem 0;
                padding: 1rem;
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

            .new-test-group-form,
            .test-group-retry-form {
                padding: 0;
                margin: 0.5rem;
            }

            .test-group-hide-button {
                margin: 0.5rem;
            }

            .test-group-list {
                display: table-cell;
                margin: 0;
                padding: 0.2rem 0;
                list-style: none;
                border-right: solid 1px #ccc;
                white-space: nowrap;
                min-width: 8rem;
            }

            .test-group-list:empty {
                margin: 0;
                padding: 0;
                border-right: none;
            }

            .test-group-list > li {
                display: block;
                font-size: 0.9rem;
            }

            .test-group-list > li > a {
                display: block;
                color: inherit;
                text-decoration: none;
                margin: 0;
                padding: 0.2rem;
            }
            
            .test-group-list > li.test-group-list-show-all {
                font-size: 0.8rem;
                margin-top: 0.5rem;
                padding-right: 1rem;
                text-align: center;
                color: #999;
            }

            .test-group-list > li.test-group-list-show-all:not(.selected) a:hover {
                background: inherit;
            }

            .test-group-list > li.selected > a {
                background: rgba(204, 153, 51, 0.1);
            }

            .test-group-list > li:not(.selected) > a:hover {
                background: #eee;
            }
`;
    }
}
