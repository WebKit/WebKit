window.App = Ember.Application.create();

App.Router.map(function () {
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

App.IndexController = Ember.Controller.extend({
    queryParams: ['grid', 'numberOfDays'],
    _previousGrid: {},
    headerColumns: [],
    rows: [],
    numberOfDays: 7,
    editMode: false,

    gridChanged: function ()
    {
        var grid = this.get('grid');
        if (grid === this._previousGrid)
            return;

        var dashboard = null;
        if (grid) {
            dashboard = this.store.createRecord('dashboard', {serialized: grid});
            if (!dashboard.get('headerColumns').length)
                dashboard = null;
        }
        if (!dashboard)
            dashboard = App.Manifest.get('defaultDashboard');
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
    }.observes('grid', 'App.Manifest.defaultDashboard').on('init'),

    updateGrid: function()
    {
        var headers = this.get('headerColumns').map(function (header) { return header.label; });
        var table = [headers].concat(this.get('rows').map(function (row) {
            return [row.get('header')].concat(row.get('cells').map(function (pane) {
                var platformAndMetric = [pane.get('platformId'), pane.get('metricId')];
                return platformAndMetric[0] || platformAndMetric[1] ? platformAndMetric : [];
            }));
        }));
        this._previousGrid = JSON.stringify(table);
        this.set('grid', this._previousGrid);
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
            if (!this.get('editMode'))
                this.updateGrid();
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
            var self = this;

            App.Manifest.fetchRunsWithPlatformAndMetric(this.get('store'), platformId, metricId).then(function (result) {
                self.set('platform', result.platform);
                self.set('metric', result.metric);
                self.set('chartData', App.createChartData(result));
            }, function (result) {
                if (!result || typeof(result) === "string")
                    self.set('failure', 'Failed to fetch the JSON with an error: ' + result);
                else if (!result.platform)
                    self.set('failure', 'Could not find the platform "' + platformId + '"');
                else if (!result.metric)
                    self.set('failure', 'Could not find the metric "' + metricId + '"');
                else
                    self.set('failure', 'An internal error');
            });

            this.fetchAnalyticRanges();
        }
    }.observes('platformId', 'metricId').on('init'),
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
    }
});

