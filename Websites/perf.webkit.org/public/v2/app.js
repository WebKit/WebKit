window.App = Ember.Application.create();

App.Router.map(function () {
    this.resource('customDashboard', {path: 'dashboard/custom'});
    this.resource('dashboard', {path: 'dashboard/:name'});
    this.resource('charts', {path: 'charts'});
    this.resource('analysis', {path: 'analysis'});
    this.resource('analysisTask', {path: 'analysis/task/:taskId'});
});

App.DashboardRow = Ember.Object.extend({
    header: null,
    cells: [],

    init: function ()
    {
        this._super();

        var cellsInfo = this.get('cellsInfo') || [];
        var columnCount = this.get('columnCount');
        while (cellsInfo.length < columnCount)
            cellsInfo.push([]);

        this.set('cells', cellsInfo.map(this._createPane.bind(this)));
    },
    addPane: function (paneInfo)
    {
        var pane = this._createPane(paneInfo);
        this.get('cells').pushObject(pane);
        this.set('columnCount', this.get('columnCount') + 1);
    },
    _createPane: function (paneInfo)
    {
        if (!paneInfo || !paneInfo.length || (!paneInfo[0] && !paneInfo[1]))
            paneInfo = null;

        var pane = App.Pane.create({
            store: this.get('store'),
            platformId: paneInfo ? paneInfo[0] : null,
            metricId: paneInfo ? paneInfo[1] : null,
            inDashboard: true
        });

        return App.DashboardPaneProxyForPicker.create({content: pane});
    },
});

App.DashboardPaneProxyForPicker = Ember.ObjectProxy.extend({
    _platformOrMetricIdChanged: function ()
    {
        var self = this;
        App.buildPopup(this.get('store'), 'choosePane', this)
            .then(function (platforms) { self.set('pickerData', platforms); });
    }.observes('platformId', 'metricId').on('init'),
    paneList: function () {
        return App.encodePrettifiedJSON([[this.get('platformId'), this.get('metricId'), null, null, null, null, null]]);
    }.property('platformId', 'metricId'),
});

App.IndexRoute = Ember.Route.extend({
    beforeModel: function ()
    {
        var self = this;
        App.Manifest.fetch(this.store).then(function () {
            self.transitionTo('dashboard', App.Manifest.defaultDashboardName() || '');
        });
    },
});

App.DashboardRoute = Ember.Route.extend({
    model: function (param)
    {
        return App.Manifest.fetch(this.store).then(function () {
            return App.Manifest.dashboardByName(param.name);
        });
    },
});

App.CustomDashboardRoute = Ember.Route.extend({
    controllerName: 'dashboard',
    model: function (param)
    {
        return this.store.createRecord('dashboard', {serialized: param.grid});
    },
    renderTemplate: function()
    {
        this.render('dashboard');
    }
});

App.DashboardController = Ember.Controller.extend({
    queryParams: ['grid', 'numberOfDays'],
    headerColumns: [],
    rows: [],
    numberOfDays: 7,
    editMode: false,

    modelChanged: function ()
    {
        var dashboard = this.get('model');
        if (!dashboard)
            return;

        var headerColumns = dashboard.get('headerColumns');
        this.set('headerColumns', headerColumns);
        var columnCount = headerColumns.length;
        this.set('columnCount', columnCount);

        var store = this.store;
        this.set('rows', dashboard.get('rows').map(function (rowParam) {
            return App.DashboardRow.create({
                store: store,
                header: rowParam[0],
                cellsInfo: rowParam.slice(1),
                columnCount: columnCount,
            })
        }));

        this.set('emptyRow', new Array(columnCount));
    }.observes('model').on('init'),

    computeGrid: function()
    {
        var headers = this.get('headerColumns').map(function (header) { return header.label; });
        var table = [headers].concat(this.get('rows').map(function (row) {
            return [row.get('header')].concat(row.get('cells').map(function (pane) {
                var platformAndMetric = [pane.get('platformId'), pane.get('metricId')];
                return platformAndMetric[0] || platformAndMetric[1] ? platformAndMetric : [];
            }));
        }));
        return JSON.stringify(table);
    },

    _sharedDomainChanged: function ()
    {
        var numberOfDays = this.get('numberOfDays');
        if (!numberOfDays)
            return;

        numberOfDays = parseInt(numberOfDays);
        var present = Date.now();
        var past = present - numberOfDays * 24 * 3600 * 1000;
        this.set('since', past);
        this.set('sharedDomain', [past, present]);
    }.observes('numberOfDays').on('init'),

    actions: {
        setNumberOfDays: function (numberOfDays)
        {
            this.set('numberOfDays', numberOfDays);
        },
        choosePane: function (param)
        {
            var pane = param.position;
            pane.set('platformId', param.platform.get('id'));
            pane.set('metricId', param.metric.get('id'));
        },
        addColumn: function ()
        {
            this.get('headerColumns').pushObject({
                label: this.get('newColumnHeader'),
                index: this.get('headerColumns').length,
            });
            this.get('rows').forEach(function (row) {
                row.addPane();
            });
            this.set('newColumnHeader', null);
        },
        removeColumn: function (index)
        {
            this.get('headerColumns').removeAt(index);
            this.get('rows').forEach(function (row) {
                row.get('cells').removeAt(index);
            });
        },
        addRow: function ()
        {
            this.get('rows').pushObject(App.DashboardRow.create({
                store: this.store,
                header: this.get('newRowHeader'),
                columnCount: this.get('columnCount'),
            }));
            this.set('newRowHeader', null);
        },
        removeRow: function (row)
        {
            this.get('rows').removeObject(row);
        },
        resetPane: function (pane)
        {
            pane.set('platformId', null);
            pane.set('metricId', null);
        },
        toggleEditMode: function ()
        {
            this.toggleProperty('editMode');
            if (this.get('editMode'))
                this.transitionToRoute('dashboard', 'custom', {name: null, queryParams: {grid: this.computeGrid()}});
            else
                this.set('grid', this.computeGrid());
        },
    },

    init: function ()
    {
        this._super();
        App.Manifest.fetch(this.get('store'));
    }
});

App.NumberOfDaysControlView = Ember.View.extend({
    classNames: ['controls'],
    templateName: 'number-of-days-controls',
    didInsertElement: function ()
    {
        this._matchingElements(this._previousNumberOfDaysClass).addClass('active');
    },
    _numberOfDaysChanged: function ()
    {
        this._matchingElements(this._previousNumberOfDaysClass).removeClass('active');

        var newNumberOfDaysClass = 'numberOfDaysIs' + this.get('numberOfDays');
        this._matchingElements(this._previousNumberOfDaysClass).addClass('active');
        this._previousNumberOfDaysClass = newNumberOfDaysClass;
    }.observes('numberOfDays').on('init'),
    _matchingElements: function (className)
    {
        var element = this.get('element');
        if (!element)
            return $([]);
        return $(element.getElementsByClassName(className));
    }
});

