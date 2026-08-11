
function createTrendLineExecutableFromAveragingFunction(callback) {
    return function (source, parameters) {
        var timeSeries = source.measurementSet.fetchedTimeSeries(source.type, source.includeOutliers, source.extendToFuture);
        var values = timeSeries.values();
        if (!values.length)
            return Promise.resolve(null);

        var averageValues = callback.call(null, values, ...parameters);
        if (!averageValues)
            return Promise.resolve(null);

        var interval = function () { return null; }
        var result = new Array(averageValues.length);
        result.firstPoint = () => result[0];
        result.nextPoint = (point) => result[point.seriesIndex + 1];
        for (var i = 0; i < averageValues.length; i++)
            result[i] = {seriesIndex: i, time: timeSeries.findPointByIndex(i).time, value: averageValues[i], interval: interval};

        return Promise.resolve(result);
    }
}

const ChartTrendLineTypes = [
    {
        id: 0,
        label: 'None',
    },
    {
        id: 5,
        label: 'Segmentation',
        execute: function (source, parameters) {
            return source.measurementSet.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', parameters,
                source.type, source.includeOutliers, source.extendToFuture).then(function (segmentation) {
                if (!segmentation)
                    return segmentation;
                segmentation.forEach((point, index) => point.seriesIndex = index);
                segmentation.firstPoint = () => segmentation[0];
                segmentation.nextPoint = (point) => segmentation[point.seriesIndex + 1];
                return segmentation;
            });
        },
        parameterList: [
            {label: "Segment count weight", value: 2.5, min: 0.01, max: 10, step: 0.01},
            {label: "Grid size", value: 500, min: 100, max: 10000, step: 10}
        ]
    },
    {
        id: 6,
        label: 'Segmentation with Welch\'s t-test change detection',
        execute: async function (source, parameters) {
            const segmentation =  await source.measurementSet.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', parameters,
                source.type, source.includeOutliers, source.extendToFuture);
            if (!segmentation)
                return segmentation;

            const metric = Metric.findById(source.measurementSet.metricId());
            const timeSeries = source.measurementSet.fetchedTimeSeries(source.type, source.includeOutliers, source.extendToFuture);
            segmentation.analysisAnnotations = Statistics.findRangesForChangeDetectionsWithWelchsTTest(timeSeries.values(),
                segmentation, parameters[parameters.length - 1]).map((range) => {
                const startPoint = timeSeries.findPointByIndex(range.startIndex);
                const endPoint = timeSeries.findPointByIndex(range.endIndex);
                const summary = metric.labelForDifference(range.segmentationStartValue, range.segmentationEndValue, 'progression', 'regression');
                return {
                    task: null,
                    fillStyle: ChartStyles.annotationFillStyleForTask(null),
                    startTime: startPoint.time,
                    endTime: endPoint.time,
                    label: `Potential ${summary.changeLabel}`,
                };
            });
            segmentation.firstPoint = () => segmentation[0];
            segmentation.nextPoint = (point) => segmentation[point.seriesIndex + 1];
            return segmentation;
        },
        parameterList: [
            {label: "Segment count weight", value: 2.5, min: 0.01, max: 10, step: 0.01},
            {label: "Grid size", value: 500, min: 100, max: 10000, step: 10},
            {label: "t-test significance", value: 0.99, options: Statistics.supportedOneSideTTestProbabilities()},
        ]
    },
    {
        id: 1,
        label: 'Simple Moving Average',
        parameterList: [
            {label: "Backward window size", value: 8, min: 2, step: 1},
            {label: "Forward window size", value: 4, min: 0, step: 1}
        ],
        execute: createTrendLineExecutableFromAveragingFunction(Statistics.movingAverage.bind(Statistics))
    },
    {
        id: 2,
        label: 'Cumulative Moving Average',
        execute: createTrendLineExecutableFromAveragingFunction(Statistics.cumulativeMovingAverage.bind(Statistics))
    },
    {
        id: 3,
        label: 'Exponential Moving Average',
        parameterList: [
            {label: "Smoothing factor", value: 0.01, min: 0.001, max: 0.9, step: 0.001},
        ],
        execute: createTrendLineExecutableFromAveragingFunction(Statistics.exponentialMovingAverage.bind(Statistics))
    },
];
ChartTrendLineTypes.DefaultType = ChartTrendLineTypes[1];


