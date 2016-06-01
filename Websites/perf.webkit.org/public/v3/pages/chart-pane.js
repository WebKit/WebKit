
class ChartPane extends ChartPaneBase {
    constructor(chartsPage, platformId, metricId)
    {
        super('chart-pane');

        this._mainChartIndicatorWasLocked = false;
        this._chartsPage = chartsPage;
        this._lockedPopover = null;

        this.content().querySelector('close-button').component().setCallback(chartsPage.closePane.bind(chartsPage, this));

        this.configure(platformId, metricId);
    }

    serializeState()
    {
        var state = [this._platformId, this._metricId];
        if (this._mainChart) {
            var selection = this._mainChart.currentSelection();
            var currentPoint = this._mainChart.currentPoint();
            if (selection)
                state[2] = selection;
            else if (this._mainChartIndicatorWasLocked && currentPoint)
                state[2] = currentPoint.id;
        }

        var graphOptions = new Set;
        if (!this.isSamplingEnabled())
            graphOptions.add('noSampling');
        if (this.isShowingOutliers())
            graphOptions.add('showOutliers');

        if (graphOptions.size)
            state[3] = graphOptions;

        return state;
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

        // FIXME: This forces sourceList to be set twice. First in configure inside the constructor then here.
        var graphOptions = state[3];
        if (graphOptions instanceof Set) {
            this.setSamplingEnabled(!graphOptions.has('nosampling'));
            this.setShowOutliers(graphOptions.has('showoutliers'));
        }

        // FIXME: Show full y-axis when graphOptions is true to be compatible with v2 UI.
        // FIXME: state[4] specifies moving average in v2 UI
        // FIXME: state[5] specifies envelope in v2 UI
        // FIXME: state[6] specifies change detection algorithm in v2 UI
    }

    setOverviewSelection(selection)
    {
        if (this._overviewChart)
            this._overviewChart.setSelection(selection);
    }

    _overviewSelectionDidChange(domain, didEndDrag)
    {
        super._overviewSelectionDidChange(domain, didEndDrag);
        this._chartsPage.setMainDomainFromOverviewSelection(domain, this, didEndDrag);
    }

    _mainSelectionDidChange(selection, didEndDrag)
    {
        super._mainSelectionDidChange(selection, didEndDrag);
        this._chartsPage.mainChartSelectionDidChange(this, didEndDrag);
    }

    _mainSelectionDidZoom(selection)
    {
        super._mainSelectionDidZoom(selection);
        this._chartsPage.setMainDomainFromZoom(selection, this);
    }

    router() { return this._chartsPage.router(); }

    _requestOpeningCommitViewer(repository, from, to)
    {
        super._requestOpeningCommitViewer(repository, from, to);
        this._chartsPage.setOpenRepository(repository);
    }