App.StartTimeSliderView = Ember.View.extend({
    templateName: 'start-time-slider',
    classNames: ['start-time-slider'],
    startTime: Date.now() - 7 * 24 * 3600 * 1000,
    oldestStartTime: null,
    _numberOfDaysView: null,
    _slider: null,
    _startTimeInSlider: null,
    _currentNumberOfDays: null,
    _MILLISECONDS_PER_DAY: 24 * 3600 * 1000,

    didInsertElement: function ()
    {
        this.oldestStartTime = Date.now() - 365 * 24 * 3600 * 1000;
        this._slider = $(this.get('element')).find('input');
        this._numberOfDaysView = $(this.get('element')).find('.numberOfDays');
        this._sliderRangeChanged();
        this._slider.change(this._sliderMoved.bind(this));
    },
    _sliderRangeChanged: function ()
    {
        var minimumNumberOfDays = 1;
        var maximumNumberOfDays = this._timeInPastToNumberOfDays(this.get('oldestStartTime'));
        var precision = 1000; // FIXME: Compute this from maximumNumberOfDays.
        var slider = this._slider;
        slider.attr('min', Math.floor(Math.log(Math.max(1, minimumNumberOfDays)) * precision) / precision);
        slider.attr('max', Math.ceil(Math.log(maximumNumberOfDays) * precision) / precision);
        slider.attr('step', 1 / precision);
        this._startTimeChanged();
    }.observes('oldestStartTime'),
    _sliderMoved: function ()
    {
        this._currentNumberOfDays = Math.round(Math.exp(this._slider.val()));
        this._numberOfDaysView.text(this._currentNumberOfDays);
        this._startTimeInSlider = this._numberOfDaysToTimeInPast(this._currentNumberOfDays);
        this.set('startTime', this._startTimeInSlider);
    },
    _startTimeChanged: function ()
    {
        var startTime = this.get('startTime');
        if (startTime == this._startTimeSetBySlider)
            return;
        this._currentNumberOfDays = this._timeInPastToNumberOfDays(startTime);

        if (this._slider) {
            this._numberOfDaysView.text(this._currentNumberOfDays);
            this._slider.val(Math.log(this._currentNumberOfDays));
            this._startTimeInSlider = startTime;
        }
    }.observes('startTime').on('init'),
    _timeInPastToNumberOfDays: function (timeInPast)
    {
        return Math.max(1, Math.round((Date.now() - timeInPast) / this._MILLISECONDS_PER_DAY));
    },
    _numberOfDaysToTimeInPast: function (numberOfDays)
    {
        return Date.now() - numberOfDays * this._MILLISECONDS_PER_DAY;
    },
});