class ChartPane extends ChartPaneBase {
    constructor(chartsPage, platformId, metricId)
    {
        super('chart-pane');

        this._mainChartIndicatorWasLocked = false;
        this._chartsPage = chartsPage;
        this._lockedPopover = null;
        this._trendLineType = null;
        this._trendLineParameters = [];
        this._trendLineVersion = 0;
        this._renderedTrendLineOptions = false;

        this.configure(platformId, metricId);
    }

    didConstructShadowTree()
    {
        this.part('close').listenToAction('activate', () => {
            this._chartsPage.closePane(this);
        });
        const createWithTestGroupCheckbox = this.content('create-with-test-group');
        const repetitionCount = this.content('confirm-repetition');
        const notifyOnCompletion = this.content('notify-on-completion');
        const repetitionTypeSelection = this.part('repetition-type-selector');
        createWithTestGroupCheckbox.onchange = () => {
            // FIXME: should invoke "enqueueToRender" instead.
            const shouldDisable = !createWithTestGroupCheckbox.checked;
            repetitionCount.disabled = shouldDisable;
            notifyOnCompletion.disabled = shouldDisable;
            repetitionTypeSelection.disabled = shouldDisable;
        };
    }

    serializeState()
    {
        var state = [this._platformId, this._metricId];
        if (this._mainChart) {
            var selection = this._mainChart.currentSelection();
            const indicator = this._mainChart.currentIndicator();
            if (selection)
                state[2] = selection;
            else if (indicator && indicator.isLocked)
                state[2] = indicator.point.id;
        }

        var graphOptions = new Set;
        if (!this.isSamplingEnabled())
            graphOptions.add('noSampling');
        if (this.isShowingOutliers())
            graphOptions.add('showOutliers');

        if (graphOptions.size)
            state[3] = graphOptions;

        if (this._trendLineType)
            state[4] = [this._trendLineType.id].concat(this._trendLineParameters);

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
        // FIXME: Show full y-axis when graphOptions is true to be compatible with v2 UI.
        var graphOptions = state[3];
        if (graphOptions instanceof Set) {
            this.setSamplingEnabled(!graphOptions.has('nosampling'));
            this.setShowOutliers(graphOptions.has('showoutliers'));
        }

        var trendLineOptions = state[4];
        if (!(trendLineOptions instanceof Array))
            trendLineOptions = [];

        var trendLineId = trendLineOptions[0];
        var trendLineType = ChartTrendLineTypes.find(function (type) { return type.id == trendLineId; }) || ChartTrendLineTypes.DefaultType;

        this._trendLineType = trendLineType;
        this._trendLineParameters = (trendLineType.parameterList || []).map(function (parameter, index) {
            var specifiedValue = parseFloat(trendLineOptions[index + 1]);
            return !isNaN(specifiedValue) ? specifiedValue : parameter.value;
        });
        this._updateTrendLine();
        this._renderedTrendLineOptions = false;

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

    openNewRepository(repository)
    {
        this.content().querySelector('.chart-pane').focus();
        this._chartsPage.setOpenRepository(repository);
    }

    _indicatorDidChange(indicatorID, isLocked)
    {
        this._chartsPage.mainChartIndicatorDidChange(this, isLocked != this._mainChartIndicatorWasLocked);
        this._mainChartIndicatorWasLocked = isLocked;
        super._indicatorDidChange(indicatorID, isLocked);
    }

    async _analyzeRange(startPoint, endPoint)
    {
        const router = this._chartsPage.router();
        const newWindow = window.open(router.url('analysis/task/create', {inProgress: true}), '_blank');

        const name = this.content('task-name').value;
        const createWithTestGroup = this.content('create-with-test-group').checked;
        const repetitionCount = this.content('confirm-repetition').value;
        const notifyOnCompletion = this.content('notify-on-completion').checked;
        const repetitionType = this.part('repetition-type-selector').selectedRepetitionType;

        try {
            const analysisTask = await (createWithTestGroup ?
                AnalysisTask.create(name, startPoint, endPoint, 'Confirm', repetitionCount, repetitionType, notifyOnCompletion) : AnalysisTask.create(name, startPoint, endPoint));
            newWindow.location.href = router.url('analysis/task/' + analysisTask.id());
            this.fetchAnalysisTasks(true);
        } catch(error) {
            newWindow.location.href = router.url('analysis/task/create', {error: error});
        }
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
                metric.fullName() + ' on ' + platform.label());
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
        const selectedPoints = this._mainChart.selectedPoints('current');
        const hasSelectedPoints = selectedPoints && selectedPoints.length();
        if (hasSelectedPoints) {
            this.part('repetition-type-selector').setTestAndPlatform(metric.test(), platform);
            actions.push(this._makePopoverActionItem(analyzePopover, 'Analyze', false));
            analyzePopover.onsubmit = this.createEventHandler(() => {
                this._analyzeRange(selectedPoints.firstPoint(), selectedPoints.lastPoint());
            });
        } else {
            analyzePopover.style.display = 'none';
            analyzePopover.onsubmit = this.createEventHandler(() => {});
        }

        var filteringOptions = this.content().querySelector('.chart-pane-filtering-options');
        actions.push(this._makePopoverActionItem(filteringOptions, 'Filtering', true));

        var trendLineOptions = this.content().querySelector('.chart-pane-trend-line-options');
        actions.push(this._makePopoverActionItem(trendLineOptions, 'Trend lines', true));

        this._renderFilteringPopover();
        this._renderTrendLinePopover();

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
        const indicator = this._mainChart.currentIndicator();
        let firstSelectedPoint = indicator && indicator.isLocked ? indicator.point : null;
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

    _renderTrendLinePopover()
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var self = this;

        const trendLineTypesContainer = this.content().querySelector('.trend-line-types');
        if (!trendLineTypesContainer.querySelector('select')) {
            this.renderReplace(trendLineTypesContainer, [
                element('select', {onchange: this._trendLineTypeDidChange.bind(this)},
                    ChartTrendLineTypes.map((type) => { return element('option', {value: type.id}, type.label); }))
            ]);
        }
        if (this._trendLineType)
            trendLineTypesContainer.querySelector('select').value = this._trendLineType.id;

        if (this._renderedTrendLineOptions)
            return;
        this._renderedTrendLineOptions = true;

        if (this._trendLineParameters.length) {
            var configuredParameters = this._trendLineParameters;
            this.renderReplace(this.content().querySelector('.trend-line-parameter-list'), [
                element('h3', 'Parameters'),
                element('ul', this._trendLineType.parameterList.map(function (parameter, index) {
                    if (parameter.options) {
                        const select = element('select', parameter.options.map((option) =>
                            element('option', {value: option, selected: option == parameter.value}, option)));
                        select.onchange = self._trendLineParameterDidChange.bind(self);
                        select.parameterIndex = index;
                        return element('li', element('label', [parameter.label + ': ', select]));
                    }

                    var attributes = {type: 'number'};
                    for (var name in parameter)
                        attributes[name] = parameter[name];

                    attributes.value = configuredParameters[index];
                    const input = element('input', attributes);
                    input.parameterIndex = index;
                    input.oninput = self._trendLineParameterDidChange.bind(self);
                    input.onchange = self._trendLineParameterDidChange.bind(self);
                    return element('li', element('label', [parameter.label + ': ', input]));
                }))
            ]);
        } else
            this.renderReplace(this.content().querySelector('.trend-line-parameter-list'), []);
    }

