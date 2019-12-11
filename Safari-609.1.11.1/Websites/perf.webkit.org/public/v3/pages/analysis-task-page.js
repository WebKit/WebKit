
class AnalysisTaskChartPane extends ChartPaneBase {
    constructor()
    {
        super('analysis-task-chart-pane');
        this._page = null;
        this._showForm = false;
    }

    setPage(page) { this._page = page; }
    setShowForm(show)
    {
        this._showForm = show;
        this.enqueueToRender();
    }
    router() { return this._page.router(); }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        super._mainSelectionDidChange(selection);
        if (didEndDrag)
            this.enqueueToRender();
    }

    didConstructShadowTree()
    {
        this.part('form').listenToAction('startTesting', (repetitionCount, name, commitSetMap, notifyOnCompletion) => {
            this.dispatchAction('newTestGroup', name, repetitionCount, commitSetMap, notifyOnCompletion);
        });
    }

    render()
    {
        super.render();
        const points = this._mainChart ? this._mainChart.selectedPoints('current') : null;

        this.content('form').style.display = this._showForm ? null : 'none';
        if (this._showForm) {
            const form = this.part('form');
            form.setCommitSetMap(points && points.length() >= 2 ? {'A': points.firstPoint().commitSet(), 'B': points.lastPoint().commitSet()} : null);
            form.enqueueToRender();
        }
    }

    static paneFooterTemplate() { return '<customizable-test-group-form id="form"></customizable-test-group-form>'; }

    static cssTemplate()
    {
        return super.cssTemplate() + `
            #form {
                margin: 0.5rem;
            }
        `;
    }
}

ComponentBase.defineElement('analysis-task-chart-pane', AnalysisTaskChartPane);

class AnalysisTaskResultsPane extends ComponentBase {
    constructor()
    {
        super('analysis-task-results-pane');
        this._showForm = false;
        this._repositoryList = [];
        this._renderRepositoryListLazily = new LazilyEvaluatedFunction(this._renderRepositoryList.bind(this));
        this._updateCommitViewerLazily = new LazilyEvaluatedFunction(this._updateCommitViewer.bind(this));
    }

    setPoints(startPoint, endPoint, metric)
    {
        const resultsViewer = this.part('results-viewer');
        this._repositoryList = startPoint ? Repository.sortByNamePreferringOnesWithURL(startPoint.commitSet().repositories()) : [];
        resultsViewer.setPoints(startPoint, endPoint, metric);
        resultsViewer.enqueueToRender();
    }

    setTestGroups(testGroups, currentGroup)
    {
        this.part('results-viewer').setTestGroups(testGroups, currentGroup);
        this.enqueueToRender();
    }

    setAnalysisResultsView(analysisResultsView)
    {
        this.part('results-viewer').setAnalysisResultsView(analysisResultsView);
        this.enqueueToRender();
    }

    setShowForm(show)
    {
        this._showForm = show;
        this.enqueueToRender();
    }

    didConstructShadowTree()
    {
        const resultsViewer = this.part('results-viewer');
        resultsViewer.listenToAction('testGroupClick', (testGroup) => {
            this.enqueueToRender();
            this.dispatchAction('showTestGroup', testGroup)
        });
        resultsViewer.setRangeSelectorLabels(['A', 'B']);
        resultsViewer.listenToAction('rangeSelectorClick', () => this.enqueueToRender());

        const repositoryPicker = this.content('commit-viewer-repository');
        repositoryPicker.addEventListener('change', () => this.enqueueToRender());
        this.part('commit-viewer').setShowRepositoryName(false);

        this.part('form').listenToAction('startTesting', (repetitionCount, name, commitSetMap, notifyOnCompletion) => {
            this.dispatchAction('newTestGroup', name, repetitionCount, commitSetMap, notifyOnCompletion);
        });
    }

    render()
    {
        const resultsViewer = this.part('results-viewer');

        const repositoryPicker = this._renderRepositoryListLazily.evaluate(this._repositoryList);
        const repository = Repository.findById(repositoryPicker.value);
        const range = resultsViewer.selectedRange();
        this._updateCommitViewerLazily.evaluate(repository, range['A'], range['B']);

        this.content('form').style.display = this._showForm ? null : 'none';
        if (!this._showForm)
            return;

        const selectedRange = this.part('results-viewer').selectedRange();
        const firstCommitSet = selectedRange['A'];
        const secondCommitSet = selectedRange['B'];
        const form = this.part('form');
        form.setCommitSetMap(firstCommitSet && secondCommitSet ? {'A': firstCommitSet, 'B': secondCommitSet} : null);
        form.enqueueToRender();
    }