App.Pane = Ember.Object.extend({
    platformId: null,
    platform: null,
    metricId: null,
    metric: null,
    selectedItem: null,
    selectedPoints: null,
    hoveredOrSelectedItem: null,
    showFullYAxis: false,
    inDashboard: false,
    searchCommit: function (repository, keyword) {
        var self = this;
        var repositoryId = repository.get('id');
        CommitLogs.fetchCommits(repositoryId, null, null, keyword).then(function (commits) {
            if (self.isDestroyed || !self.get('chartData') || !commits.length)
                return;
            var currentRuns = self.get('chartData').current.series();
            if (!currentRuns.length)
                return;

            var highlightedItems = {};
            var commitIndex = 0;
            for (var runIndex = 0; runIndex < currentRuns.length && commitIndex < commits.length; runIndex++) {
                var measurement = currentRuns[runIndex].measurement;
                var commitTime = measurement.commitTimeForRepository(repositoryId);
                if (!commitTime)
                    continue;
                if (commits[commitIndex].time <= commitTime) {
                    highlightedItems[measurement.id()] = true;
                    do {
                        commitIndex++;
                    } while (commitIndex < commits.length && commits[commitIndex].time <= commitTime);
                }
            }

            self.set('highlightedItems', highlightedItems);
        }, function () {
            // FIXME: Report errors
            this.set('highlightedItems', {});
        });
    },
    _fetch: function () {
        var platformId = this.get('platformId');
        var metricId = this.get('metricId');
        if (!platformId && !metricId) {
            this.set('empty', true);
            return;
        }
        this.set('empty', false);
        this.set('platform', null);
        this.set('chartData', null);
        this.set('metric', null);
        this.set('failure', null);

        if (!this._isValidId(platformId))
            this.set('failure', platformId ? 'Invalid platform id:' + platformId : 'Platform id was not specified');
        else if (!this._isValidId(metricId))
            this.set('failure', metricId ? 'Invalid metric id:' + metricId : 'Metric id was not specified');
        else {
            var self = this;
            var useCache = true;
            App.Manifest.fetchRunsWithPlatformAndMetric(this.get('store'), platformId, metricId, null, useCache)
                .then(function (result) {
                    if (!result || !result.data || result.shouldRefetch)
                        self.refetchRuns(platformId, metricId);
                    else
                        self._didFetchRuns(result);
                }, function () {
                    self.refetchRuns(platformId, metricId);
                });
            if (!this.get('inDashboard'))
                this.fetchAnalyticRanges();
        }
    }.observes('platformId', 'metricId').on('init'),
    refetchRuns: function (platformId, metricId) {
        if (!platformId)
            platformId = this.get('platform').get('id');
        if (!metricId)
            metricId = this.get('metric').get('id');
        Ember.assert('refetchRuns should be called only after platform and metric are resolved', platformId && metricId);

        var useCache = false;
        App.Manifest.fetchRunsWithPlatformAndMetric(this.get('store'), platformId, metricId, null, useCache)
            .then(this._didFetchRuns.bind(this), this._handleFetchErrors.bind(this, platformId, metricId));
    },
    _didFetchRuns: function (result)
    {
        this.set('platform', result.platform);
        this.set('metric', result.metric);
        this._setNewChartData(result.data);
    },
    _showOutlierChanged: function ()
    {
        var chartData = this.get('chartData');
        if (chartData)
            this._setNewChartData(chartData);
    }.observes('showOutlier'),
    _setNewChartData: function (chartData)
    {
        var newChartData = {};
        for (var property in chartData)
            newChartData[property] = chartData[property];

        var showOutlier = this.get('showOutlier');
        newChartData.showOutlier(showOutlier);
        this.set('chartData', newChartData);
        this._updateMovingAverageAndEnvelope();

        if (!this.get('anomalyDetectionStrategies').filterBy('enabled').length)
            this._highlightPointsMarkedAsOutlier(newChartData);
    },
    _highlightPointsMarkedAsOutlier: function (newChartData)
    {
        var data = newChartData.current.series();
        var items = {};
        for (var i = 0; i < data.length; i++) {
            if (data[i].measurement.markedOutlier())
                items[data[i].measurement.id()] = true;
        }

        this.set('highlightedItems', items);
    },
    _handleFetchErrors: function (platformId, metricId, result)
    {
        if (!result || typeof(result) === "string")
            this.set('failure', 'Failed to fetch the JSON with an error: ' + result);
        else if (!result.platform)
            this.set('failure', 'Could not find the platform "' + platformId + '"');
        else if (!result.metric)
            this.set('failure', 'Could not find the metric "' + metricId + '"');
        else
            this.set('failure', 'An internal error');
    },
    fetchAnalyticRanges: function ()
    {
        var platformId = this.get('platformId');
        var metricId = this.get('metricId');
        var self = this;
        this.get('store')
            .findAll('analysisTask') // FIXME: Fetch only analysis tasks relevant for this pane.
            .then(function (tasks) {
                self.set('analyticRanges', tasks.filter(function (task) {
                    return task.get('platform').get('id') == platformId
                        && task.get('metric').get('id') == metricId
                        && task.get('startRun') && task.get('endRun');
                }));
            });
    },
    ranges: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData || !chartData.unfilteredCurrentTimeSeries)
            return [];

        function midPoint(firstPoint, secondPoint) {
            if (firstPoint && secondPoint)
                return (+firstPoint.time + +secondPoint.time) / 2;
            if (firstPoint)
                return firstPoint.time;
            return secondPoint.time;
        }

        var timeSeries = chartData.unfilteredCurrentTimeSeries;
        var ranges = this.getWithDefault('analyticRanges', []);
        var testranges = this.getWithDefault('testRangeCandidates', []);
        return this.getWithDefault('analyticRanges', []).concat(this.getWithDefault('testRangeCandidates', [])).map(function (range) {
            var start = timeSeries.findPointByMeasurementId(range.get('startRun'));
            var end = timeSeries.findPointByMeasurementId(range.get('endRun'));

            return Ember.ObjectProxy.create({
                content: range,
                startTime: start ? midPoint(timeSeries.previousPoint(start), start) : null,
                endTime: end ? midPoint(end, timeSeries.nextPoint(end)) : null,
            });
        });
    }.property('chartData', 'analyticRanges', 'testRangeCandidates'),
    _isValidId: function (id)
    {
        if (typeof(id) == "number")
            return true;
        if (typeof(id) == "string")
            return !!id.match(/^[A-Za-z0-9_]+$/);
        return false;
    },
    computeStatus: function (currentPoint, previousPoint)
    {
        var chartData = this.get('chartData');
        var diffFromBaseline = this._relativeDifferentToLaterPointInTimeSeries(currentPoint, chartData.baseline);
        var diffFromTarget = this._relativeDifferentToLaterPointInTimeSeries(currentPoint, chartData.target);

        var label = '';
        var className = '';
        var formatter = d3.format('.3p');

        function labelForDiff(diff, name) { return formatter(Math.abs(diff)) + ' ' + (diff > 0 ? 'above' : 'below') + ' ' + name; }

        var smallerIsBetter = chartData.smallerIsBetter;
        if (diffFromBaseline !== undefined && diffFromTarget !== undefined) {
            if (diffFromBaseline > 0 == smallerIsBetter) {
                label = labelForDiff(diffFromBaseline, 'baseline');
                className = 'worse';
            } else if (diffFromTarget < 0 == smallerIsBetter) {
                label = labelForDiff(diffFromBaseline, 'target');
                className = 'better';
            } else
                label = formatter(Math.abs(diffFromTarget)) + ' until target';
        } else if (diffFromBaseline !== undefined) {
            label = labelForDiff(diffFromBaseline, 'baseline');
            if (diffFromBaseline > 0 == smallerIsBetter)
                className = 'worse';
        } else if (diffFromTarget !== undefined) {
            label = labelForDiff(diffFromTarget, 'target');
            if (diffFromTarget < 0 == smallerIsBetter)
                className = 'better';
        }

        var valueDelta = null;
        var relativeDelta = null;
        if (previousPoint) {
            valueDelta = chartData.deltaFormatter(currentPoint.value - previousPoint.value);
            relativeDelta = d3.format('+.2p')((currentPoint.value - previousPoint.value) / previousPoint.value);
        }
        return {
            className: className,
            label: label,
            currentValue: chartData.formatter(currentPoint.value),
            valueDelta: valueDelta,
            relativeDelta: relativeDelta,
        };
    },
    _relativeDifferentToLaterPointInTimeSeries: function (currentPoint, timeSeries)
    {
        if (!currentPoint || !timeSeries)
            return undefined;

        var referencePoint = timeSeries.findPointAfterTime(currentPoint.time);
        if (!referencePoint)
            return undefined;

        return (currentPoint.value - referencePoint.value) / referencePoint.value;
    },
    latestStatus: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData || !chartData.current)
            return null;

        var lastPoint = chartData.current.lastPoint();
        if (!lastPoint)
            return null;

        return this.computeStatus(lastPoint, chartData.current.previousPoint(lastPoint));
    }.property('chartData'),
    updateStatisticsTools: function ()
    {
        var movingAverageStrategies = StatisticsStrategies.MovingAverageStrategies.map(this._cloneStrategy.bind(this));
        this.set('movingAverageStrategies', [{label: 'None'}].concat(movingAverageStrategies));
        this.set('chosenMovingAverageStrategy', this._configureStrategy(movingAverageStrategies, this.get('movingAverageConfig')));

        var envelopingStrategies = StatisticsStrategies.EnvelopingStrategies.map(this._cloneStrategy.bind(this));
        this.set('envelopingStrategies', [{label: 'None'}].concat(envelopingStrategies));
        this.set('chosenEnvelopingStrategy', this._configureStrategy(envelopingStrategies, this.get('envelopingConfig')));

        var testRangeSelectionStrategies = StatisticsStrategies.TestRangeSelectionStrategies.map(this._cloneStrategy.bind(this));
        this.set('testRangeSelectionStrategies', [{label: 'None'}].concat(testRangeSelectionStrategies));
        this.set('chosenTestRangeSelectionStrategy', this._configureStrategy(testRangeSelectionStrategies, this.get('testRangeSelectionConfig')));

        var anomalyDetectionStrategies = StatisticsStrategies.AnomalyDetectionStrategy.map(this._cloneStrategy.bind(this));
        this.set('anomalyDetectionStrategies', anomalyDetectionStrategies);
    }.on('init'),
    _cloneStrategy: function (strategy)
    {
        var parameterList = (strategy.parameterList || []).map(function (param) { return Ember.Object.create(param); });
        return Ember.Object.create({
            id: strategy.id,
            label: strategy.label,
            isSegmentation: strategy.isSegmentation,
            description: strategy.description,
            parameterList: parameterList,
            execute: strategy.execute,
        });
    },
    _configureStrategy: function (strategies, config)
    {
        if (!config || !config[0])
            return null;

        var id = config[0];
        var chosenStrategy = strategies.find(function (strategy) { return strategy.id == id });
        if (!chosenStrategy)
            return null;

        if (chosenStrategy.parameterList) {
            for (var i = 0; i < chosenStrategy.parameterList.length; i++)
                chosenStrategy.parameterList[i].value = parseFloat(config[i + 1]);
        }

        return chosenStrategy;
    },
    _updateMovingAverageAndEnvelope: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData)
            return;

        var movingAverageStrategy = this.get('chosenMovingAverageStrategy');
        this._updateStrategyConfigIfNeeded(movingAverageStrategy, 'movingAverageConfig');

        var envelopingStrategy = this.get('chosenEnvelopingStrategy');
        this._updateStrategyConfigIfNeeded(envelopingStrategy, 'envelopingConfig');

        var testRangeSelectionStrategy = this.get('chosenTestRangeSelectionStrategy');
        this._updateStrategyConfigIfNeeded(testRangeSelectionStrategy, 'testRangeSelectionConfig');

        var anomalyDetectionStrategies = this.get('anomalyDetectionStrategies').filterBy('enabled');
        var result = this._computeMovingAverageAndOutliers(chartData, movingAverageStrategy, envelopingStrategy, testRangeSelectionStrategy, anomalyDetectionStrategies);
        if (!result)
            return;

        chartData.movingAverage = result.movingAverage;
        this.set('highlightedItems', result.anomalies);
        var currentTimeSeriesData = chartData.current.series();
        this.set('testRangeCandidates', result.testRangeCandidates.map(function (range) {
            return Ember.Object.create({
                startRun: currentTimeSeriesData[range[0]].measurement.id(),
                endRun: currentTimeSeriesData[range[1]].measurement.id(),
                status: 'testingRange',
            });
        }));
    },
    _movingAverageOrEnvelopeStrategyDidChange: function () {
        var chartData = this.get('chartData');
        if (!chartData)
            return;
        this._setNewChartData(chartData);
    }.observes('chosenMovingAverageStrategy', 'chosenMovingAverageStrategy.parameterList.@each.value',
        'chosenEnvelopingStrategy', 'chosenEnvelopingStrategy.parameterList.@each.value',
        'chosenTestRangeSelectionStrategy', 'chosenTestRangeSelectionStrategy.parameterList.@each.value',
        'anomalyDetectionStrategies.@each.enabled'),
    _computeMovingAverageAndOutliers: function (chartData, movingAverageStrategy, envelopingStrategy, testRangeSelectionStrategy, anomalyDetectionStrategies)
    {
        var currentTimeSeriesData = chartData.current.series();

        var rawValues = chartData.current.rawValues();
        var movingAverageIsSetByUser = movingAverageStrategy && movingAverageStrategy.execute;
        if (!movingAverageIsSetByUser)
            return null;

        var movingAverageValues = StatisticsStrategies.executeStrategy(movingAverageStrategy, rawValues);
        if (!movingAverageValues)
            return null;

        var testRangeCandidates = [];
        if (movingAverageStrategy && movingAverageStrategy.isSegmentation && testRangeSelectionStrategy && testRangeSelectionStrategy.execute)
            testRangeCandidates = StatisticsStrategies.executeStrategy(testRangeSelectionStrategy, rawValues, [movingAverageValues]);

        if (envelopingStrategy && envelopingStrategy.execute) {
            var envelopeDelta = StatisticsStrategies.executeStrategy(envelopingStrategy, rawValues, [movingAverageValues]);
            var anomalies = {};
            if (anomalyDetectionStrategies.length) {
                var isAnomalyArray = new Array(currentTimeSeriesData.length);
                for (var strategy of anomalyDetectionStrategies) {
                    var anomalyLengths = StatisticsStrategies.executeStrategy(strategy, rawValues, [movingAverageValues, envelopeDelta]);
                    for (var i = 0; i < currentTimeSeriesData.length; i++)
                        isAnomalyArray[i] = isAnomalyArray[i] || anomalyLengths[i];
                }
                for (var i = 0; i < isAnomalyArray.length; i++) {
                    if (!isAnomalyArray[i])
                        continue;
                    anomalies[currentTimeSeriesData[i].measurement.id()] = true;
                    while (isAnomalyArray[i] && i < isAnomalyArray.length)
                        ++i;
                }
            }
        }

        var movingAverageTimeSeries = null;
        if (movingAverageIsSetByUser) {
            movingAverageTimeSeries = new TimeSeries(currentTimeSeriesData.map(function (point, index) {
                var value = movingAverageValues[index];
                return {
                    measurement: point.measurement,
                    time: point.time,
                    value: value,
                    interval: envelopeDelta !== null ? [value - envelopeDelta, value + envelopeDelta] : null,
                }
            }));
        }

        return {
            movingAverage: movingAverageTimeSeries,
            anomalies: anomalies,
            testRangeCandidates: testRangeCandidates,
        };
    },
    _updateStrategyConfigIfNeeded: function (strategy, configName)
    {
        var config = null;
        if (strategy && strategy.execute)
            config = [strategy.id].concat((strategy.parameterList || []).map(function (param) { return param.value; }));

        if (JSON.stringify(config) != JSON.stringify(this.get(configName)))
            this.set(configName, config);
    },
    _updateDetails: function ()
    {
        var selectedPoints = this.get('selectedPoints');
        var currentPoint = this.get('hoveredOrSelectedItem');
        if (!selectedPoints && !currentPoint) {
            this.set('details', null);
            return;
        }

        var currentMeasurement;
        var previousPoint;
        if (!selectedPoints)
            previousPoint = currentPoint.series.previousPoint(currentPoint);
        else {
            currentPoint = selectedPoints[selectedPoints.length - 1];
            previousPoint = selectedPoints[0];
        }
        var currentMeasurement = currentPoint.measurement;
        var oldMeasurement = previousPoint ? previousPoint.measurement : null;

        var formattedRevisions = currentMeasurement.formattedRevisions(oldMeasurement);
        var revisions = App.Manifest.get('repositories')
            .filter(function (repository) { return formattedRevisions[repository.get('id')]; })
            .map(function (repository) {
            var revision = Ember.Object.create(formattedRevisions[repository.get('id')]);
            revision['url'] = revision.previousRevision
                ? repository.urlForRevisionRange(revision.previousRevision, revision.currentRevision)
                : repository.urlForRevision(revision.currentRevision);
            revision['name'] = repository.get('name');
            revision['repository'] = repository;
            return revision; 
        });

        var buildNumber = null;
        var buildURL = null;
        if (!selectedPoints) {
            buildNumber = currentMeasurement.buildNumber();
            var builder = App.Manifest.builder(currentMeasurement.builderId());
            if (builder)
                buildURL = builder.urlFromBuildNumber(buildNumber);
        }

        this.set('details', Ember.Object.create({
            status: this.computeStatus(currentPoint, previousPoint),
            buildNumber: buildNumber,
            buildURL: buildURL,
            buildTime: currentMeasurement.formattedBuildTime(),
            revisions: revisions,
        }));
    }.observes('hoveredOrSelectedItem', 'selectedPoints'),
});

