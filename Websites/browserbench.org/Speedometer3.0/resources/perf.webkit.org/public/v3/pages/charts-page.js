
class ChartsPage extends PageWithHeading {
    constructor(toolbar)
    {
        console.assert(toolbar instanceof ChartsToolbar);
        super('Charts', toolbar);
        this._paneList = [];
        this._paneListChanged = false;
        this._mainDomain = null;
        this._currentRepositoryId = null;

        toolbar.setAddPaneCallback(this.insertPaneAfter.bind(this));
    }

    routeName() { return 'charts'; }

    static createStateForDashboardItem(platformId, metricId, startTime)
    {
        var state = {paneList: [[platformId, metricId]], since: startTime};
        return state;
    }

    static createDomainForAnalysisTask(task)
    {
        var diff = (task.endTime() - task.startTime()) * 0.1;
        return [task.startTime() - diff, task.endTime() + diff];
    }

    static createStateForAnalysisTask(task)
    {
        var domain = this.createDomainForAnalysisTask(task);
        var state = {
            paneList: [[task.platform().id(), task.metric().id()]],
            since: Math.round(task.startTime() - (Date.now() - task.startTime()) * 0.1),
            zoom: domain,
        };
        return state;
    }

    static createStateForConfigurationList(configurationList, startTime)
    {
        console.assert(configurationList instanceof Array);
        var state = {paneList: configurationList};
        return state;
    }

    open(state)
    {
        this.toolbar().setNumberOfDaysCallback(this.setNumberOfDaysFromToolbar.bind(this));
        super.open(state);
    }

    serializeState()
    {
        var state = {since: this.toolbar().startTime()};
        var serializedPaneList = [];
        for (var pane of this._paneList)
            serializedPaneList.push(pane.serializeState());

        if (this._mainDomain)
            state['zoom'] = this._mainDomain;
        if (serializedPaneList.length)
            state['paneList'] = serializedPaneList;

        var repository = this._currentRepositoryId;
        if (repository)
            state['repository'] = this._currentRepositoryId;

        return state;
    }

    updateFromSerializedState(state, isOpen)
    {
        var paneList = [];
        if (state.paneList instanceof Array)
            paneList = state.paneList;

        var newPaneList = this._updateChartPanesFromSerializedState(paneList);
        if (newPaneList) {
            this._paneList = newPaneList;
            this._paneListChanged = true;
            this.enqueueToRender();
        }

        this._updateDomainsFromSerializedState(state);

        this._currentRepositoryId = parseInt(state['repository']);
        var currentRepository = Repository.findById(this._currentRepositoryId);

        console.assert(this._paneList.length == paneList.length);
        for (var i = 0; i < this._paneList.length; i++) {
            this._paneList[i].updateFromSerializedState(state.paneList[i], isOpen);
            this._paneList[i].setOpenRepository(currentRepository);
        }
    }

    _updateDomainsFromSerializedState(state)
    {
        var since = parseFloat(state.since);
        var zoom = state.zoom;
        if (typeof(zoom) == 'string' && zoom.indexOf('-'))
            zoom = zoom.split('-')
        if (zoom instanceof Array && zoom.length >= 2)
            zoom = zoom.map(function (value) { return parseFloat(value); });

        if (zoom && since)
            since = Math.min(zoom[0], since);
        else if (zoom)
            since = zoom[0] - (zoom[1] - zoom[0]) / 2;

        this.toolbar().setStartTime(since);
        this.toolbar().enqueueToRender();

        this._mainDomain = zoom || null;

        this._updateOverviewDomain();
        this._updateMainDomain();
    }

    _updateChartPanesFromSerializedState(paneList)
    {
        var paneMap = {}
        for (var pane of this._paneList)
            paneMap[pane.platformId() + '-' + pane.metricId()] = pane;

        var newPaneList = [];
        var createdNewPane = false;
        for (var paneInfo of paneList) {
            var platformId = parseInt(paneInfo[0]);
            var metricId = parseInt(paneInfo[1]);
            var existingPane = paneMap[platformId + '-' + metricId];
            if (existingPane)
                newPaneList.push(existingPane);
            else {
                newPaneList.push(new ChartPane(this, platformId, metricId));
                createdNewPane = true;
            }
        }

        if (createdNewPane || newPaneList.length !== this._paneList.length)
            return newPaneList;

        for (var i = 0; i < newPaneList.length; i++) {
            if (newPaneList[i] != this._paneList[i])
                return newPaneList;
        }

        return null;
    }