    _renderRepositoryList(repositoryList)
    {
        const element = ComponentBase.createElement;
        const selectElement = this.content('commit-viewer-repository');
        this.renderReplace(selectElement,
            repositoryList.map((repository) => {
                return element('option', {value: repository.id()}, repository.label());
            }));
        return selectElement;
    }

    _updateCommitViewer(repository, preceedingCommitSet, lastCommitSet)
    {
        if (repository && preceedingCommitSet && lastCommitSet && !preceedingCommitSet.equals(lastCommitSet)) {
            const precedingRevision = preceedingCommitSet.revisionForRepository(repository);
            const lastRevision = lastCommitSet.revisionForRepository(repository);
            if (precedingRevision && lastRevision && precedingRevision != lastRevision) {
                this.part('commit-viewer').view(repository, precedingRevision, lastRevision);
                return;
            }
        }
        this.part('commit-viewer').view(null, null, null);
    }

    static htmlTemplate()
    {
        return `
            <div id="results-container">
                <div id="results-viewer-container">
                    <analysis-results-viewer id="results-viewer"></analysis-results-viewer>
                </div>
                <div id="commit-pane">
                    <select id="commit-viewer-repository"></select>
                    <commit-log-viewer id="commit-viewer"></commit-log-viewer>
                </div>
            </div>
            <customizable-test-group-form id="form"></customizable-test-group-form>
        `;
    }

    static cssTemplate()
    {
        return `
            #results-container {
                position: relative;
                text-align: center;
                padding-right: 20rem;
            }
            #results-viewer-container {
                overflow-x: scroll;
                overflow-y: hidden;
            }
            #commit-pane {
                position: absolute;
                width: 20rem;
                height: 100%;
                top: 0;
                right: 0;
            }
            #form {
                margin: 0.5rem;
            }
        `;
    }
}

ComponentBase.defineElement('analysis-task-results-pane', AnalysisTaskResultsPane);

class AnalysisTaskConfiguratorPane extends ComponentBase {
    constructor()
    {
        super('analysis-task-configurator-pane');
        this._currentGroup = null;
    }

    didConstructShadowTree()
    {
        const form = this.part('form');
        form.setHasTask(true);
        form.listenToAction('startTesting', (...args) => {
            this.dispatchAction('createCustomTestGroup', ...args);
        });
    }

    setTestGroups(testGroups, currentGroup)
    {
        this._currentGroup = currentGroup;
        const form = this.part('form');
        if (!form.hasCommitSets() && currentGroup)
            form.setConfigurations(currentGroup.test(), currentGroup.platform(), currentGroup.repetitionCount(), currentGroup.requestedCommitSets());
        this.enqueueToRender();
    }

    render()
    {
        super.render();
    }

    static htmlTemplate()
    {
        return `<custom-configuration-test-group-form id="form"></custom-configuration-test-group-form>`;
    }

    static cssTemplate()
    {
        return `
            #form {
                margin: 1rem;
            }
        `;
    }
};

ComponentBase.defineElement('analysis-task-configurator-pane', AnalysisTaskConfiguratorPane);

class AnalysisTaskTestGroupPane extends ComponentBase {

    constructor()
    {
        super('analysis-task-test-group-pane');
        this._renderTestGroupsLazily = new LazilyEvaluatedFunction(this._renderTestGroups.bind(this));
        this._renderTestGroupVisibilityLazily = new LazilyEvaluatedFunction(this._renderTestGroupVisibility.bind(this));
        this._renderTestGroupNamesLazily = new LazilyEvaluatedFunction(this._renderTestGroupNames.bind(this));
        this._renderCurrentTestGroupLazily = new LazilyEvaluatedFunction(this._renderCurrentTestGroup.bind(this));
        this._testGroupMap = new Map;
        this._testGroups = [];
        this._bisectingCommitSetByTestGroup = null;
        this._currentTestGroup = null;
        this._showHiddenGroups = false;
        this._allTestGroupIdSetForCurrentTask = null;
    }

