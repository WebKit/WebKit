
class ChartPane extends ChartPaneBase {
    constructor(chartsPage, platformId, metricId)
    {
        super('chart-pane');

        this._mainChartIndicatorWasLocked = false;
        this._chartsPage = chartsPage;
        this._paneOpenedByClick = null;

        this.content().querySelector('close-button').component().setCallback(chartsPage.closePane.bind(chartsPage, this));

        this.configure(platformId, metricId);
    }

    serializeState()
    {
        var selection = this._mainChart ? this._mainChart.currentSelection() : null;
        var point = this._mainChart ? this._mainChart.currentPoint() : null;
        return [
            this._platformId,
            this._metricId,
            selection || (point && this._mainChartIndicatorWasLocked ? point.id : null),
        ];
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

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._mainChartIndicatorWasLocked = isLocked;
        this._chartsPage.mainChartIndicatorDidChange(this, isLocked || this._mainChartIndicatorWasLocked);
        super._indicatorDidChange(indicatorID, isLocked);
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

        var platformPane = this.content().querySelector('.chart-pane-alternative-platforms');
        var alternativePlatforms = this._chartsPage.alternatePlatforms(platform, metric);
        if (alternativePlatforms.length) {
            this.renderReplace(platformPane, Platform.sortByName(alternativePlatforms).map(function (platform) {
                return element('li', link(platform.label(), function () {
                    self._chartsPage.insertPaneAfter(platform, metric, self);
                }));
            }));

            actions.push(element('li', {class: this._paneOpenedByClick == platformPane ? 'selected' : ''},
                this._makeAnchorToOpenPane(platformPane, 'Other Platforms', true)));
        } else {
            platformPane.style.display = 'none';
        }

        var analyzePane = this.content().querySelector('.chart-pane-analyze-pane');
        var pointsRangeForAnalysis = this._mainChartStatus.pointsRangeForAnalysis();
        if (pointsRangeForAnalysis) {
            actions.push(element('li', {class: this._paneOpenedByClick == analyzePane ? 'selected' : ''},
                this._makeAnchorToOpenPane(analyzePane, 'Analyze', false)));

            var router = this._chartsPage.router();
            analyzePane.onsubmit = function (event) {
                event.preventDefault();
                var newWindow = window.open(router.url('analysis/task/create'), '_blank');

                var name = analyzePane.querySelector('input').value;
                AnalysisTask.create(name, pointsRangeForAnalysis.startPointId, pointsRangeForAnalysis.endPointId).then(function (data) {
                    newWindow.location.href = router.url('analysis/task/' + data['taskId']);
                    // FIXME: Refetch the list of analysis tasks.
                }, function (error) {
                    newWindow.location.href = router.url('analysis/task/create', {error: error});
                });
            }
        } else {
            analyzePane.style.display = 'none';
            analyzePane.onsubmit = function (event) { event.preventDefault(); }
        }

        this._paneOpenedByClick = null;
        this.renderReplace(this.content().querySelector('.chart-pane-action-buttons'), actions);
    }

    _makeAnchorToOpenPane(pane, label, shouldRespondToHover)
    {
        var anchor = null;
        var ignoreMouseLeave = false;
        var self = this;
        var setPaneVisibility = function (pane, shouldShow) {
            var anchor = pane.anchor;
            if (shouldShow) {
                var width = anchor.offsetParent.offsetWidth;
                pane.style.top = anchor.offsetTop + anchor.offsetHeight + 'px';
                pane.style.right = (width - anchor.offsetLeft - anchor.offsetWidth) + 'px';
            }
            pane.style.display = shouldShow ? null : 'none';
            anchor.parentNode.className = shouldShow ? 'selected' : '';
            if (self._paneOpenedByClick == pane && !shouldShow)
                self._paneOpenedByClick = null;
        }

        var attributes = {
            href: '#',
            onclick: function (event) {
                event.preventDefault();
                var shouldShowPane = pane.style.display == 'none';
                if (shouldShowPane) {
                    if (self._paneOpenedByClick)
                        setPaneVisibility(self._paneOpenedByClick, false);
                    self._paneOpenedByClick = pane;
                }
                setPaneVisibility(pane, shouldShowPane);
            },
        };
        if (shouldRespondToHover) {
            var mouseIsInAnchor = false;
            var mouseIsInPane = false;

            attributes.onmouseenter = function () {
                if (self._paneOpenedByClick)
                    return;
                mouseIsInAnchor = true;
                setPaneVisibility(pane, true);
            }
            attributes.onmouseleave = function () {
                setTimeout(function () {
                    if (!mouseIsInPane)
                        setPaneVisibility(pane, false);
                }, 0);
                mouseIsInAnchor = false;                
            }

            pane.onmouseleave = function () {
                setTimeout(function () {
                    if (!mouseIsInAnchor)
                        setPaneVisibility(pane, false);
                }, 0);
                mouseIsInPane = false;
            }
            pane.onmouseenter = function () {
                mouseIsInPane = true;
            }
        }

        var anchor = ComponentBase.createElement('a', attributes, label);
        pane.anchor = anchor;
        return anchor;
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
                    <ul class="chart-pane-alternative-platforms" style="display:none"></ul>
                    <form class="chart-pane-analyze-pane" style="display:none">
                        <input type="text" required>
                        <button>Create</button>
                    </form>
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

            .chart-pane-actions .chart-pane-alternative-platforms,
            .chart-pane-analyze-pane {
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

            .chart-pane-alternative-platforms li {
            }

            .chart-pane-alternative-platforms li a {
                display: block;
                text-decoration: none;
                color: inherit;
                font-size: 0.9rem;
                padding: 0.2rem 0.5rem;
            }

            .chart-pane-alternative-platforms a:hover,
            .chart-pane-analyze-pane input:focus {
                background: rgba(204, 153, 51, 0.1);
            }

            .chart-pane-analyze-pane {
                padding: 0.5rem;
            }

            .chart-pane-analyze-pane input {
                font-size: 1rem;
                width: 15rem;
                outline: none;
                border: solid 1px #ccc;
            }
`;
    }
}