    setNumberOfDaysFromToolbar(numberOfDays, shouldUpdateState)
    {
        this.toolbar().setNumberOfDays(numberOfDays, true);
        this.toolbar().enqueueToRender();
        this._updateOverviewDomain();
        this._updateMainDomain();
        if (shouldUpdateState)
            this.scheduleUrlStateUpdate();
    }

    setMainDomainFromOverviewSelection(domain, originatingPane, shouldUpdateState)
    {
        this._mainDomain = domain;
        this._updateMainDomain(originatingPane);
        if (shouldUpdateState)
            this.scheduleUrlStateUpdate();
    }

    setMainDomainFromZoom(selection, originatingPane)
    {
        this._mainDomain = selection;
        this._updateMainDomain(originatingPane);
        this.scheduleUrlStateUpdate();
    }

    mainChartSelectionDidChange(pane, shouldUpdateState)
    {
        if (shouldUpdateState)
            this.scheduleUrlStateUpdate();
    }

    mainChartIndicatorDidChange(pane, shouldUpdateState)
    {
        if (shouldUpdateState)
            this.scheduleUrlStateUpdate();
    }

    graphOptionsDidChange(pane)
    {
        this.scheduleUrlStateUpdate();
    }

    setOpenRepository(repository)
    {
        this._currentRepositoryId = repository ? repository.id() : null;
        for (var pane of this._paneList)
            pane.setOpenRepository(repository);
        this.scheduleUrlStateUpdate();
    }

    _updateOverviewDomain()
    {
        var startTime = this.toolbar().startTime();
        var endTime = this.toolbar().endTime();
        for (var pane of this._paneList)
            pane.setOverviewDomain(startTime, endTime);
    }

    _updateMainDomain(originatingPane)
    {
        var startTime = this.toolbar().startTime();
        var endTime = this.toolbar().endTime();
        if (this._mainDomain) {
            startTime = this._mainDomain[0];
            endTime = this._mainDomain[1];
        }

        for (var pane of this._paneList) {
            pane.setMainDomain(startTime, endTime);
            if (pane != originatingPane) // Don't mess up the selection state.
                pane.setOverviewSelection(this._mainDomain);
        }
    }

    closePane(pane)
    {
        var index = this._paneList.indexOf(pane);
        console.assert(index >= 0);
        this._paneList.splice(index, 1);
        this._didMutatePaneList(false);
    }

    insertPaneAfter(platform, metric, referencePane)
    {
        var newPane = new ChartPane(this, platform.id(), metric.id());
        if (referencePane) {
            var index = this._paneList.indexOf(referencePane);
            console.assert(index >= 0);
            this._paneList.splice(index + 1, 0, newPane);
        } else
            this._paneList.unshift(newPane);
        this._didMutatePaneList(true);
    }

    alternatePlatforms(platform, metric)
    {
        var existingPlatforms = {};
        for (var pane of this._paneList) {
            if (pane.metricId() == metric.id())   
                existingPlatforms[pane.platformId()] = true;
        }

        return metric.platforms().filter(function (platform) {
            return !existingPlatforms[platform.id()] && !platform.isHidden();
        });
    }

    insertBreakdownPanesAfter(platform, metric, referencePane)
    {
        console.assert(referencePane);
        var childMetrics = metric.childMetrics().filter(metric => !metric.test().isHidden());

        var index = this._paneList.indexOf(referencePane);
        console.assert(index >= 0);
        var args = [index + 1, 0];

        for (var metric of childMetrics)
            args.push(new ChartPane(this, platform.id(), metric.id()));

        this._paneList.splice.apply(this._paneList, args);
        this._didMutatePaneList(true);
    }

    canBreakdown(platform, metric)
    {
        var childMetrics = metric.childMetrics().filter(metric => !metric.test().isHidden());
        if (!childMetrics.length)
            return false;

        var existingMetrics = {};
        for (var pane of this._paneList) {
            if (pane.platformId() == platform.id())
                existingMetrics[pane.metricId()] = true;
        }

        for (var metric of childMetrics) {
            if (!existingMetrics[metric.id()])
                return true;
        }

        return false;
    }

    _didMutatePaneList(addedNewPane)
    {
        this._paneListChanged = true;
        if (addedNewPane) {
            this._updateOverviewDomain();
            this._updateMainDomain();
        }
        this.enqueueToRender();
        this.scheduleUrlStateUpdate();
    }

    render()
    {
        super.render();

        if (this._paneListChanged)
            this.renderReplace(this.content().querySelector('.pane-list'), this._paneList);

        for (var pane of this._paneList)
            pane.enqueueToRender();

        this._paneListChanged = false;
    }

    static htmlTemplate()
    {
        return `<div class="pane-list"></div>`;
    }

}
