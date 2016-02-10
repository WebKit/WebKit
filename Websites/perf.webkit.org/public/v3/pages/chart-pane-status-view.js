
class ChartPaneStatusView extends ChartStatusView {
    
    constructor(metric, chart, revisionCallback)
    {
        super(metric, chart);

        this._buildLabel = null;
        this._buildUrl = null;

        this._revisionList = [];
        this._currentRepository = null;
        this._revisionCallback = revisionCallback;
        this._pointsRangeForAnalysis = null;

        this._renderedRevisionList = null;
        this._renderedRepository = null;

        this._usedRevisionRange = null;
    }

    pointsRangeForAnalysis() { return this._pointsRangeForAnalysis; }

    render()
    {
        super.render();

        if (this._renderedRevisionList == this._revisionList && this._renderedRepository == this._currentRepository)
            return;
        this._renderedRevisionList = this._revisionList;    
        this._renderedRepository = this._currentRepository;

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var self = this;
        var buildInfo = this._buildInfo;
        var tableContent = this._revisionList.map(function (info, rowIndex) {
            var selected = info.repository == self._currentRepository;
            var action = function () {
                if (self._currentRepository == info.repository)
                    self._setRevisionRange(null, null, null);
                else
                    self._setRevisionRange(info.repository, info.from, info.to);
            };

            return element('tr', {class: selected ? 'selected' : '', onclick: action}, [
                element('td', info.name),
                element('td', info.url ? link(info.label, info.label, info.url, true) : info.label),
                element('td', {class: 'commit-viewer-opener'}, link('\u00BB', action)),
            ]);
        });

        if (this._buildInfo) {
            var number = this._buildInfo.buildNumber();
            var builder = Builder.findById(this._buildInfo.builderId());
            var url = null;
            if (builder)
                url = builder.urlForBuild(number);
            var buildTime = this._buildInfo.formattedBuildTime();

            tableContent.unshift(element('tr', [
                element('td', 'Build'),
                element('td', {colspan: 2}, [
                    url ? link(number, `Build ${number} on "${builder.name()}"`, url, true) : number,
                    ` (${buildTime})`]),
            ]));
        }

        this.renderReplace(this.content().querySelector('.chart-pane-revisions'), tableContent);
    }

    setCurrentRepository(repository)
    {
        this._currentRepository = repository;
        this._forceRender = true;
    }

    _setRevisionRange(repository, from, to)
    {
        if (this._usedRevisionRange && this._usedRevisionRange[0] == repository
            && this._usedRevisionRange[1] == from && this._usedRevisionRange[2] == to)
            return;
        this._usedRevisionRange = [repository, from, to];
        this._revisionCallback(repository, from, to);
    }

    moveRepositoryWithNotification(forward)
    {
        var currentRepository = this._currentRepository;
        if (!currentRepository)
            return false;
        var index = this._revisionList.findIndex(function (info) { return info.repository == currentRepository; });
        console.assert(index >= 0);

        var newIndex = index + (forward ? 1 : -1);
        newIndex = Math.min(this._revisionList.length - 1, Math.max(0, newIndex));
        if (newIndex == index)
            return false;

        var info = this._revisionList[newIndex];
        this.setCurrentRepository(info.repository);
        this.updateRevisionListWithNotification();
    }

    updateRevisionListWithNotification()
    {
        if (!this._currentRepository)
            return;

        this.updateStatusIfNeeded();

        this._forceRender = true;
        for (var info of this._revisionList) {
            if (info.repository == this._currentRepository) {
                this._setRevisionRange(info.repository, info.from, info.to);
                return;
            }
        }
        this._setRevisionRange(this._currentRepository, null, null);
    }

    computeChartStatusLabels(currentPoint, previousPoint)
    {
        super.computeChartStatusLabels(currentPoint, previousPoint);

        this._buildInfo = null;
        this._revisionList = [];
        this._pointsRangeForAnalysis = null;

        if (!currentPoint)
            return;

        var currentMeasurement = currentPoint.measurement();
        if (!currentMeasurement)
            return;

        if (!this._chart.currentSelection() && currentMeasurement)
            this._buildInfo = currentMeasurement;

        if (currentPoint && previousPoint && this._chart.currentSelection()) {
            this._pointsRangeForAnalysis = {
                startPointId: previousPoint.id,
                endPointId: currentPoint.id,
            };
        }

        // FIXME: Rewrite the interface to obtain the list of revision changes.
        var previousMeasurement = previousPoint ? previousPoint.measurement() : null;

        var revisions = currentMeasurement.formattedRevisions(previousMeasurement);
        var revisionList = [];
        for (var repositoryId in revisions) {
            var repository = Repository.findById(repositoryId);
            var revision = revisions[repositoryId];
            var url = revision.previousRevision ? repository.urlForRevisionRange(revision.previousRevision, revision.currentRevision) : '';
            if (!url)
                url = repository.urlForRevision(revision.currentRevision);

            revisionList.push({
                from: revision.previousRevision,
                to: revision.currentRevision,
                repository: repository,
                name: repository.name(),
                label: revision.label,
                url: url,
            });
        }

        // Sort by repository names preferring ones with URL.
        revisionList = revisionList.sort(function (a, b) {
            if (!!a.url == !!b.url) {
                if (a.name > b.name)
                    return 1;
                else if (a.name < b.name)
                    return -1;
                return 0;
            } else if (b.url) // a > b
                return 1;
            return -1;
        });

        this._revisionList = revisionList;
    }

    static htmlTemplate()
    {
        return `
            <div class="chart-pane-status">
                <h3 class="chart-status-current-value"></h3>
                <span class="chart-status-comparison"></span>
            </div>
            <table class="chart-pane-revisions"></table>
        `;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + ChartStatusView.cssTemplate() + `
            .chart-pane-status {
                display: block;
                text-align: center;
            }

            .chart-pane-status .chart-status-current-value,
            .chart-pane-status .chart-status-comparison {
                display: block;
                margin: 0;
                padding: 0;
                font-weight: normal;
                font-size: 1rem;
            }

            .chart-pane-revisions {
                line-height: 1rem;
                font-size: 0.9rem;
                font-weight: normal;
                padding: 0;
                margin: 0;
                margin-top: 0.5rem;
                border-bottom: solid 1px #ccc;
                border-collapse: collapse;
                width: 100%;
            }

            .chart-pane-revisions th,
            .chart-pane-revisions td {
                font-weight: inherit;
                border-top: solid 1px #ccc;
                padding: 0.2rem 0.2rem;
            }
            
            .chart-pane-revisions .selected > th,
            .chart-pane-revisions .selected > td {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-revisions .commit-viewer-opener {
                width: 1rem;
            }

            .chart-pane-revisions .commit-viewer-opener a {
                text-decoration: none;
                color: inherit;
                font-weight: inherit;
            }
        `;
    }
}