App.createChartData = function (data)
{
    var runs = data.runs;
    return {
        current: runs.current.timeSeriesByCommitTime(),
        baseline: runs.baseline ? runs.baseline.timeSeriesByCommitTime() : null,
        target: runs.target ? runs.target.timeSeriesByCommitTime() : null,
        unit: data.unit,
        formatter: data.useSI ? d3.format('.4s') : d3.format('.4g'),
        smallerIsBetter: data.smallerIsBetter,
    };
}

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

        // Don't re-create all panes.
        var self = this;
        return parsedPaneList.map(function (paneInfo) {
            var timeRange = null;
            if (paneInfo[3] && paneInfo[3] instanceof Array) {
                var timeRange = paneInfo[3];
                try {
                    timeRange = [new Date(timeRange[0]), new Date(timeRange[1])];
                } catch (error) {
                    console.log("Failed to parse the time range:", timeRange, error);
                }
            }
            return App.Pane.create({
                store: self.store,
                info: paneInfo,
                platformId: paneInfo[0],
                metricId: paneInfo[1],
                selectedItem: paneInfo[2],
                timeRange: timeRange,
                timeRangeIsLocked: !!paneInfo[4],
            });
        });
    },

    _serializePaneList: function (panes)
    {
        if (!panes.length)
            return undefined;
        return App.encodePrettifiedJSON(panes.map(function (pane) {
            return [
                pane.get('platformId'),
                pane.get('metricId'),
                pane.get('selectedItem'),
                pane.get('timeRange') ? pane.get('timeRange').map(function (date) { return date.getTime() }) : null,
                !!pane.get('timeRangeIsLocked'),
            ];
        }));
    },

    _scheduleQueryStringUpdate: function ()
    {
        Ember.run.debounce(this, '_updateQueryString', 1000);
    }.observes('sharedZoom', 'panes.@each.platform', 'panes.@each.metric', 'panes.@each.selectedItem',
        'panes.@each.timeRange', 'panes.@each.timeRangeIsLocked'),

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
            if (this.toggleProperty('showingAnalysisPane'))
                this.set('showingSearchPane', false);
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
            if (this.toggleProperty('showingSearchPane'))
                this.set('showingAnalysisPane', false);
        },
        searchCommit: function () {
            var model = this.get('model');
            model.searchCommit(model.get('commitSearchRepository'), model.get('commitSearchKeyword'));                
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
        this.set('mainPlotDomain', newSelection);
        this.set('overviewSelection', newSelection);
    }.observes('parentController.sharedZoom').on('init'),
    _updateDetails: function ()
    {
        var selectedPoints = this.get('selectedPoints');
        var currentPoint = this.get('currentItem');
        if (!selectedPoints && !currentPoint) {
            this.set('details', null);
            return;
        }

        var currentMeasurement;
        var oldMeasurement;
        if (currentPoint) {
            currentMeasurement = currentPoint.measurement;
            var previousPoint = currentPoint.series.previousPoint(currentPoint);
            oldMeasurement = previousPoint ? previousPoint.measurement : null;
        } else {
            currentMeasurement = selectedPoints[selectedPoints.length - 1].measurement;
            oldMeasurement = selectedPoints[0].measurement;            
        }

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
        if (currentPoint) {
            buildNumber = currentMeasurement.buildNumber();
            var builder = App.Manifest.builder(currentMeasurement.builderId());
            if (builder)
                buildURL = builder.urlFromBuildNumber(buildNumber);
        }

        var chartData = this.get('chartData');
        var valueDiff = oldMeasurement ? chartData.formatter(currentMeasurement.mean() - oldMeasurement.mean()) : null;
        if (valueDiff && valueDiff > 0)
            valueDiff = '+' + valueDiff;

        this.set('details', Ember.Object.create({
            status: this._computeStatus(currentPoint),
            currentValue: chartData.formatter(currentMeasurement.mean()),
            valueDiff: valueDiff,
            buildNumber: buildNumber,
            buildURL: buildURL,
            buildTime: currentMeasurement.formattedBuildTime(),
            revisions: revisions,
        }));
        this._updateCanAnalyze();
    }.observes('currentItem', 'selectedPoints'),
    _updateCanAnalyze: function ()
    {
        var points = this.get('selectedPoints');
        this.set('cannotAnalyze', !this.get('newAnalysisTaskName') || !points || points.length < 2);
    }.observes('newAnalysisTaskName'),
    _computeStatus: function (currentPoint)
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

        return {className: className, label: label};
    },
    _relativeDifferentToLaterPointInTimeSeries: function (currentPoint, timeSeries)
    {
        if (!currentPoint || !timeSeries)
            return undefined;
        
        var referencePoint = timeSeries.findPointAfterTime(currentPoint.time);
        if (!referencePoint)
            return undefined;

        return (currentPoint.value - referencePoint.value) / referencePoint.value;
    }
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
    testGroups: Ember.computed.alias('model.testGroups'),
    testSets: [],
    roots: [],
    bugTrackers: [],
    possibleRepetitionCounts: [1, 2, 3, 4, 5, 6],
    _taskUpdated: function ()
    {
        var model = this.get('model');
        if (!model)
            return;

        var platformId = model.get('platform').get('id');
        var metricId = model.get('metric').get('id');
        App.Manifest.fetch(this.store).then(this._fetchedManifest.bind(this));
        App.Manifest.fetchRunsWithPlatformAndMetric(this.store, platformId, metricId).then(this._fetchedRuns.bind(this));
    }.observes('model').on('init'),
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
    _fetchedRuns: function (data)
    {
        var runs = data.runs;

        var currentTimeSeries = runs.current.timeSeriesByCommitTime();
        if (!currentTimeSeries)
            return; // FIXME: Report an error.

        var start = currentTimeSeries.findPointByMeasurementId(this.get('model').get('startRun'));
        var end = currentTimeSeries.findPointByMeasurementId(this.get('model').get('endRun'));
        if (!start || !end)
            return; // FIXME: Report an error.

        var highlightedItems = {};
        highlightedItems[start.measurement.id()] = true;
        highlightedItems[end.measurement.id()] = true;

        var chartData = App.createChartData(data);
        var formatedPoints = currentTimeSeries.seriesBetweenPoints(start, end).map(function (point, index) {
            return {
                id: point.measurement.id(),
                measurement: point.measurement,
                label: 'Point ' + (index + 1),
                value: chartData.formatter(point.value) + (data.unit ? ' ' + data.unit : ''),
            };
        });

        var margin = (end.time - start.time) * 0.1;
        this.set('chartData', chartData);
        this.set('chartDomain', [start.time - margin, +end.time + margin]);
        this.set('highlightedItems', highlightedItems);
        this.set('analysisPoints', formatedPoints);
    },
    testSets: function ()
    {
        var analysisPoints = this.get('analysisPoints');
        if (!analysisPoints)
            return;
        var pointOptions = [{value: ' ', label: 'None'}]
            .concat(analysisPoints.map(function (point) { return {value: point.id, label: point.label}; }));
        return [
            Ember.Object.create({name: "A", options: pointOptions, selection: pointOptions[1]}),
            Ember.Object.create({name: "B", options: pointOptions, selection: pointOptions[pointOptions.length - 1]}),
        ];
    }.property('analysisPoints'),
    _rootChangedForTestSet: function ()
    {
        var sets = this.get('testSets');
        var roots = this.get('roots');
        if (!sets || !roots)
            return;

        sets.forEach(function (testSet, setIndex) {
            var currentSelection = testSet.get('selection');
            if (currentSelection == testSet.get('previousSelection'))
                return;
            testSet.set('previousSelection', currentSelection);
            var pointIndex = testSet.get('options').indexOf(currentSelection);

            roots.forEach(function (root) {
                var set = root.sets[setIndex];
                set.set('selection', set.revisions[pointIndex]);
            });
        });

    }.observes('testSets.@each.selection'),
    updateRoots: function ()
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

            self.set('roots', triggerable.get('acceptedRepositories').map(function (repository) {
                var repositoryId = repository.get('id');
                var revisions = [{value: ' ', label: 'None'}].concat(repositoryToRevisions[repositoryId]);
                return Ember.Object.create({
                    name: repository.get('name'),
                    sets: [
                        Ember.Object.create({name: 'A[' + repositoryId + ']',
                            revisions: revisions,
                            selection: revisions[1]}),
                        Ember.Object.create({name: 'B[' + repositoryId + ']',
                            revisions: revisions,
                            selection: revisions[revisions.length - 1]}),
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
            this.get('roots').map(function (root) {
                roots[root.get('name')] = root.get('sets').map(function (item) { return item.get('selection').value; });
            });
            App.TestGroup.create(this.get('model'), name, roots, repetitionCount).then(function () {
                
            });
        },
    },
});