App.encodePrettifiedJSON = function (plain)
{
    function numberIfPossible(string) {
        return string == parseInt(string) ? parseInt(string) : string;
    }

    function recursivelyConvertNumberIfPossible(input) {
        if (input instanceof Array) {
            return input.map(recursivelyConvertNumberIfPossible);
        }
        return numberIfPossible(input);
    }

    return JSON.stringify(recursivelyConvertNumberIfPossible(plain))
        .replace(/\[/g, '(').replace(/\]/g, ')').replace(/\,/g, '-');
}

App.decodePrettifiedJSON = function (encoded)
{
    var parsed = encoded.replace(/\(/g, '[').replace(/\)/g, ']').replace(/\-/g, ',');
    try {
        return JSON.parse(parsed);
    } catch (exception) {
        return null;
    }
}

App.ChartsController = Ember.Controller.extend({
    queryParams: ['paneList', 'zoom', 'since'],
    needs: ['pane'],
    _currentEncodedPaneList: null,
    panes: [],
    platforms: null,
    sharedZoom: null,
    startTime: null,
    present: Date.now(),
    defaultSince: Date.now() - 7 * 24 * 3600 * 1000,

    addPane: function (pane)
    {
        this.panes.unshiftObject(pane);
    },

    removePane: function (pane)
    {
        this.panes.removeObject(pane);
    },

    refreshPanes: function()
    {
        var paneList = this.get('paneList');
        if (paneList === this._currentEncodedPaneList)
            return;

        var panes = this._parsePaneList(paneList || '[]');
        if (!panes) {
            console.log('Failed to parse', jsonPaneList, exception);
            return;
        }
        this.set('panes', panes);
        this._currentEncodedPaneList = paneList;
    }.observes('paneList').on('init'),

    refreshZoom: function()
    {
        var zoom = this.get('zoom');
        if (!zoom) {
            this.set('sharedZoom', null);
            return;
        }

        zoom = zoom.split('-');
        var selection = new Array(2);
        try {
            selection[0] = new Date(parseFloat(zoom[0]));
            selection[1] = new Date(parseFloat(zoom[1]));
        } catch (error) {
            console.log('Failed to parse the zoom', zoom);
        }
        this.set('sharedZoom', selection);

        var startTime = this.get('startTime');
        if (startTime && startTime > selection[0])
            this.set('startTime', selection[0]);

    }.observes('zoom').on('init'),

    _startTimeChanged: function () {
        this.set('sharedDomain', [this.get('startTime'), this.get('present')]);
        this._scheduleQueryStringUpdate();
    }.observes('startTime'),

    _sinceChanged: function () {
        var since = parseInt(this.get('since'));
        if (isNaN(since))
            since = this.defaultSince;
        this.set('startTime', new Date(since));
    }.observes('since').on('init'),

    _parsePaneList: function (encodedPaneList)
    {
        var parsedPaneList = App.decodePrettifiedJSON(encodedPaneList);
        if (!parsedPaneList)
            return null;

        // FIXME: Don't re-create all panes.
        var self = this;
        return parsedPaneList.map(function (paneInfo) {
            var timeRange = null;
            var selectedItem = null;
            if (paneInfo[2] instanceof Array) {
                var timeRange = paneInfo[2];
                try {
                    timeRange = [new Date(timeRange[0]), new Date(timeRange[1])];
                } catch (error) {
                    console.log("Failed to parse the time range:", timeRange, error);
                }
            } else
                selectedItem = paneInfo[2];

            return App.Pane.create({
                store: self.store,
                info: paneInfo,
                platformId: paneInfo[0],
                metricId: paneInfo[1],
                selectedItem: selectedItem,
                timeRange: timeRange,
                showFullYAxis: paneInfo[3],
                movingAverageConfig: paneInfo[4],
                envelopingConfig: paneInfo[5],
                testRangeSelectionConfig: paneInfo[6],
            });
        });
    },

    _serializePaneList: function (panes)
    {
        if (!panes.length)
            return undefined;
        var self = this;
        return App.encodePrettifiedJSON(panes.map(function (pane) {
            return [
                pane.get('platformId'),
                pane.get('metricId'),
                pane.get('timeRange') ? pane.get('timeRange').map(function (date) { return date.getTime() }) : pane.get('selectedItem'),
                pane.get('showFullYAxis'),
                pane.get('movingAverageConfig'),
                pane.get('envelopingConfig'),
                pane.get('testRangeSelectionConfig'),
            ];
        }));
    },

    _scheduleQueryStringUpdate: function ()
    {
        Ember.run.debounce(this, '_updateQueryString', 1000);
    }.observes('sharedZoom', 'panes.@each.platform', 'panes.@each.metric', 'panes.@each.selectedItem', 'panes.@each.timeRange',
        'panes.@each.showFullYAxis', 'panes.@each.movingAverageConfig', 'panes.@each.envelopingConfig', 'panes.@each.testRangeSelectionConfig'),

    _updateQueryString: function ()
    {
        this._currentEncodedPaneList = this._serializePaneList(this.get('panes'));
        this.set('paneList', this._currentEncodedPaneList);

        var zoom = undefined;
        var sharedZoom = this.get('sharedZoom');
        if (sharedZoom && !App.domainsAreEqual(sharedZoom, this.get('sharedDomain')))
            zoom = +sharedZoom[0] + '-' + +sharedZoom[1];
        this.set('zoom', zoom);

        if (this.get('startTime') - this.defaultSince)
            this.set('since', this.get('startTime') - 0);
    },

    actions: {
        addPaneByMetricAndPlatform: function (param)
        {
            this.addPane(App.Pane.create({
                store: this.store,
                platformId: param.platform.get('id'),
                metricId: param.metric.get('id'),
                showingDetails: false
            }));
        },
        addAlternativePanes: function (pane, platform, metrics)
        {
            var panes = this.panes;
            var store = this.store;
            var startingIndex = panes.indexOf(pane) + 1;
            metrics.forEach(function (metric, index) {
                panes.insertAt(startingIndex + index, App.Pane.create({
                    store: store,
                    platformId: platform.get('id'),
                    metricId: metric.get('id'),
                    showingDetails: false
                }));
            })
        }
    },

    init: function ()
    {
        this._super();
        var self = this;
        App.buildPopup(this.store, 'addPaneByMetricAndPlatform').then(function (platforms) {
            self.set('platforms', platforms);
        });
    },
});