    didConstructShadowTree()
    {
        this.content('hide-button').onclick = () => this.dispatchAction('toggleTestGroupVisibility', this._currentTestGroup);
        this.part('retry-form').listenToAction('startTesting', (repetitionCount, notifyOnCompletion) => {
            this.dispatchAction('retryTestGroup', this._currentTestGroup, repetitionCount, notifyOnCompletion);
        });
        this.part('bisect-form').listenToAction('startTesting', (repetitionCount, notifyOnCompletion) => {
            const bisectingCommitSet = this._bisectingCommitSetByTestGroup.get(this._currentTestGroup);
            const [oneCommitSet, anotherCommitSet] = this._currentTestGroup.requestedCommitSets();
            const commitSets = [oneCommitSet, bisectingCommitSet, anotherCommitSet];
            this.dispatchAction('bisectTestGroup', this._currentTestGroup, commitSets, repetitionCount, notifyOnCompletion);
        });
    }

    setTestGroups(testGroups, currentTestGroup, showHiddenGroups)
    {
        this._testGroups = testGroups;
        this._currentTestGroup = currentTestGroup;
        this._showHiddenGroups = showHiddenGroups;
        this.part('revision-table').setTestGroup(currentTestGroup);
        this.part('results-viewer').setTestGroup(currentTestGroup);

        const analysisTask = currentTestGroup.task();
        const allTestGroupIdsForCurrentTask = TestGroup.findAllByTask(analysisTask.id()).map((testGroup) => testGroup.id());
        const testGroupChanged = !this._allTestGroupIdSetForCurrentTask
            || this._allTestGroupIdSetForCurrentTask.size !== allTestGroupIdsForCurrentTask.length
            || !allTestGroupIdsForCurrentTask.every((testGroupId) => this._allTestGroupIdSetForCurrentTask.has(testGroupId));

        const computedForCurrentTestGroup = this._bisectingCommitSetByTestGroup && this._bisectingCommitSetByTestGroup.has(currentTestGroup);

        if (!testGroupChanged && computedForCurrentTestGroup) {
            this.enqueueToRender();
            return;
        }

        if (testGroupChanged) {
            this._bisectingCommitSetByTestGroup = new Map;
            this._allTestGroupIdSetForCurrentTask = new Set(allTestGroupIdsForCurrentTask);
        }

        analysisTask.commitSetsFromTestGroupsAndMeasurementSet().then(async (availableCommitSets) => {
            const commitSetClosestToMiddle = await CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits(currentTestGroup.requestedCommitSets(), availableCommitSets);
            this._bisectingCommitSetByTestGroup.set(currentTestGroup, commitSetClosestToMiddle);
            this.enqueueToRender();
        });
    }

    setAnalysisResults(analysisResults, metric)
    {
        this.part('revision-table').setAnalysisResults(analysisResults);
        this.part('results-viewer').setAnalysisResults(analysisResults, metric);
        this.enqueueToRender();
    }

    render()
    {
        this._renderTestGroupsLazily.evaluate(this._showHiddenGroups, ...this._testGroups);
        this._renderTestGroupVisibilityLazily.evaluate(...this._testGroups.map((group) => group.isHidden() ? 'hidden' : 'visible'));
        this._renderTestGroupNamesLazily.evaluate(...this._testGroups.map((group) => group.label()));
        this._renderCurrentTestGroup(this._currentTestGroup);
    }

    _renderTestGroups(showHiddenGroups, ...testGroups)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;

        this._testGroupMap = new Map;
        const testGroupItems = testGroups.map((group) => {
            const text = new EditableText(group.label());
            text.listenToAction('update', () => this.dispatchAction('renameTestGroup', group, text.editedText()));

            const listItem = element('li', link(text, group.label(), () => this.dispatchAction('showTestGroup', group)));

            this._testGroupMap.set(group, {text, listItem});
            return listItem;
        });