    _trendLineTypeDidChange(event)
    {
        var newType = ChartTrendLineTypes.find(function (type) { return type.id == event.target.value });
        if (newType == this._trendLineType)
            return;

        this._trendLineType = newType;
        this._trendLineParameters = this._defaultParametersForTrendLine(newType);
        this._renderedTrendLineOptions = false;

        this._updateTrendLine();
        this._chartsPage.graphOptionsDidChange();
        this.enqueueToRender();
    }

    _defaultParametersForTrendLine(type)
    {
        return type && type.parameterList ? type.parameterList.map(function (parameter) { return parameter.value; }) : [];
    }

    _trendLineParameterDidChange(event)
    {
        var input = event.target;
        var index = input.parameterIndex;
        var newValue = parseFloat(input.value);
        if (this._trendLineParameters[index] == newValue)
            return;
        this._trendLineParameters[index] = newValue;
        var self = this;
        setTimeout(function () { // Some trend lines, e.g. sementations, are expensive.
            if (self._trendLineParameters[index] != newValue)
                return;
            self._updateTrendLine();
            self._chartsPage.graphOptionsDidChange();
        }, 500);
    }

    _didFetchData()
    {
        super._didFetchData();
        this._updateTrendLine();
    }

    async _updateTrendLine()
    {
        if (!this._mainChart.sourceList())
            return;

        this._trendLineVersion++;
        var currentTrendLineType = this._trendLineType || ChartTrendLineTypes.DefaultType;
        var currentTrendLineParameters = this._trendLineParameters || this._defaultParametersForTrendLine(currentTrendLineType);
        var currentTrendLineVersion = this._trendLineVersion;
        var sourceList = this._mainChart.sourceList();

        if (!currentTrendLineType.execute) {
            this._mainChart.clearTrendLines();
            this.enqueueToRender();
        } else {
            // Wait for all trendlines to be ready. Otherwise we might see FOC when the domain is expanded.
            await Promise.all(sourceList.map(async (source, sourceIndex) => {
                const trendlineSeries = await currentTrendLineType.execute.call(null, source, currentTrendLineParameters);
                if (this._trendLineVersion == currentTrendLineVersion)
                    this._mainChart.setTrendLine(sourceIndex, trendlineSeries);

                if (trendlineSeries && trendlineSeries.analysisAnnotations)
                    this._detectedAnnotations = trendlineSeries.analysisAnnotations;
                else
                    this._detectedAnnotations = null;
            }));
            this.enqueueToRender();
        }
    }

