
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

        this._usedRevisionRange = [null, null, null];
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
                    self._setRevisionRange(true, null, null, null);
                else
                    self._setRevisionRange(true, info.repository, info.from, info.to);
            };

            return element('tr', {class: selected ? 'selected' : ''}, [
                element('td', info.repository.name()),
                element('td', info.url ? link(info.label, info.label, info.url, true) : info.label),
                element('td', {class: 'commit-viewer-opener'}, link('\u00BB', action)),
            ]);
        });

        if (this._buildInfo) {
            var build = this._buildInfo;
            var number = build.buildNumber();
            var buildTime = this._formatTime(build.buildTime());
            var url = build.url();

            tableContent.unshift(element('tr', [
                element('td', 'Build'),
                element('td', {colspan: 2}, [url ? link(number, build.label(), url, true) : number, ` (${buildTime})`]),
            ]));
        }

        this.renderReplace(this.content().querySelector('.chart-pane-revisions'), tableContent);
    }

    _formatTime(date)
    {
        console.assert(date instanceof Date);
        return date.toISOString().replace('T', ' ').replace(/\.\d+Z$/, '');
    }

    setCurrentRepository(repository)
    {
        this._currentRepository = repository;
        return this._updateRevisionListForNewCurrentRepository();
    }

    _setRevisionRange(shouldNotify, repository, from, to)
    {
        if (this._usedRevisionRange[0] == repository
            && this._usedRevisionRange[1] == from && this._usedRevisionRange[2] == to)
            return;
        this._usedRevisionRange = [repository, from, to];
        if (shouldNotify)
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

        var item = this._revisionList[newIndex];
        this.setCurrentRepository(item ? item.repository : null);

        return true;
    }

    updateRevisionList()
    {
        if (!this._currentRepository)
            return {repository: null, from: null, to: null};
        return this._updateRevisionListForNewCurrentRepository();
    }

    _updateRevisionListForNewCurrentRepository()
    {
        this.updateStatusIfNeeded();

        this._forceRender = true;
        for (var info of this._revisionList) {
            if (info.repository == this._currentRepository) {
                this._setRevisionRange(false, info.repository, info.from, info.to);
                return {repository: info.repository, from: info.from, to: info.to};
            }
        }
        this._setRevisionRange(false, null, null, null);
        return {repository: this._currentRepository, from: null, to: null};
    }

    computeChartStatusLabels(currentPoint, previousPoint)
    {
        super.computeChartStatusLabels(currentPoint, previousPoint);

        this._buildInfo = null;
        this._revisionList = [];
        this._pointsRangeForAnalysis = null;

        if (!currentPoint)
            return;

        if (!this._chart.currentSelection())
            this._buildInfo = currentPoint.build();

        if (currentPoint && previousPoint && this._chart.currentSelection()) {
            this._pointsRangeForAnalysis = {
                startPointId: previousPoint.id,
                endPointId: currentPoint.id,
            };
        }

        // FIXME: Rewrite the interface to obtain the list of revision changes.
        var currentRootSet = currentPoint.rootSet();
        var previousRootSet = previousPoint ? previousPoint.rootSet() : null;

        var repositoriesInCurrentRootSet = Repository.sortByNamePreferringOnesWithURL(currentRootSet.repositories());
        var revisionList = [];
        for (var repository of repositoriesInCurrentRootSet) {
            var currentCommit = currentRootSet.commitForRepository(repository);
            var previousCommit = previousRootSet ? previousRootSet.commitForRepository(repository) : null;
            revisionList.push(currentCommit.diff(previousCommit));
        }

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