App.buildPopup = function(store, action, position)
{
    return App.Manifest.fetch(store).then(function () {
        return App.Manifest.get('platforms').sortBy('label').map(function (platform) {
            return App.PlatformProxyForPopup.create({content: platform,
                action: action, position: position});
        });
    });
}

App.PlatformProxyForPopup = Ember.ObjectProxy.extend({
    children: function ()
    {
        var platform = this.content;
        var containsTest = this.content.containsTest.bind(this.content);
        var action = this.get('action');
        var position = this.get('position');
        return App.Manifest.get('topLevelTests')
            .filter(containsTest)
            .map(function (test) {
                return App.TestProxyForPopup.create({content: test, platform: platform, action: action, position: position});
            });
    }.property('App.Manifest.topLevelTests'),
});

App.TestProxyForPopup = Ember.ObjectProxy.extend({
    platform: null,
    children: function ()
    {
        this._updateChildren();
        return this._children;
    }.property('childTests', 'metrics'),
    actionName: function ()
    {
        this._updateChildren();
        return this._actionName;
    }.property('childTests', 'metrics'),
    actionArgument: function ()
    {
        this._updateChildren();
        return this._actionArgument;
    }.property('childTests', 'metrics'),
    _updateChildren: function ()
    {
        var platform = this.get('platform');
        var action = this.get('action');
        var position = this.get('position');

        var childTests = this.get('childTests')
            .filter(function (test) { return platform.containsTest(test); })
            .map(function (test) {
                return App.TestProxyForPopup.create({content: test, platform: platform, action: action, position: position});
            });

        var metrics = this.get('metrics')
            .filter(function (metric) { return platform.containsMetric(metric); })
            .map(function (metric) {
                var aggregator = metric.get('aggregator');
                return {
                    actionName: action,
                    actionArgument: {platform: platform, metric: metric, position:position},
                    label: metric.get('label')
                };
            });

        if (childTests.length && metrics.length)
            metrics.push({isSeparator: true});
        else if (metrics.length == 1) {
            this._actionName = action;
            this._actionArgument = metrics[0].actionArgument;
            return;
        }

        this._actionName = null;
        this._actionArgument = null;
        this._children = metrics.concat(childTests);
    },
});

App.domainsAreEqual = function (domain1, domain2) {
    return (!domain1 && !domain2) || (domain1 && domain2 && !(domain1[0] - domain2[0]) && !(domain1[1] - domain2[1]));
}

App.PaneController = Ember.ObjectController.extend({
    needs: ['charts'],
    sharedTime: Ember.computed.alias('parentController.sharedTime'),
    sharedSelection: Ember.computed.alias('parentController.sharedSelection'),
    selection: null,
    actions: {
        toggleDetails: function()
        {
            this.toggleProperty('showingDetails');
        },
        close: function ()
        {
            this.parentController.removePane(this.get('model'));
        },
        toggleBugsPane: function ()
        {
            if (this.toggleProperty('showingAnalysisPane')) {
                this.set('showingSearchPane', false);
                this.set('showingStatPane', false);
            }
        },
        toggleShowOutlier: function ()
        {
            this.get('model').toggleProperty('showOutlier');
        },
        createAnalysisTask: function ()
        {
            var name = this.get('newAnalysisTaskName');
            var points = this.get('selectedPoints');
            Ember.assert('The analysis name should not be empty', name);
            Ember.assert('There should be at least two points in the range', points && points.length >= 2);

            var newWindow = window.open();
            var self = this;
            App.AnalysisTask.create(name, points[0].measurement, points[points.length - 1].measurement).then(function (data) {
                // FIXME: Update the UI to show the new analysis task.
                var url = App.Router.router.generate('analysisTask', data['taskId']);
                newWindow.location.href = '#' + url;
                self.get('model').fetchAnalyticRanges();
            }, function (error) {
                newWindow.close();
                if (error === 'DuplicateAnalysisTask') {
                    // FIXME: Duplicate this error more gracefully.
                }
                alert(error);
            });
        },
        toggleSearchPane: function ()
        {
            if (!App.Manifest.repositoriesWithReportedCommits)
                return;
            var model = this.get('model');
            if (!model.get('commitSearchRepository'))
                model.set('commitSearchRepository', App.Manifest.repositoriesWithReportedCommits[0]);
            if (this.toggleProperty('showingSearchPane')) {
                this.set('showingAnalysisPane', false);
                this.set('showingStatPane', false);
            }
        },
        searchCommit: function () {
            var model = this.get('model');
            model.searchCommit(model.get('commitSearchRepository'), model.get('commitSearchKeyword'));                
        },
        toggleStatPane: function ()
        {
            if (this.toggleProperty('showingStatPane')) {
                this.set('showingSearchPane', false);
                this.set('showingAnalysisPane', false);
            }
        },
        zoomed: function (selection)
        {
            this.set('mainPlotDomain', selection ? selection : this.get('overviewDomain'));
            if (selection)
                this.set('overviewSelection', selection);
            Ember.run.debounce(this, 'propagateZoom', 100);
        },
    },
    _overviewSelectionChanged: function ()
    {
        var overviewSelection = this.get('overviewSelection');
        if (App.domainsAreEqual(overviewSelection, this.get('mainPlotDomain')))
            return;
        this.set('mainPlotDomain', overviewSelection || this.get('overviewDomain'));
        Ember.run.debounce(this, 'propagateZoom', 100);
    }.observes('overviewSelection'),
    _sharedDomainChanged: function ()
    {
        var newDomain = this.get('parentController').get('sharedDomain');
        if (App.domainsAreEqual(newDomain, this.get('overviewDomain')))
            return;
        this.set('overviewDomain', newDomain);
        if (!this.get('overviewSelection'))
            this.set('mainPlotDomain', newDomain);
    }.observes('parentController.sharedDomain').on('init'),
    propagateZoom: function ()
    {
        this.get('parentController').set('sharedZoom', this.get('mainPlotDomain'));
    },
    _sharedZoomChanged: function ()
    {
        var newSelection = this.get('parentController').get('sharedZoom');
        if (App.domainsAreEqual(newSelection, this.get('mainPlotDomain')))
            return;
        this.set('mainPlotDomain', newSelection || this.get('overviewDomain'));
        this.set('overviewSelection', newSelection);
    }.observes('parentController.sharedZoom').on('init'),
    _updateCanAnalyze: function ()
    {
        var pane = this.get('model');
        var points = pane.get('selectedPoints');
        this.set('cannotAnalyze', !this.get('newAnalysisTaskName') || !points || points.length < 2);
        this.set('cannotMarkOutlier', !!points || !this.get('selectedItem'));

        var selectedMeasurement = this.selectedMeasurement();
        this.set('selectedItemIsMarkedOutlier', selectedMeasurement && selectedMeasurement.markedOutlier());

    }.observes('newAnalysisTaskName', 'model.selectedPoints', 'model.selectedItem').on('init'),
    selectedMeasurement: function () {
        var chartData = this.get('model').get('chartData');
        var selectedItem = this.get('selectedItem');
        if (!chartData || !selectedItem)
            return null;
        var point = chartData.current.findPointByMeasurementId(selectedItem);
        Ember.assert('selectedItem should always be in the current chart data', point);
        return point.measurement;
    },
    showOutlierTitle: function ()
    {
        return this.get('showOutlier') ? 'Hide outliers' : 'Show outliers';
    }.property('showOutlier'),
    _selectedItemIsMarkedOutlierDidChange: function ()
    {
        var selectedMeasurement = this.selectedMeasurement();
        if (!selectedMeasurement)
            return;
        var selectedItemIsMarkedOutlier = this.get('selectedItemIsMarkedOutlier');
        if (selectedMeasurement.markedOutlier() == selectedItemIsMarkedOutlier)
            return;
        var pane = this.get('model');
        selectedMeasurement.setMarkedOutlier(!!selectedItemIsMarkedOutlier).then(function () {
            alert(selectedItemIsMarkedOutlier ? 'Marked the point as an outlier' : 'The point is no longer marked as an outlier');
            pane.refetchRuns();
        }, function (error) {
            alert('Failed to update the status:' + error);
        });
    }.observes('selectedItemIsMarkedOutlier'),
    alternativePanes: function ()
    {
        var pane = this.get('model');
        var metric = pane.get('metric');
        var currentPlatform = pane.get('platform');
        var platforms = App.Manifest.get('platforms');
        if (!platforms || !metric)
            return;

        var exitingPlatforms = {};
        this.get('parentController').get('panes').forEach(function (pane) {
            if (pane.get('metricId') == metric.get('id'))
                exitingPlatforms[pane.get('platformId')] = true;
        });

        var alternativePanes = platforms.filter(function (platform) {
            return !exitingPlatforms[platform.get('id')] && platform.containsMetric(metric);
        }).map(function (platform) {
            return {
                pane: pane,
                platform: platform,
                metrics: [metric],
                label: platform.get('label')
            };
        });

        var childMetrics = metric.get('childMetrics');
        if (childMetrics && childMetrics.length) {
            alternativePanes.push({
                pane: pane,
                platform: currentPlatform,
                metrics: childMetrics,
                label: 'Breakdown',
            });
        }

        return alternativePanes;
    }.property('model.metric', 'model.platform', 'App.Manifest.platforms',
        'parentController.panes.@each.platformId', 'parentController.panes.@each.metricId'),
});