    setOpenRepository(repository)
    {
        if (repository != this._commitLogViewer.currentRepository()) {
            var range = this._mainChartStatus.setCurrentRepository(repository);
            this._commitLogViewer.view(repository, range.from, range.to).then(this.render.bind(this));
            this.render();
        }
    }

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._chartsPage.mainChartIndicatorDidChange(this, isLocked != this._mainChartIndicatorWasLocked);
        this._mainChartIndicatorWasLocked = isLocked;
        super._indicatorDidChange(indicatorID, isLocked);
    }

    _analyzeRange(pointsRangeForAnalysis)
    {
        var router = this._chartsPage.router();
        var newWindow = window.open(router.url('analysis/task/create'), '_blank');

        var analyzePopover = this.content().querySelector('.chart-pane-analyze-popover');
        var name = analyzePopover.querySelector('input').value;
        var self = this;
        AnalysisTask.create(name, pointsRangeForAnalysis.startPointId, pointsRangeForAnalysis.endPointId).then(function (data) {
            newWindow.location.href = router.url('analysis/task/' + data['taskId']);
            self.fetchAnalysisTasks(true);
            // FIXME: Refetch the list of analysis tasks.
        }, function (error) {
            newWindow.location.href = router.url('analysis/task/create', {error: error});
        });
    }

    _markAsOutlier(markAsOutlier, points)
    {
        var self = this;
        return Promise.all(points.map(function (point) {
            return PrivilegedAPI.sendRequest('update-run-status', {'run': point.id, 'markedOutlier': markAsOutlier});
        })).then(function () {
            self._mainChart.fetchMeasurementSets(true /* noCache */);
        }, function (error) {
            alert('Failed to update the outlier status: ' + error);
        }).catch();
    }

    render()
    {
        if (this._platform && this._metric) {
            var metric = this._metric;
            var platform = this._platform;

            this.renderReplace(this.content().querySelector('.chart-pane-title'),
                metric.fullName() + ' on ' + platform.name());
        }

        if (this._mainChartStatus)
            this._renderActionToolbar();

        super.render();
    }

    _renderActionToolbar()
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

        var platformPopover = this.content().querySelector('.chart-pane-alternative-platforms');
        var alternativePlatforms = this._chartsPage.alternatePlatforms(platform, metric);
        if (alternativePlatforms.length) {
            this.renderReplace(platformPopover, Platform.sortByName(alternativePlatforms).map(function (platform) {
                return element('li', link(platform.label(), function () {
                    self._chartsPage.insertPaneAfter(platform, metric, self);
                }));
            }));

            actions.push(this._makePopoverActionItem(platformPopover, 'Other Platforms', true));
        } else
            platformPopover.style.display = 'none';

        var analyzePopover = this.content().querySelector('.chart-pane-analyze-popover');
        var pointsRangeForAnalysis = this._mainChartStatus.pointsRangeForAnalysis();
        if (pointsRangeForAnalysis) {
            actions.push(this._makePopoverActionItem(analyzePopover, 'Analyze', false));
            analyzePopover.onsubmit = function (event) {
                event.preventDefault();
                self._analyzeRange(pointsRangeForAnalysis);
            }
        } else {
            analyzePopover.style.display = 'none';
            analyzePopover.onsubmit = function (event) { event.preventDefault(); }
        }

        var filteringOptions = this.content().querySelector('.chart-pane-filtering-options');
        actions.push(this._makePopoverActionItem(filteringOptions, 'Filtering', true));

        this._renderFilteringPopover();

        this._lockedPopover = null;
        this.renderReplace(this.content().querySelector('.chart-pane-action-buttons'), actions);
    }

    _makePopoverActionItem(popover, label, shouldRespondToHover)
    {
        var self = this;
        popover.anchor = ComponentBase.createLink(label, function () {
            var makeVisible = self._lockedPopover != popover;
            self._setPopoverVisibility(popover, makeVisible);
            if (makeVisible)
                self._lockedPopover = popover;
        });
        if (shouldRespondToHover)
            this._makePopoverOpenOnHover(popover);

        return ComponentBase.createElement('li', {class: this._lockedPopover == popover ? 'selected' : ''}, popover.anchor);
    }

    _makePopoverOpenOnHover(popover)
    {
        var mouseIsInAnchor = false;
        var mouseIsInPopover = false;

        var self = this;
        var closeIfNeeded = function () {
            setTimeout(function () {
                if (self._lockedPopover != popover && !mouseIsInAnchor && !mouseIsInPopover)
                    self._setPopoverVisibility(popover, false);
            }, 0);
        }

        popover.anchor.onmouseenter = function () {
            if (self._lockedPopover)
                return;
            mouseIsInAnchor = true;
            self._setPopoverVisibility(popover, true);
        }
        popover.anchor.onmouseleave = function () {
            mouseIsInAnchor = false;
            closeIfNeeded();         
        }

        popover.onmouseenter = function () {
            mouseIsInPopover = true;
        }
        popover.onmouseleave = function () {
            mouseIsInPopover = false;
            closeIfNeeded();
        }
    }

    _setPopoverVisibility(popover, visible)
    {
        var anchor = popover.anchor;
        if (visible) {
            var width = anchor.offsetParent.offsetWidth;
            popover.style.top = anchor.offsetTop + anchor.offsetHeight + 'px';
            popover.style.right = (width - anchor.offsetLeft - anchor.offsetWidth) + 'px';
        }
        popover.style.display = visible ? null : 'none';
        anchor.parentNode.className = visible ? 'selected' : '';

        if (this._lockedPopover && this._lockedPopover != popover && visible)
            this._setPopoverVisibility(this._lockedPopover, false);

        if (this._lockedPopover == popover && !visible)
            this._lockedPopover = null;
    }

    _renderFilteringPopover()
    {
        var enableSampling = this.content().querySelector('.enable-sampling');
        enableSampling.checked = this.isSamplingEnabled();
        enableSampling.onchange = function () {
            self.setSamplingEnabled(enableSampling.checked);
            self._chartsPage.graphOptionsDidChange();
        }

        var showOutliers = this.content().querySelector('.show-outliers');
        showOutliers.checked = this.isShowingOutliers();
        showOutliers.onchange = function () {
            self.setShowOutliers(showOutliers.checked);
            self._chartsPage.graphOptionsDidChange();
        }

        var markAsOutlierButton = this.content().querySelector('.mark-as-outlier');
        var firstSelectedPoint = this._mainChart.lockedIndicator();
        if (!firstSelectedPoint)
            firstSelectedPoint = this._mainChart.firstSelectedPoint('current');
        var alreayMarkedAsOutlier = firstSelectedPoint && firstSelectedPoint.markedOutlier;

        var self = this;
        markAsOutlierButton.textContent = (alreayMarkedAsOutlier ? 'Unmark' : 'Mark') + ' selected points as outlier';
        markAsOutlierButton.onclick = function () {
            var selectedPoints = [firstSelectedPoint];
            if (self._mainChart.currentSelection('current'))
                selectedPoints = self._mainChart.selectedPoints('current');
            self._markAsOutlier(!alreayMarkedAsOutlier, selectedPoints);
        }
        markAsOutlierButton.disabled = !firstSelectedPoint;
    }

    static paneHeaderTemplate()
    {
        return `
            <header class="chart-pane-header">
                <h2 class="chart-pane-title">-</h2>
                <nav class="chart-pane-actions">
                    <ul>
                        <li class="close"><close-button></close-button></li>
                    </ul>
                    <ul class="chart-pane-action-buttons buttoned-toolbar"></ul>
                    <ul class="chart-pane-alternative-platforms popover" style="display:none"></ul>
                    <form class="chart-pane-analyze-popover popover" style="display:none">
                        <input type="text" required>
                        <button>Create</button>
                    </form>
                    <ul class="chart-pane-filtering-options popover" style="display:none">
                        <li><label><input type="checkbox" class="enable-sampling">Sampling</label></li>
                        <li><label><input type="checkbox" class="show-outliers">Show outliers</label></li>
                        <li><button class="mark-as-outlier">Mark selected points as outlier</button></li>
                    </ul>
                </nav>
            </header>
        `;
    }

    static cssTemplate()
    {
        return ChartPaneBase.cssTemplate() + `
            .chart-pane {
                border: solid 1px #ccc;
                border-radius: 0.5rem;
                margin: 1rem;
                margin-bottom: 2rem;
            }

            .chart-pane-body {
                height: calc(100% - 2rem);
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

            .chart-pane-actions .popover {
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

            .chart-pane-actions .popover li {
            }

            .chart-pane-actions .popover li a {
                display: block;
                text-decoration: none;
                color: inherit;
                font-size: 0.9rem;
                padding: 0.2rem 0.5rem;
            }

            .chart-pane-actions .popover a:hover,
            .chart-pane-actions .popover input:focus {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-actions .chart-pane-analyze-popover {
                padding: 0.5rem;
            }

            .chart-pane-actions .popover label {
                font-size: 0.9rem;
            }

            .chart-pane-actions .popover input[type=text] {
                font-size: 1rem;
                width: 15rem;
                outline: none;
                border: solid 1px #ccc;
            }
`;
    }
}