    static paneHeaderTemplate()
    {
        return `
            <header class="chart-pane-header">
                <h2 class="chart-pane-title">-</h2>
                <nav class="chart-pane-actions">
                    <ul>
                        <li><close-button id="close"></close-button></li>
                    </ul>
                    <ul class="chart-pane-action-buttons buttoned-toolbar"></ul>
                    <ul class="chart-pane-alternative-platforms popover" style="display:none"></ul>
                    <form class="chart-pane-analyze-popover popover" style="display:none">
                        <input type="text" id="task-name" required>
                        <button>Create</button>
                        <li>
                            <label><input type="checkbox" id="create-with-test-group" checked></label>
                            <label>Confirm with</label>
                                <select id="confirm-repetition">
                                    <option>1</option>
                                    <option>2</option>
                                    <option>3</option>
                                    <option selected>4</option>
                                    <option>5</option>
                                    <option>6</option>
                                    <option>7</option>
                                    <option>8</option>
                                    <option>9</option>
                                    <option>10</option>
                                </select>
                            <label>iterations</label>
                        </li>
                        <li>
                            <label>In</label>
                            <repetition-type-selection id="repetition-type-selector"></repetition-type-selection>
                        </li>
                        <li>
                            <label><input type="checkbox" id="notify-on-completion" checked>Notify on completion</label>
                        </li>
                    </form>
                    <ul class="chart-pane-filtering-options popover" style="display:none">
                        <li><label><input type="checkbox" class="enable-sampling">Sampling</label></li>
                        <li><label><input type="checkbox" class="show-outliers">Show outliers</label></li>
                        <li><button class="mark-as-outlier">Mark selected points as outlier</button></li>
                    </ul>
                    <ul class="chart-pane-trend-line-options popover" style="display:none">
                        <div class="trend-line-types"></div>
                        <div class="trend-line-parameter-list"></div>
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

            .chart-pane-actions ul, form {
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
                padding: 0.2rem 0;
                margin: 0;
                margin-top: -0.2rem;
                margin-right: -0.2rem;
                background: rgba(255, 255, 255, 0.95);
            }

            @supports ( -webkit-backdrop-filter: blur(0.5rem) ) {
                .chart-pane-actions .popover {
                    background: rgba(255, 255, 255, 0.6);
                    -webkit-backdrop-filter: blur(0.5rem);
                }
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

            .chart-pane-actions .popover.chart-pane-filtering-options {
                padding: 0.2rem;
            }

            .chart-pane-actions .popover.chart-pane-trend-line-options h3 {
                font-size: 0.9rem;
                line-height: 0.9rem;
                font-weight: inherit;
                margin: 0;
                padding: 0.2rem;
                border-bottom: solid 1px #ccc;
            }

            .chart-pane-actions .popover.chart-pane-trend-line-options select,
            .chart-pane-actions .popover.chart-pane-trend-line-options label {
                margin: 0.2rem;
            }

            .chart-pane-actions .popover.chart-pane-trend-line-options label {
                font-size: 0.8rem;
            }

            .chart-pane-actions .popover.chart-pane-trend-line-options input {
                width: 2.5rem;
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