App.AnalysisRoute = Ember.Route.extend({
    model: function () {
        var store = this.store;
        return App.Manifest.fetch(store).then(function () {
            return store.findAll('analysisTask').then(function (tasks) {
                return Ember.Object.create({'tasks': tasks.sortBy('createdAt').toArray().reverse()});
            });
        });
    },
});

App.AnalysisTaskRoute = Ember.Route.extend({
    model: function (param)
    {
        var store = this.store;
        return App.Manifest.fetch(store).then(function () {
            return store.find('analysisTask', param.taskId);
        });
    },
});

App.AnalysisTaskController = Ember.Controller.extend({
    label: Ember.computed.alias('model.name'),
    platform: Ember.computed.alias('model.platform'),
    metric: Ember.computed.alias('model.metric'),
    details: Ember.computed.alias('pane.details'),
    roots: [],
    bugTrackers: [],
    possibleRepetitionCounts: [1, 2, 3, 4, 5, 6],
    analysisResultOptions: [
        {label: 'Still in investigation', result: null},
        {label: 'Inconclusive', result: 'inconclusive', needed: true},
        {label: 'Definite progression', result: 'progression', needed: true},
        {label: 'Definite regression', result: 'regression', needed: true},
        {label: 'No change', result: 'unchanged', needsFeedback: true},
    ],
    shouldNotHaveBeenCreated: false,
    needsFeedback: function ()
    {
        var chosen = this.get('chosenAnalysisResult');
        return chosen && chosen.needsFeedback;
    }.property('chosenAnalysisResult'),
    _updateChosenAnalysisResult: function ()
    {
        var analysisTask = this.get('model');
        if (!analysisTask)
            return;
        var currentResult = analysisTask.get('result');
        for (var option of this.analysisResultOptions) {
            if (option.result == currentResult) {
                this.set('chosenAnalysisResult', option);
                break;                
            }
        }
    }.observes('model'),
    _taskUpdated: function ()
    {
        var model = this.get('model');
        if (!model)
            return;

        App.Manifest.fetch(this.store).then(this._fetchedManifest.bind(this));
        this.set('pane', App.Pane.create({
            store: this.store,
            platformId: model.get('platform').get('id'),
            metricId: model.get('metric').get('id'),
        }));

        var self = this;
        model.get('testGroups').then(function (groups) {
            self.set('testGroupPanes', groups.map(function (group) { return App.TestGroupPane.create({content: group}); }));
        });
    }.observes('model', 'model.testGroups').on('init'),
    _fetchedManifest: function ()
    {
        var trackerIdToBugNumber = {};
        this.get('model').get('bugs').forEach(function (bug) {
            trackerIdToBugNumber[bug.get('bugTracker').get('id')] = bug.get('number');
        });

        this.set('bugTrackers', App.Manifest.get('bugTrackers').map(function (bugTracker) {
            var bugNumber = trackerIdToBugNumber[bugTracker.get('id')];
            return Ember.ObjectProxy.create({
                elementId: 'bug-' + bugTracker.get('id'),
                content: bugTracker,
                bugNumber: bugNumber,
                bugUrl: bugTracker.urlFromBugNumber(bugNumber),
                editedBugNumber: bugNumber,
            });
        }));
    },
    _chartDataChanged: function ()
    {
        var pane = this.get('pane');
        if (!pane)
            return;

        var chartData = pane.get('chartData');
        if (!chartData)
            return null;

        var currentTimeSeries = chartData.current;
        Ember.assert('chartData.current should always be defined', currentTimeSeries);

        var start = currentTimeSeries.findPointByMeasurementId(this.get('model').get('startRun'));
        var end = currentTimeSeries.findPointByMeasurementId(this.get('model').get('endRun'));
        if (!start || !end) {
            if (!pane.get('showOutlier'))
                pane.set('showOutlier', true);
            return;
        }

        var highlightedItems = {};
        highlightedItems[start.measurement.id()] = true;
        highlightedItems[end.measurement.id()] = true;

        var formatedPoints = currentTimeSeries.seriesBetweenPoints(start, end).map(function (point, index) {
            return {
                id: point.measurement.id(),
                measurement: point.measurement,
                label: 'Point ' + (index + 1),
                value: chartData.formatter(point.value),
            };
        });

        var margin = (end.time - start.time) * 0.1;
        this.set('highlightedItems', highlightedItems);
        this.set('overviewEndPoints', [start, end]);
        this.set('analysisPoints', formatedPoints);

        var overviewDomain = [start.time - margin, +end.time + margin];

        var testGroupPanes = this.get('testGroupPanes');
        if (testGroupPanes) {
            testGroupPanes.setEach('overviewPane', pane);
            testGroupPanes.setEach('overviewDomain', overviewDomain);
        }

        this.set('overviewDomain', overviewDomain);
    }.observes('pane.chartData'),
    updateRootConfigurations: function ()
    {
        var analysisPoints = this.get('analysisPoints');
        if (!analysisPoints)
            return;
        var repositoryToRevisions = {};
        analysisPoints.forEach(function (point, pointIndex) {
            var revisions = point.measurement.formattedRevisions();
            for (var repositoryId in revisions) {
                if (!repositoryToRevisions[repositoryId])
                    repositoryToRevisions[repositoryId] = new Array();
                var revision = revisions[repositoryId];
                repositoryToRevisions[repositoryId].push({time: point.measurement.latestCommitTime(), value: revision.currentRevision});
            }
        });

        var commitsPromises = [];
        var repositoryToIndex = {};
        for (var repositoryId in repositoryToRevisions) {
            var revisions = repositoryToRevisions[repositoryId].sort(function (a, b) { return a.time - b.time; });
            repositoryToIndex[repositoryId] = commitsPromises.length;
            commitsPromises.push(CommitLogs.fetchCommits(repositoryId, revisions[0].value, revisions[revisions.length - 1].value));
        }

        var self = this;
        this.get('model').get('triggerable').then(function (triggerable) {
            if (!triggerable)
                return;
            Ember.RSVP.Promise.all(commitsPromises).then(function (commitsList) {
                self.set('configurations', ['A', 'B']);
                self.set('rootConfigurations', triggerable.get('acceptedRepositories').map(function (repository) {
                    return self._createConfiguration(repository, commitsList[repositoryToIndex[repository.get('id')]], analysisPoints);
                }));
            });
        });
    }.observes('analysisPoints'),
    _createConfiguration: function(repository, commits, analysisPoints) {
        var repositoryId = repository.get('id');

        var revisionToPoints = {};
        var missingPoints = [];
        analysisPoints.forEach(function (point, pointIndex) {
            var revision = point.measurement.revisionForRepository(repositoryId);
            if (!revision) {
                missingPoints.push(pointIndex);
                return;
            }
            if (!revisionToPoints[revision])
                revisionToPoints[revision] = [];
            revisionToPoints[revision].push(pointIndex);
        });

        if (!commits || !commits.length) {
            commits = [];
            for (var revision in revisionToPoints)
                commits.push({revision: revision});
        }

        var options = [{label: 'None'}, {label: 'Custom', isCustom: true}];
        if (missingPoints.length) {
            options[0].label += ' ' + this._labelForPoints(missingPoints);
            options[0].points = missingPoints;
        }

        for (var commit of commits) {
            var revision = commit.revision;
            var points = revisionToPoints[revision];
            var label = Measurement.formatRevisionRange(revision).label;
            if (points)
                label += ' ' + this._labelForPoints(points);
            options.push({value: revision, label: label, points: points});
        }

        var firstOption = null;
        var lastOption = null;
        for (var option of options) {
            var points = option.points;
            if (!points)
                continue;
            if (points.indexOf(0) >= 0)
                firstOption = option;
            if (points.indexOf(analysisPoints.length - 1) >= 0)
                lastOption = option;
        }

        return Ember.Object.create({
            repository: repository,
            name: repository.get('name'),
            sets: [
                Ember.Object.create({name: 'A[' + repositoryId + ']',
                    options: options,
                    selection: firstOption}),
                Ember.Object.create({name: 'B[' + repositoryId + ']',
                    options: options,
                    selection: lastOption}),
            ]});
    },
    _labelForPoints: function (points)
    {
        var serializedPoints = this._serializeNumbersSkippingConsecutiveEntries(points);
        return ['(', points.length > 1 ? 'points' : 'point', serializedPoints, ')'].join(' ');
    },
    _serializeNumbersSkippingConsecutiveEntries: function (numbers) {
        var result = numbers[0];
        for (var i = 1; i < numbers.length; i++) {
            if (numbers[i - 1] + 1 == numbers[i]) {
                while (numbers[i] + 1 == numbers[i + 1])
                    i++;
                result += '-' + numbers[i];
                continue;
            }
            result += ', ' + numbers[i]
        }
        return result;
    },
    actions: {
        addBug: function (bugTracker, bugNumber)
        {
            var model = this.get('model');
            if (!bugTracker)
                bugTracker = this.get('bugTrackers').objectAt(0);
            var bug = {task: this.get('model'), bugTracker: bugTracker.get('content'), number: bugNumber};
            this.store.createRecord('bug', bug).save().then(function () {
                alert('Associated the ' + bugTracker.get('name') + ' ' + bugNumber + ' with this analysis.');
            }, function (error) {
                alert('Failed to associate the bug: ' + error);
            });
        },
        deleteBug: function (bug)
        {
            bug.destroyRecord().then(function () {
                alert('Successfully deassociated the bug.');
            }, function (error) {
                alert('Failed to disassociate the bug: ' + error);
            });
        },
        saveStatus: function ()
        {
            var chosenResult = this.get('chosenAnalysisResult');
            var analysisTask = this.get('model');
            analysisTask.set('result', chosenResult.result);
            if (chosenResult.needed)
                analysisTask.set('needed', true);
            else if (chosenResult.needsFeedback && this.get('notNeeded'))
                analysisTask.set('needed', false);
            else
                analysisTask.set('needed', null);

            analysisTask.saveStatus().then(function () {
                alert('Saved the status');
            }, function (error) {
                alert('Failed to save the status: ' + error);
            });
        },
        createTestGroup: function (name, repetitionCount)
        {
            var analysisTask = this.get('model');
            if (analysisTask.get('testGroups').isAny('name', name)) {
                alert('Cannot create two test groups of the same name.');
                return;
            }

            var roots = {};
            var rootConfigurations = this.get('rootConfigurations').toArray();
            for (var root of rootConfigurations) {
                var sets = root.get('sets');
                var hasSelection = function (item) {
                    var selection = item.get('selection');
                    return selection.value || (selection.isCustom && item.get('customValue'));
                }
                if (!sets.any(hasSelection))
                    continue;
                if (!sets.every(hasSelection)) {
                    alert('Only some configuration specifies ' + root.get('name'));
                    return;
                }
                roots[root.get('name')] = sets.map(function (item) {
                    var selection = item.get('selection');
                    return selection.isCustom ? item.get('customValue') : selection.value;
                });
            }

            App.TestGroup.create(analysisTask, name, roots, repetitionCount).then(function () {
            }, function (error) {
                alert('Failed to create a new test group:' + error);
            });
        },
        toggleShowRequestList: function (configuration)
        {
            configuration.toggleProperty('showRequestList');
        }
    },
    _updateRootsBySelectedPoints: function ()
    {
        var rootConfigurations = this.get('rootConfigurations');
        var pane = this.get('pane');
        if (!rootConfigurations || !pane)
            return;

        var rootSetPoints;
        var selectedPoints = pane.get('selectedPoints');
        if (selectedPoints && selectedPoints.length >= 2)
            rootSetPoints = [selectedPoints[0], selectedPoints[selectedPoints.length - 1]];
        else
            rootSetPoints = this.get('overviewEndPoints');
        if (!rootSetPoints)
            return;

        rootConfigurations.forEach(function (root) {
            root.get('sets').forEach(function (set, setIndex) {
                if (setIndex >= rootSetPoints.length)
                    return;
                var targetRevision = rootSetPoints[setIndex].measurement.revisionForRepository(root.get('repository').get('id'));
                var selectedOption;
                if (targetRevision)
                    selectedOption = set.get('options').find(function (option) { return option.value == targetRevision; });
                set.set('selection', selectedOption || set.get('options')[0]);
            });
        });

    }.observes('pane.selectedPoints'),
});

