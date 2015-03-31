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
        return App.encodePrettifiedJSON([[this.get('platformId'), this.get('metricId'), null, null, false]]);
    }.property('platformId', 'metricId'),
});

App.IndexRoute = Ember.Route.extend({
    beforeModel: function ()
    {
        var self = this;
        App.Manifest.fetch(this.store).then(function () {
            self.transitionTo('dashboard', App.Manifest.defaultDashboardName());
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
    searchCommit: function (repository, keyword) {
        var self = this;
        var repositoryId = repository.get('id');
        CommitLogs.fetchForTimeRange(repositoryId, null, null, keyword).then(function (commits) {
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
            var store = this.get('store');
            var updateChartData = this._updateChartData.bind(this);
            var handleErrors = this._handleFetchErrors.bind(this, platformId, metricId);
            var useCache = true;
            App.Manifest.fetchRunsWithPlatformAndMetric(store, platformId, metricId, null, useCache).then(function (result) {
                    updateChartData(result);
                    if (!result.shouldRefetch)
                        return;

                    useCache = false;
                    App.Manifest.fetchRunsWithPlatformAndMetric(store, platformId, metricId, null, useCache)
                        .then(updateChartData, handleErrors);
                }, handleErrors);
            this.fetchAnalyticRanges();
        }
    }.observes('platformId', 'metricId').on('init'),
    _updateChartData: function (result)
    {
        this.set('platform', result.platform);
        this.set('metric', result.metric);
        this.set('chartData', result.data);
        this._updateMovingAverageAndEnvelope();
    },
    _handleFetchErrors: function (platformId, metricId, result)
    {
        console.log(platformId, metricId, result)
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
            .find('analysisTask', {platform: platformId, metric: metricId})
            .then(function (tasks) {
                self.set('analyticRanges', tasks.filter(function (task) { return task.get('startRun') && task.get('endRun'); }));
            });
    },
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

        var smallerIsBetter = chartData.smallerIsBetter;
        if (diffFromBaseline !== undefined && diffFromBaseline > 0 == smallerIsBetter) {
            label = formatter(Math.abs(diffFromBaseline)) + ' ' + (smallerIsBetter ? 'above' : 'below') + ' baseline';
            className = 'worse';
        } else if (diffFromTarget !== undefined && diffFromTarget < 0 == smallerIsBetter) {
            label = formatter(Math.abs(diffFromTarget)) + ' ' + (smallerIsBetter ? 'below' : 'above') + ' target';
            className = 'better';
        } else if (diffFromTarget !== undefined)
            label = formatter(Math.abs(diffFromTarget)) + ' until target';

        var valueDelta = previousPoint ? chartData.deltaFormatter(currentPoint.value - previousPoint.value) : null;
        return {
            className: className,
            label: label,
            currentValue: chartData.formatter(currentPoint.value),
            valueDelta: valueDelta,
            relativeDelta: d3.format('+.2p')((currentPoint.value - previousPoint.value) / previousPoint.value),
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
        var movingAverageStrategies = Statistics.MovingAverageStrategies.map(this._cloneStrategy.bind(this));
        this.set('movingAverageStrategies', [{label: 'None'}].concat(movingAverageStrategies));
        this.set('chosenMovingAverageStrategy', this._configureStrategy(movingAverageStrategies, this.get('movingAverageConfig')));

        var envelopingStrategies = Statistics.EnvelopingStrategies.map(this._cloneStrategy.bind(this));
        this.set('envelopingStrategies', [{label: 'None'}].concat(envelopingStrategies));
        this.set('chosenEnvelopingStrategy', this._configureStrategy(envelopingStrategies, this.get('envelopingConfig')));
    }.on('init'),
    _cloneStrategy: function (strategy)
    {
        var parameterList = (strategy.parameterList || []).map(function (param) { return Ember.Object.create(param); });
        return Ember.Object.create({
            id: strategy.id,
            label: strategy.label,
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

        chartData.movingAverage = this._computeMovingAverageAndOutliers(chartData, movingAverageStrategy, envelopingStrategy);
    },
    _movingAverageOrEnvelopeStrategyDidChange: function () {
        this._updateMovingAverageAndEnvelope();

        var newChartData = {};
        var chartData = this.get('chartData');
        if (!chartData)
            return;
        for (var property in chartData)
            newChartData[property] = chartData[property];
        this.set('chartData', newChartData);

    }.observes('chosenMovingAverageStrategy', 'chosenMovingAverageStrategy.parameterList.@each.value',
        'chosenEnvelopingStrategy', 'chosenEnvelopingStrategy.parameterList.@each.value'),
    _computeMovingAverageAndOutliers: function (chartData, movingAverageStrategy, envelopingStrategy)
    {
        var currentTimeSeriesData = chartData.current.series();
        var movingAverageIsSetByUser = movingAverageStrategy && movingAverageStrategy.execute;
        var movingAverageValues = this._executeStrategy(
            movingAverageIsSetByUser ? movingAverageStrategy : Statistics.MovingAverageStrategies[0], currentTimeSeriesData);
        if (!movingAverageValues)
            return null;

        var envelopeIsSetByUser = envelopingStrategy && envelopingStrategy.execute;
        var envelopeDelta = this._executeStrategy(envelopeIsSetByUser ? envelopingStrategy : Statistics.EnvelopingStrategies[0],
            currentTimeSeriesData, [movingAverageValues]);

        for (var i = 0; i < currentTimeSeriesData.length; i++) {
            var currentValue = currentTimeSeriesData[i].value;
            var movingAverageValue = movingAverageValues[i];
            if (currentValue < movingAverageValue - envelopeDelta || movingAverageValue + envelopeDelta < currentValue)
                currentTimeSeriesData[i].isOutlier = true;
        }
        if (!envelopeIsSetByUser)
            envelopeDelta = null;

        if (movingAverageIsSetByUser) {
            return new TimeSeries(currentTimeSeriesData.map(function (point, index) {
                var value = movingAverageValues[index];
                return {
                    measurement: point.measurement,
                    time: point.time,
                    value: value,
                    interval: envelopeDelta !== null ? [value - envelopeDelta, value + envelopeDelta] : null,
                }
            }));            
        }
    },
    _executeStrategy: function (strategy, currentTimeSeriesData, additionalArguments)
    {
        var parameters = (strategy.parameterList || []).map(function (param) {
            var parsed = parseFloat(param.value);
            return Math.min(param.max || Infinity, Math.max(param.min || -Infinity, isNaN(parsed) ? 0 : parsed));
        });
        parameters.push(currentTimeSeriesData.map(function (point) { return point.value }));
        return strategy.execute.apply(window, parameters.concat(additionalArguments));
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
            ];
        }));
    },

    _scheduleQueryStringUpdate: function ()
    {
        Ember.run.debounce(this, '_updateQueryString', 1000);
    }.observes('sharedZoom', 'panes.@each.platform', 'panes.@each.metric', 'panes.@each.selectedItem', 'panes.@each.timeRange',
        'panes.@each.showFullYAxis', 'panes.@each.movingAverageConfig', 'panes.@each.envelopingConfig'),

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
        return App.Manifest.get('platforms').map(function (platform) {
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

        return metrics.concat(childTests);
    }.property('childTests', 'metrics'),
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
            Ember.run.debounce(this, 'propagateZoom', 100);
        },
    },
    _detailsChanged: function ()
    {
        this.set('showingAnalysisPane', false);
    }.observes('details'),
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
        var points = this.get('model').get('selectedPoints');
        this.set('cannotAnalyze', !this.get('newAnalysisTaskName') || !points || points.length < 2);
    }.observes('newAnalysisTaskName', 'model.selectedPoints'),
});