        this.renderReplace(this.content('test-group-list'), [testGroupItems,
            showHiddenGroups ? [] : element('li', {class: 'test-group-list-show-all'}, link('Show hidden tests', () => {
                this.dispatchAction('showHiddenTestGroups');
            }))]);
    }

    _renderTestGroupVisibility(...groupVisibilities)
    {
        for (let i = 0; i < groupVisibilities.length; i++)
            this._testGroupMap.get(this._testGroups[i]).listItem.className = groupVisibilities[i];
    }

    _renderTestGroupNames(...groupNames)
    {
        for (let i = 0; i < groupNames.length; i++)
            this._testGroupMap.get(this._testGroups[i]).text.setText(groupNames[i]);
    }

    _renderCurrentTestGroup(currentGroup)
    {
        const selected = this.content('test-group-list').querySelector('.selected');
        if (selected)
            selected.classList.remove('selected');
        if (currentGroup)
            this._testGroupMap.get(currentGroup).listItem.classList.add('selected');

        if (currentGroup) {
            this.part('retry-form').setRepetitionCount(currentGroup.initialRepetitionCount());
            this.part('bisect-form').setRepetitionCount(currentGroup.initialRepetitionCount());
            const statusSummary = `${currentGroup.initialRepetitionCount()} requested, ${currentGroup.repetitionCount() - currentGroup.initialRepetitionCount()} added due to failures.`;
            this.content('status-summary').innerHTML = statusSummary;

            const authoredBy = currentGroup.author() ? `by "${currentGroup.author()}"` : '';
            this.content('request-summary').innerHTML = `Scheduled ${authoredBy} at ${currentGroup.createdAt()}`
        }
        this.content('retry-form').style.display = currentGroup ? null : 'none';
        this.content('bisect-form').style.display = currentGroup && this._bisectingCommitSetByTestGroup.get(currentGroup) ? null : 'none';
        this.content('status-summary').style.display = currentGroup && currentGroup.repetitionCount() > currentGroup.initialRepetitionCount() ? null : 'none';
        this.content('request-summary').style.display = currentGroup ? null : 'none';

        const hideButton = this.content('hide-button');
        hideButton.textContent = currentGroup && currentGroup.isHidden() ? 'Unhide' : 'Hide';
        hideButton.style.display = currentGroup ? null : 'none';

        this.content('pending-request-cancel-warning').style.display = currentGroup && currentGroup.hasPending() ? null : 'none';
    }

    static htmlTemplate()
    {
        return `
            <ul id="test-group-list"></ul>
            <div id="test-group-details">
                <test-group-results-viewer id="results-viewer"></test-group-results-viewer>
                <test-group-revision-table id="revision-table"></test-group-revision-table>
                <div id="status-summary" class="summary"></div>
                <div id="request-summary" class="summary"></div>
                <test-group-form id="retry-form">Retry</test-group-form>
                <test-group-form id="bisect-form">Bisect</test-group-form>
                <button id="hide-button">Hide</button>
                <span id="pending-request-cancel-warning">(cancels pending requests)</span>
            </div>`;
    }

    static cssTemplate()
    {
        return `
            :host {
                display: flex !important;
                font-size: 0.9rem;
            }

            #test-group-list {
                flex: none;
                margin: 0;
                padding: 0.2rem 0;
                list-style: none;
                border-right: solid 1px #ccc;
                white-space: nowrap;
                min-width: 8rem;
            }

            li {
                display: block;
                font-size: 0.9rem;
            }

            li > a {
                display: block;
                color: inherit;
                text-decoration: none;
                margin: 0;
                padding: 0.2rem;
            }

            li.test-group-list-show-all {
                font-size: 0.8rem;
                margin-top: 0.5rem;
                padding-right: 1rem;
                text-align: center;
                color: #999;
            }

            li.test-group-list-show-all:not(.selected) a:hover {
                background: inherit;
            }

            li.selected > a {
                background: rgba(204, 153, 51, 0.1);
            }

            li.hidden {
                color: #999;
            }

            li:not(.selected) > a:hover {
                background: #eee;
            }

            div.summary {
                padding-left: 1rem;
            }

            #test-group-details {
                display: table-cell;
                margin-bottom: 1rem;
                padding: 0 0.5rem;
                margin: 0;
            }

            #retry-form, #bisect-form {
                display: block;
                margin: 0.5rem;
            }

            #hide-button {
                margin: 0.5rem;
            }`;
    }
}

ComponentBase.defineElement('analysis-task-test-group-pane', AnalysisTaskTestGroupPane);

class AnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Analysis Task');
        this._renderTaskNameAndStatusLazily = new LazilyEvaluatedFunction(this._renderTaskNameAndStatus.bind(this));
        this._renderCauseAndFixesLazily = new LazilyEvaluatedFunction(this._renderCauseAndFixes.bind(this));
        this._renderRelatedTasksLazily = new LazilyEvaluatedFunction(this._renderRelatedTasks.bind(this));
        this._resetVariables();
    }

    title() { return this._task ? this._task.label() : 'Analysis Task'; }
    routeName() { return 'analysis/task'; }

    _resetVariables()
    {
        this._task = null;
        this._metric = null;
        this._triggerable = null;
        this._relatedTasks = null;
        this._testGroups = null;
        this._testGroupLabelMap = new Map;
        this._analysisResults = null;
        this._measurementSet = null;
        this._startPoint = null;
        this._endPoint = null;
        this._errorMessage = null;
        this._currentTestGroup = null;
        this._filteredTestGroups = null;
        this._showHiddenTestGroups = false;
    }

    updateFromSerializedState(state)
    {
        this._resetVariables();
        if (state.remainingRoute) {
            const taskId = parseInt(state.remainingRoute);
            AnalysisTask.fetchById(taskId).then(this._didFetchTask.bind(this)).then(() => {
                this._fetchRelatedInfoForTaskId(taskId);
            }, (error) => {
                this._errorMessage = `Failed to fetch the analysis task ${state.remainingRoute}: ${error}`;
                this.enqueueToRender();
            });
        } else if (state.buildRequest) {
            const buildRequestId = parseInt(state.buildRequest);
            AnalysisTask.fetchByBuildRequestId(buildRequestId).then(this._didFetchTask.bind(this)).then((task) => {
                this._fetchRelatedInfoForTaskId(task.id());
            }, (error) => {
                this._errorMessage = `Failed to fetch the analysis task for the build request ${buildRequestId}: ${error}`;
                this.enqueueToRender();
            });
        }
    }

    didConstructShadowTree()
    {
        this.part('analysis-task-name').listenToAction('update', () => this._updateTaskName(this.part('analysis-task-name').editedText()));

        this.content('change-type-form').onsubmit = ComponentBase.createEventHandler((event) => this._updateChangeType(event));

        this.part('chart-pane').listenToAction('newTestGroup', this._createTestGroupAfterVerifyingCommitSetList.bind(this));

        const resultsPane = this.part('results-pane');
        resultsPane.listenToAction('showTestGroup', (testGroup) => this._showTestGroup(testGroup));
        resultsPane.listenToAction('newTestGroup', this._createTestGroupAfterVerifyingCommitSetList.bind(this));

        this.part('configurator-pane').listenToAction('createCustomTestGroup', this._createCustomTestGroup.bind(this));

        const groupPane = this.part('group-pane');
        groupPane.listenToAction('showTestGroup', (testGroup) => this._showTestGroup(testGroup));
        groupPane.listenToAction('showHiddenTestGroups', () => this._showAllTestGroups());
        groupPane.listenToAction('renameTestGroup', (testGroup, newName) => this._updateTestGroupName(testGroup, newName));
        groupPane.listenToAction('toggleTestGroupVisibility', (testGroup) => this._hideCurrentTestGroup(testGroup));
        groupPane.listenToAction('retryTestGroup', (testGroup, repetitionCount, notifyOnCompletion) => this._retryCurrentTestGroup(testGroup, repetitionCount, notifyOnCompletion));
        groupPane.listenToAction('bisectTestGroup', (testGroup, commitSets, repetitionCount, notifyOnCompletion) => this._bisectCurrentTestGroup(testGroup, commitSets, repetitionCount, notifyOnCompletion));

        this.part('cause-list').listenToAction('addItem', (repository, revision) => {
            this._associateCommit('cause', repository, revision);
        });
        this.part('fix-list').listenToAction('addItem', (repository, revision) => {
            this._associateCommit('fix', repository, revision);
        });
    }

    _fetchRelatedInfoForTaskId(taskId)
    {
        TestGroup.fetchForTask(taskId).then(this._didFetchTestGroups.bind(this));
        AnalysisResults.fetch(taskId).then(this._didFetchAnalysisResults.bind(this));
        AnalysisTask.fetchRelatedTasks(taskId).then((relatedTasks) => {
            this._relatedTasks = relatedTasks;
            this.enqueueToRender();
        });
    }

    _didFetchTask(task)
    {
        console.assert(!this._task);

        this._task = task;
        const bugList = this.part('bug-list');
        this.part('bug-list').setTask(this._task);
        this.enqueueToRender();

        if (task.isCustom())
            return task;

        const platform = task.platform();
        const metric = task.metric();
        const lastModified = platform.lastModified(metric);
        const triggerable = Triggerable.findByTestConfiguration(metric.test(), platform);
        this._triggerable = triggerable && !triggerable.isDisabled() ? triggerable : null;
        this._metric = metric;

        this._measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        this._measurementSet.fetchBetween(task.startTime(), task.endTime(), this._didFetchMeasurement.bind(this));

        const chart = this.part('chart-pane');
        const domain = ChartsPage.createDomainForAnalysisTask(task);
        chart.configure(platform.id(), metric.id());
        chart.setOverviewDomain(domain[0], domain[1]);
        chart.setMainDomain(domain[0], domain[1]);

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

        this.part('results-pane').setPoints(startPoint, endPoint, this._task.metric());

        this._startPoint = startPoint;
        this._endPoint = endPoint;
        this.enqueueToRender();
    }

    _didFetchTestGroups(testGroups)
    {
        this._testGroups = testGroups.sort(function (a, b) { return +a.createdAt() - b.createdAt(); });
        this._didUpdateTestGroupHiddenState();
        this._assignTestResultsIfPossible();
        this.enqueueToRender();
    }

    _showAllTestGroups()
    {
        this._showHiddenTestGroups = true;
        this._didUpdateTestGroupHiddenState();
        this.enqueueToRender();
    }

    _didUpdateTestGroupHiddenState()
    {
        if (!this._showHiddenTestGroups)
            this._filteredTestGroups = this._testGroups.filter(function (group) { return !group.isHidden(); });
        else
            this._filteredTestGroups = this._testGroups;
        this._showTestGroup(this._filteredTestGroups ? this._filteredTestGroups[this._filteredTestGroups.length - 1] : null);
    }

    _didFetchAnalysisResults(results)
    {
        this._analysisResults = results;
        if (this._assignTestResultsIfPossible())
            this.enqueueToRender();
    }

    _assignTestResultsIfPossible()
    {
        if (!this._task || !this._testGroups || !this._analysisResults)
            return false;

        let metric = this._metric;
        this.part('group-pane').setAnalysisResults(this._analysisResults, metric);
        if (metric) {
            const view = this._analysisResults.viewForMetric(metric);
            this.part('results-pane').setAnalysisResultsView(view);
        }

        return true;
    }

    render()
    {
        super.render();

        Instrumentation.startMeasuringTime('AnalysisTaskPage', 'render');

        this.content().querySelector('.error-message').textContent = this._errorMessage || '';

        this._renderTaskNameAndStatusLazily.evaluate(this._task, this._task ? this._task.name() : null, this._task ? this._task.changeType() : null);
        this._renderCauseAndFixesLazily.evaluate(this._startPoint, this._task, this.part('cause-list'), this._task ? this._task.causes() : []);
        this._renderCauseAndFixesLazily.evaluate(this._startPoint, this._task, this.part('fix-list'), this._task ? this._task.fixes() : []);
        this._renderRelatedTasksLazily.evaluate(this._task, this._relatedTasks);

        this.content('chart-pane').style.display = this._task && !this._task.isCustom() ? null : 'none';
        this.part('chart-pane').setShowForm(!!this._triggerable);

        this.content('results-pane').style.display = this._task && !this._task.isCustom() ? null : 'none';
        this.part('results-pane').setShowForm(!!this._triggerable);

        this.content('configurator-pane').style.display = this._task && this._task.isCustom() ? null : 'none';

        Instrumentation.endMeasuringTime('AnalysisTaskPage', 'render');
    }

    _renderTaskNameAndStatus(task, taskName, changeType)
    {
        this.part('analysis-task-name').setText(taskName);
        if (task && !task.isCustom()) {
            const link = ComponentBase.createLink;
            const platform = task.platform();
            const metric = task.metric();
            const subtitle = `${metric.fullName()} on ${platform.label()}`;
            this.renderReplace(this.content('platform-metric-names'),
                link(subtitle, this.router().url('charts', ChartsPage.createStateForAnalysisTask(task))));
        }
        this.content('change-type').value = changeType || 'unconfirmed';
    }

    _renderRelatedTasks(task, relatedTasks)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        this.renderReplace(this.content('related-tasks-list'), (task && relatedTasks ? relatedTasks : []).map((otherTask) => {
                let suffix = '';
                const taskLabel = otherTask.label();
                if (otherTask.metric() && otherTask.metric() != task.metric() && taskLabel.indexOf(otherTask.metric().label()) < 0)
                    suffix += ` with "${otherTask.metric().label()}"`;
                if (otherTask.platform() && otherTask.platform() != task.platform() && taskLabel.indexOf(otherTask.platform().label()) < 0)
                    suffix += ` on ${otherTask.platform().label()}`;
                return element('li', [link(taskLabel, this.router().url(`analysis/task/${otherTask.id()}`)), suffix]);
            }));
    }

    _renderCauseAndFixes(startPoint, task, list, commits)
    {
        const hasData = startPoint && task;
        this.content('cause-fix').style.display = hasData ? null : 'none';
        if (!hasData)
            return;

        const commitSet = startPoint.commitSet();
        const repositoryList = Repository.sortByNamePreferringOnesWithURL(commitSet.repositories());

        const makeItem = (commit) => {
            return new MutableListItem(commit.repository(), commit.label(), commit.title(), commit.url(),
                'Disassociate this commit', this._dissociateCommit.bind(this, commit));
        }

        list.setKindList(repositoryList);
        list.setList(commits.map((commit) => makeItem(commit)));
    }

    _showTestGroup(testGroup)
    {
        this._currentTestGroup = testGroup;
        this.part('configurator-pane').setTestGroups(this._filteredTestGroups, this._currentTestGroup);
        this.part('results-pane').setTestGroups(this._filteredTestGroups, this._currentTestGroup);
        const groupsInReverseChronology = this._filteredTestGroups.slice(0).reverse();
        const showHiddenGroups = !this._testGroups.some((group) => group.isHidden()) || this._showHiddenTestGroups;
        this.part('group-pane').setTestGroups(groupsInReverseChronology, this._currentTestGroup, showHiddenGroups);
        this.enqueueToRender();
    }

    _updateTaskName(newName)
    {
        console.assert(this._task);

        return this._task.updateName(newName).then(() => {
            this.enqueueToRender();
        }, (error) => {
            this.enqueueToRender();
            alert('Failed to update the name: ' + error);
        });
    }

    _updateTestGroupName(testGroup, newName)
    {
        return testGroup.updateName(newName).then(() => {
            this._showTestGroup(this._currentTestGroup);
            this.enqueueToRender();
        }, (error) => {
            this.enqueueToRender();
            alert('Failed to hide the test name: ' + error);
        });
    }

    _hideCurrentTestGroup(testGroup)
    {
        return testGroup.updateHiddenFlag(!testGroup.isHidden()).then(() => {
            this._didUpdateTestGroupHiddenState();
            this.enqueueToRender();
        }, function (error) {
            this._mayHaveMutatedTestGroupHiddenState();
            this.enqueueToRender();
            alert('Failed to update the group: ' + error);
        });
    }

    _updateChangeType(event)
    {
        event.preventDefault();
        console.assert(this._task);

        let newChangeType = this.content('change-type').value;
        if (newChangeType == 'unconfirmed')
            newChangeType = null;

        const updateRendering = () => {
            this.part('chart-pane').didUpdateAnnotations();
            this.enqueueToRender();
        };
        return this._task.updateChangeType(newChangeType).then(updateRendering, (error) => {
            updateRendering();
            alert('Failed to update the status: ' + error);
        });
    }

    _associateCommit(kind, repository, revision)
    {
        const updateRendering = () => { this.enqueueToRender(); };
        return this._task.associateCommit(kind, repository, revision).then(updateRendering, (error) => {
            updateRendering();
            if (error == 'AmbiguousRevision')
                alert('There are multiple revisions that match the specified string: ' + revision);
            else if (error == 'CommitNotFound')
                alert('There are no revisions that match the specified string:' + revision);
            else
                alert('Failed to associate the commit: ' + error);
        });
    }

    _dissociateCommit(commit)
    {
        const updateRendering = () => { this.enqueueToRender(); };
        return this._task.dissociateCommit(commit).then(updateRendering, (error) => {
            updateRendering();
            alert('Failed to dissociate the commit: ' + error);
        });
    }

    _retryCurrentTestGroup(testGroup, repetitionCount, notifyOnCompletion)
    {
        const existingNames = (this._testGroups || []).map((group) => group.name());
        const newName = CommitSet.createNameWithoutCollision(testGroup.name(), new Set(existingNames));
        const commitSetList = testGroup.requestedCommitSets();
        const platform = this._task.platform() || testGroup.platform();
        return TestGroup.createWithCustomConfiguration(this._task, platform, testGroup.test(), newName, repetitionCount, commitSetList, notifyOnCompletion)
            .then(this._didFetchTestGroups.bind(this), function (error) {
            alert('Failed to create a new test group: ' + error);
        });
    }

    async _bisectCurrentTestGroup(testGroup, commitSets, repetitionCount, notifyOnCompletion)
    {
        console.assert(testGroup.task());
        const existingTestGroupNames = new Set((this._testGroups || []).map((testGroup) => testGroup.name()));

        for (let i = 1; i < commitSets.length; i++) {
            const previousCommitSet = commitSets[i - 1];
            const currentCommitSet = commitSets[i];
            const testGroupName = CommitSet.createNameWithoutCollision(CommitSet.diff(previousCommitSet, currentCommitSet), existingTestGroupNames);
            try {
                const testGroups = await TestGroup.createAndRefetchTestGroups(testGroup.task(), testGroupName, repetitionCount, [previousCommitSet, currentCommitSet], notifyOnCompletion);
                await this._didFetchTestGroups(testGroups);
            } catch(error) {
                alert('Failed to create a new test group: ' + error);
                break;
            }
        }
    }

    _createTestGroupAfterVerifyingCommitSetList(testGroupName, repetitionCount, commitSetMap, notifyOnCompletion)
    {
        if (this._hasDuplicateTestGroupName(testGroupName)) {
            alert(`There is already a test group named "${testGroupName}"`);
            return;
        }

        const firstLabel = Object.keys(commitSetMap)[0];
        const firstCommitSet = commitSetMap[firstLabel];

        for (let currentLabel in commitSetMap) {
            const commitSet = commitSetMap[currentLabel];
            for (let repository of commitSet.repositories()) {
                if (!firstCommitSet.revisionForRepository(repository))
                    return alert(`Set ${currentLabel} specifies ${repository.label()} but set ${firstLabel} does not.`);
            }
            for (let repository of firstCommitSet.repositories()) {
                if (!commitSet.revisionForRepository(repository))
                    return alert(`Set ${firstLabel} specifies ${repository.label()} but set ${currentLabel} does not.`);
            }
        }

        const commitSets = [];
        for (let label in commitSetMap)
            commitSets.push(commitSetMap[label]);

        return TestGroup.createAndRefetchTestGroups(this._task, testGroupName, repetitionCount, commitSets, notifyOnCompletion)
            .then(this._didFetchTestGroups.bind(this), function (error) {
            alert('Failed to create a new test group: ' + error);
        });
    }

    _createCustomTestGroup(repetitionCount, testGroupName, commitSets, platform, test, notifyOnCompletion)
    {
        console.assert(this._task.isCustom());
        if (this._hasDuplicateTestGroupName(testGroupName)) {
            alert(`There is already a test group named "${testGroupName}"`);
            return;
        }

        TestGroup.createWithCustomConfiguration(this._task, platform, test, testGroupName, repetitionCount, commitSets, notifyOnCompletion)
            .then(this._didFetchTestGroups.bind(this), function (error) {
            alert('Failed to create a new test group: ' + error);
        });
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
                <h2 class="analysis-task-name"><editable-text id="analysis-task-name"></editable-text></h2>
                <h3 id="platform-metric-names"></h3>
                <p class="error-message"></p>
                <div class="analysis-task-status">
                    <section>
                        <h3>Status</h3>
                        <form id="change-type-form">
                            <select id="change-type">
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
                        <analysis-task-bug-list id="bug-list"></analysis-task-bug-list>
                    </section>
                    <section id="cause-fix">
                        <h3>Caused by</h3>
                        <mutable-list-view id="cause-list"></mutable-list-view>
                        <h3>Fixed by</h3>
                        <mutable-list-view id="fix-list"></mutable-list-view>
                    </section>
                    <section class="related-tasks">
                        <h3>Related Tasks</h3>
                        <ul id="related-tasks-list"></ul>
                    </section>
                </div>
                <analysis-task-chart-pane id="chart-pane"></analysis-task-chart-pane>
                <analysis-task-results-pane id="results-pane"></analysis-task-results-pane>
                <analysis-task-configurator-pane id="configurator-pane"></analysis-task-configurator-pane>
                <analysis-task-test-group-pane id="group-pane"></analysis-task-test-group-pane>
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

            #platform-metric-names {
                font-size: 1rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            #platform-metric-names a {
                text-decoration: none;
                color: inherit;
            }

            #platform-metric-names:empty {
                visibility: hidden;
                height: 0;
                width: 0;
                /* FIXME: Use display: none instead once r214290 is shipped everywhere */
            }

            .error-message:empty {
                visibility: hidden;
                height: 0;
                width: 0;
                /* FIXME: Use display: none instead once r214290 is shipped everywhere */
            }

            .error-message:not(:empty) {
                margin: 1rem;
                padding: 0;
            }

            #chart-pane,
            #results-pane {
                display: block;
                padding: 0 1rem;
                border-bottom: solid 1px #ccc;
            }

            #results-pane {
                margin-top: 1rem;
            }

            #group-pane {
                margin: 1rem;
                margin-bottom: 2rem;
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

            .test-configuration h3 {
                font-size: 1rem;
                font-weight: inherit;
                color: inherit;
                margin: 0 1rem;
                padding: 0;
            }`;
    }
}