App.TestGroupPane = Ember.ObjectProxy.extend({
    _populate: function ()
    {
        var buildRequests = this.get('buildRequests');
        var testResults = this.get('testResults');
        if (!buildRequests || !testResults)
            return [];

        var repositories = this._computeRepositoryList();
        this.set('repositories', repositories);

        var requestsByRooSet = this._groupRequestsByConfigurations(buildRequests);

        var configurations = [];
        var index = 0;
        var range = {min: Infinity, max: -Infinity};
        for (var rootSetId in requestsByRooSet) {
            var configLetter = String.fromCharCode('A'.charCodeAt(0) + index++);
            configurations.push(this._createConfigurationSummary(requestsByRooSet[rootSetId], configLetter, range));
        }

        var margin = 0.1 * (range.max - range.min);
        range.max += margin;
        range.min -= margin;

        this.set('configurations', configurations);

        var probabilityFormatter = d3.format('.2p');
        var comparisons = [];
        for (var i = 0; i < configurations.length - 1; i++) {
            var summary1 = configurations[i].summary;
            for (var j = i + 1; j < configurations.length; j++) {
                var summary2 = configurations[j].summary;

                var valueDelta = testResults.deltaFormatter(summary2.value - summary1.value);
                var relativeDelta = d3.format('+.2p')((summary2.value - summary1.value) / summary1.value);

                var stat = this._computeStatisticalSignificance(summary1.measuredValues, summary2.measuredValues);
                comparisons.push({
                    label: summary1.configLetter + ' / ' + summary2.configLetter,
                    difference: isNaN(summary1.value) || isNaN(summary2.value) ? 'N/A' : valueDelta + ' (' + relativeDelta + ') ',
                    result: stat,
                });
            }
        }
        this.set('comparisons', comparisons);
    }.observes('testResults', 'buildRequests'),
    _computeStatisticalSignificance: function (values1, values2)
    {
        var tFormatter = d3.format('.3g');
        var probabilityFormatter = d3.format('.2p');
        var statistics = Statistics.probabilityRangeForWelchsT(values1, values2);
        if (isNaN(statistics.t) || isNaN(statistics.degreesOfFreedom))
            return 'N/A';

        var details = ' (t=' + tFormatter(statistics.t) + ' df=' + tFormatter(statistics.degreesOfFreedom) + ')';

        if (!statistics.range[0])
            return 'Not significant' + details;

        var lowerLimit = probabilityFormatter(statistics.range[0]);
        if (!statistics.range[1])
            return 'Significance > ' + lowerLimit + details;

        return lowerLimit + ' < Significance < ' + probabilityFormatter(statistics.range[1]) + details;
    },
    _updateReferenceChart: function ()
    {
        var configurations = this.get('configurations');
        var chartData = this.get('overviewPane') ? this.get('overviewPane').get('chartData') : null;
        if (!configurations || !chartData || this.get('referenceChart'))
            return;

        var currentTimeSeries = chartData.current;
        if (!currentTimeSeries)
            return;

        var repositories = this.get('repositories');
        var highlightedItems = {};
        var failedToFindPoint = false;
        configurations.forEach(function (config) {
            var revisions = {};
            config.get('rootSet').get('roots').forEach(function (root) {
                revisions[root.get('repository').get('id')] = root.get('revision');
            });
            var point = currentTimeSeries.findPointByRevisions(revisions);
            if (!point) {
                failedToFindPoint = true;
                return;
            }
            highlightedItems[point.measurement.id()] = true;
        });
        if (failedToFindPoint)
            return;

        this.set('referenceChart', {
            data: chartData,
            highlightedItems: highlightedItems,
        });
    }.observes('configurations', 'overviewPane.chartData'),
    _computeRepositoryList: function ()
    {
        var specifiedRepositories = new Ember.Set();
        (this.get('rootSets') || []).forEach(function (rootSet) {
            (rootSet.get('roots') || []).forEach(function (root) {
                specifiedRepositories.add(root.get('repository'));
            });
        });
        var reportedRepositories = new Ember.Set();
        var testResults = this.get('testResults');
        (this.get('buildRequests') || []).forEach(function (request) {
            var point = testResults.current.findPointByBuild(request.get('build'));
            if (!point)
                return;

            var revisionByRepositoryId = point.measurement.formattedRevisions();
            for (var repositoryId in revisionByRepositoryId) {
                var repository = App.Manifest.repository(repositoryId);
                if (!specifiedRepositories.contains(repository))
                    reportedRepositories.add(repository);
            }
        });
        return specifiedRepositories.sortBy('name').concat(reportedRepositories.sortBy('name'));
    },
    _groupRequestsByConfigurations: function (requests, repositoryList)
    {
        var rootSetIdToRequests = {};
        var testGroup = this;
        requests.forEach(function (request) {
            var rootSetId = request.get('rootSet').get('id');
            if (!rootSetIdToRequests[rootSetId])
                rootSetIdToRequests[rootSetId] = [];
            rootSetIdToRequests[rootSetId].push(request);
        });
        return rootSetIdToRequests;
    },
    _createConfigurationSummary: function (buildRequests, configLetter, range)
    {
        var repositories = this.get('repositories');
        var testResults = this.get('testResults');
        var requests = buildRequests.map(function (originalRequest) {
            var point = testResults.current.findPointByBuild(originalRequest.get('build'));
            var revisionByRepositoryId = point ? point.measurement.formattedRevisions() : {};
            return Ember.ObjectProxy.create({
                content: originalRequest,
                revisionList: repositories.map(function (repository, index) {
                    return (revisionByRepositoryId[repository.get('id')] || {label:null}).label;
                }),
                value: point ? point.value : null,
                valueRange: range,
                formattedValue: point ? testResults.formatter(point.value) : null,
                buildLabel: point ? 'Build ' + point.measurement.buildNumber() : null,
            });
        });

        var rootSet = requests ? requests[0].get('rootSet') : null;
        var summaryRevisions = repositories.map(function (repository, index) {
            var revision = rootSet ? rootSet.revisionForRepository(repository) : null;
            if (!revision)
                return requests[0].get('revisionList')[index];
            return Measurement.formatRevisionRange(revision).label;
        });

        requests.forEach(function (request) {
            var revisionList = request.get('revisionList');
            repositories.forEach(function (repository, index) {
                if (revisionList[index] == summaryRevisions[index])
                    revisionList[index] = null;
            });
        });

        var valuesInConfig = requests.mapBy('value').filter(function (value) { return typeof(value) === 'number' && !isNaN(value); });
        var sum = Statistics.sum(valuesInConfig);
        var ciDelta = Statistics.confidenceIntervalDelta(0.95, valuesInConfig.length, sum, Statistics.squareSum(valuesInConfig));
        var mean = sum / valuesInConfig.length;

        range.min = Math.min(range.min, Statistics.min(valuesInConfig));
        range.max = Math.max(range.max, Statistics.max(valuesInConfig));
        if (ciDelta && !isNaN(ciDelta)) {
            range.min = Math.min(range.min, mean - ciDelta);
            range.max = Math.max(range.max, mean + ciDelta);
        }

        var summary = Ember.Object.create({
            isAverage: true,
            configLetter: configLetter,
            revisionList: summaryRevisions,
            formattedValue: isNaN(mean) ? null : testResults.formatWithDeltaAndUnit(mean, ciDelta),
            value: mean,
            measuredValues: valuesInConfig,
            confidenceIntervalDelta: ciDelta,
            valueRange: range,
            statusLabel: App.BuildRequest.aggregateStatuses(requests),
        });

        return Ember.Object.create({summary: summary, requests: requests, rootSet: rootSet});
    },
});