App.AnalysisRoute = Ember.Route.extend({
    model: function () {
        return this.store.findAll('analysisTask').then(function (tasks) {
            return Ember.Object.create({'tasks': tasks});
        });
    },
});

App.AnalysisTaskRoute = Ember.Route.extend({
    model: function (param)
    {
        return this.store.find('analysisTask', param.taskId);
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
                content: bugTracker,
                bugNumber: bugNumber,
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
        if (!currentTimeSeries)
            return null; // FIXME: Report an error.

        var start = currentTimeSeries.findPointByMeasurementId(this.get('model').get('startRun'));
        var end = currentTimeSeries.findPointByMeasurementId(this.get('model').get('endRun'));
        if (!start || !end)
            return null; // FIXME: Report an error.

        var highlightedItems = {};
        highlightedItems[start.measurement.id()] = true;
        highlightedItems[end.measurement.id()] = true;

        var formatedPoints = currentTimeSeries.seriesBetweenPoints(start, end).map(function (point, index) {
            return {
                id: point.measurement.id(),
                measurement: point.measurement,
                label: 'Point ' + (index + 1),
                value: chartData.formatWithUnit(point.value),
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
                    repositoryToRevisions[repositoryId] = new Array(analysisPoints.length);
                var revision = revisions[repositoryId];
                repositoryToRevisions[repositoryId][pointIndex] = {
                    label: point.label + ': ' + revision.label,
                    value: revision.currentRevision,
                };
            }
        });

        var self = this;
        this.get('model').get('triggerable').then(function (triggerable) {
            if (!triggerable)
                return;

            self.set('configurations', ['A', 'B']);
            self.set('rootConfigurations', triggerable.get('acceptedRepositories').map(function (repository) {
                var repositoryId = repository.get('id');
                var options = [{value: ' ', label: 'None'}].concat(repositoryToRevisions[repositoryId]);
                return Ember.Object.create({
                    repository: repository,
                    name: repository.get('name'),
                    sets: [
                        Ember.Object.create({name: 'A[' + repositoryId + ']',
                            options: options,
                            selection: options[1]}),
                        Ember.Object.create({name: 'B[' + repositoryId + ']',
                            options: options,
                            selection: options[options.length - 1]}),
                    ],
                });
            }));
        });
    }.observes('analysisPoints'),
    actions: {
        associateBug: function (bugTracker, bugNumber)
        {
            var model = this.get('model');
            this.store.createRecord('bug',
                {task: this.get('model'), bugTracker: bugTracker.get('content'), number: bugNumber}).save().then(function () {
                    // FIXME: Should we notify the user?
                }, function (error) {
                    alert('Failed to associate the bug: ' + error);
                });
        },
        createTestGroup: function (name, repetitionCount)
        {
            var roots = {};
            this.get('rootConfigurations').map(function (root) {
                roots[root.get('name')] = root.get('sets').map(function (item) { return item.get('selection').value; });
            });
            App.TestGroup.create(this.get('model'), name, roots, repetitionCount).then(function () {
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
                set.set('selection', selectedOption || sets[i].get('options')[0]);
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
    }.observes('testResults', 'buildRequests'),
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
                formattedValue: point ? testResults.formatWithUnit(point.value) : null,
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
            confidenceIntervalDelta: ciDelta,
            valueRange: range,
            statusLabel: App.BuildRequest.aggregateStatuses(requests),
        });

        return Ember.Object.create({summary: summary, items: requests, rootSet: rootSet});
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