App.BoxPlotComponent = Ember.Component.extend({
    classNames: ['box-plot'],
    range: null,
    value: null,
    delta: null,
    didInsertElement: function ()
    {
        var element = this.get('element');
        var svg = d3.select(element).append('svg')
            .attr('viewBox', '0 0 100 20')
            .attr('preserveAspectRatio', 'none')
            .style({width: '100%', height: '100%'});

        this._percentageRect = svg
            .append('rect')
            .attr('x', 0)
            .attr('y', 0)
            .attr('width', 0)
            .attr('height', 20)
            .attr('class', 'percentage');

        this._deltaRect = svg
            .append('rect')
            .attr('x', 0)
            .attr('y', 5)
            .attr('width', 0)
            .attr('height', 10)
            .attr('class', 'delta')
            .attr('opacity', 0.5)
        this._updateBars();
    },
    _updateBars: function ()
    {
        if (!this._percentageRect || typeof(this._percentage) !== 'number' || isNaN(this._percentage))
            return;

        this._percentageRect.attr('width', this._percentage);
        if (typeof(this._delta) === 'number' && !isNaN(this._delta)) {
            this._deltaRect.attr('x', this._percentage - this._delta);
            this._deltaRect.attr('width', this._delta * 2);
        }
    },
    valueChanged: function ()
    {
        var range = this.get('range');
        var value = this.get('value');
        if (!range || !value)
            return;
        var scalingFactor = 100 / (range.max - range.min);
        var percentage = (value - range.min) * scalingFactor;
        this._percentage = percentage;
        this._delta = this.get('delta') * scalingFactor;
        this._updateBars();
    }.observes('value', 'range').on('init'),
});
