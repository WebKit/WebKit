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
        App.BuildPopup('choosePane', this)
            .then(function (platforms) { self.set('pickerData', platforms); });
    }.observes('platformId', 'metricId').on('init'),
    paneList: function () {
        return App.encodePrettifiedJSON([[this.get('platformId'), this.get('metricId'), null, null, false]]);
    }.property('platformId', 'metricId'),
});

App.IndexController = Ember.Controller.extend({
    queryParams: ['grid', 'numberOfDays'],
    _previousGrid: {},
    defaultTable: [],
    headerColumns: [],
    rows: [],
    numberOfDays: 30,
    editMode: false,

    gridChanged: function ()
    {
        var grid = this.get('grid');
        if (grid === this._previousGrid)
            return;

        var table = null;
        try {
            if (grid)
                table = JSON.parse(grid);
        } catch (error) {
            console.log("Failed to parse the grid:", error, grid);
        }

        if (!table || !table.length) // FIXME: We should fetch this from the manifest.
            table = this.get('defaultTable');

        table = this._normalizeTable(table);
        var columnCount = table[0].length;
        this.set('columnCount', columnCount);

        this.set('headerColumns', table[0].map(function (name, index) {
            return {label:name, index: index};
        }));

        this.set('rows', table.slice(1).map(function (rowParam) {
            return App.DashboardRow.create({
                header: rowParam[0],
                cellsInfo: rowParam.slice(1),
                columnCount: columnCount,
            })
        }));

        this.set('emptyRow', new Array(columnCount));
    }.observes('grid').on('init'),

    _normalizeTable: function (table)
    {
        var maxColumnCount = Math.max(table.map(function (column) { return column.length; }));
        for (var i = 1; i < table.length; i++) {
            var row = table[i];
            for (var j = 1; j < row.length; j++) {
                if (row[j] && !(row[j] instanceof Array)) {
                    console.log('Unrecognized (platform, metric) pair at column ' + i + ' row ' + j + ':' + row[j]);
                    row[j] = [];
                }
            }
        }
        return table;
    },

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
        App.Manifest.fetch();
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
        var repositoryName = repository.get('id');
        CommitLogs.fetchForTimeRange(repositoryName, null, null, keyword).then(function (commits) {
            if (self.isDestroyed || !self.get('chartData') || !commits.length)
                return;
            var currentRuns = self.get('chartData').current.timeSeriesByCommitTime().series();
            if (!currentRuns.length)
                return;

            var highlightedItems = {};
            var commitIndex = 0;
            for (var runIndex = 0; runIndex < currentRuns.length && commitIndex < commits.length; runIndex++) {
                var measurement = currentRuns[runIndex].measurement;
                var commitTime = measurement.commitTimeForRepository(repositoryName);
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

            App.Manifest.fetchRunsWithPlatformAndMetric(this.store, platformId, metricId).then(function (result) {
                self.set('platform', result.platform);
                self.set('metric', result.metric);
                self.set('chartData', result.runs);
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
        }
    }.observes('platformId', 'metricId').on('init'),
    _isValidId: function (id)
    {
        if (typeof(id) == "number")
            return true;
        if (typeof(id) == "string")
            return !!id.match(/^[A-Za-z0-9_]+$/);
        return false;
    }
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

    updateSharedDomain: function ()
    {
        var panes = this.get('panes');
        if (!panes.length)
            return;

        var union = [undefined, undefined];
        for (var i = 0; i < panes.length; i++) {
            var domain = panes[i].intrinsicDomain;
            if (!domain)
                continue;
            if (!union[0] || domain[0] < union[0])
                union[0] = domain[0];
            if (!union[1] || domain[1] > union[1])
                union[1] = domain[1];
        }
        if (union[0] === undefined)
            return;

        var startTime = this.get('startTime');
        var zoom = this.get('sharedZoom');
        if (startTime)
            union[0] = zoom ? Math.min(zoom[0], startTime) : startTime;

        this.set('sharedDomain', union);
    }.observes('panes.@each'),

    _startTimeChanged: function () {
        this.updateSharedDomain();
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
        Ember.run.debounce(this, '_updateQueryString', 500);
    }.observes('sharedZoom')
        .observes('panes.@each.platform', 'panes.@each.metric', 'panes.@each.selectedItem',
        'panes.@each.timeRange', 'panes.@each.timeRangeIsLocked'),

    _updateQueryString: function ()
    {
        this._currentEncodedPaneList = this._serializePaneList(this.get('panes'));
        this.set('paneList', this._currentEncodedPaneList);

        var zoom = undefined;
        var selection = this.get('sharedZoom');
        if (selection)
            zoom = (selection[0] - 0) + '-' + (selection[1] - 0);
        this.set('zoom', zoom);

        if (this.get('startTime') - this.defaultSince)
            this.set('since', this.get('startTime') - 0);
    },

    actions: {
        addPaneByMetricAndPlatform: function (param)
        {
            this.addPane(App.Pane.create({
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
        App.BuildPopup('addPaneByMetricAndPlatform').then(function (platforms) {
            self.set('platforms', platforms);
        });
    },
});

App.BuildPopup = function(action, position)
{
    return App.Manifest.fetch().then(function () {
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
            if (this.toggleProperty('showingBugsPane'))
                this.set('showingSearchPane', false);
        },
        associateBug: function (bugTracker, bugNumber)
        {
            var point = this.get('selectedSinglePoint');
            if (!point)
                return;
            var self = this;
            point.measurement.associateBug(bugTracker.get('id'), bugNumber).then(function () {
                self._updateBugs();
                self._updateMarkedPoints();
            }, function (error) {
                alert(error);
            });
        },
        createAnalysisTask: function ()
        {
            var name = this.get('newAnalysisTaskName');
            var points = this._selectedPoints;
            if (!name || !points || points.length < 2)
                return;

            var newWindow = window.open();
            App.AnalysisTask.create(name, points[0].measurement, points[points.length - 1].measurement).then(function (data) {
                // FIXME: Update the UI to show the new analysis task.
                var url = App.Router.router.generate('analysisTask', data['taskId']);
                newWindow.location.href = '#' + url;
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
                this.set('showingBugsPane', false);
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
        overviewDomainChanged: function (domain, intrinsicDomain)
        {
            this.set('overviewDomain', domain);
            this.set('intrinsicDomain', intrinsicDomain);
            this.get('parentController').updateSharedDomain();
        },
        rangeChanged: function (extent, points)
        {
            if (!points) {
                this._hasRange = false;
                this.set('details', null);
                this.set('timeRange', null);
                return;
            }
            this._hasRange = true;
            this._showDetails(points);
            this.set('timeRange', extent);
        },
    },
    _detailsChanged: function ()
    {
        this.set('showingBugsPane', false);
        this.set('selectedSinglePoint', !this._hasRange && this._selectedPoints ? this._selectedPoints[0] : null);
    }.observes('details'),
    _overviewSelectionChanged: function ()
    {
        var overviewSelection = this.get('overviewSelection');
        this.set('mainPlotDomain', overviewSelection);
        Ember.run.debounce(this, 'propagateZoom', 100);
    }.observes('overviewSelection'),
    _sharedDomainChanged: function ()
    {
        var newDomain = this.get('parentController').get('sharedDomain');
        if (newDomain == this.get('overviewDomain'))
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
        if (newSelection == this.get('mainPlotDomain'))
            return;
        this.set('overviewSelection', newSelection);
        this.set('mainPlotDomain', newSelection);
    }.observes('parentController.sharedZoom').on('init'),
    _currentItemChanged: function ()
    {
        if (this._hasRange)
            return;
        var point = this.get('currentItem');
        if (!point || !point.measurement)
            this.set('details', null);
        else {
            var previousPoint = point.series.previousPoint(point);
            this._showDetails(previousPoint ? [previousPoint, point] : [point]);
        }
    }.observes('currentItem'),
    _showDetails: function (points)
    {
        var isShowingEndPoint = !this._hasRange;
        var currentMeasurement = points[points.length - 1].measurement;
        var oldMeasurement = points[0].measurement;
        var formattedRevisions = currentMeasurement.formattedRevisions(oldMeasurement);
        var revisions = App.Manifest.get('repositories')
            .filter(function (repository) { return formattedRevisions[repository.get('id')]; })
            .map(function (repository) {
            var repositoryName = repository.get('id');
            var revision = Ember.Object.create(formattedRevisions[repositoryName]);
            revision['url'] = revision.previousRevision
                ? repository.urlForRevisionRange(revision.previousRevision, revision.currentRevision)
                : repository.urlForRevision(revision.currentRevision);
            revision['name'] = repositoryName;
            revision['repository'] = repository;
            return revision; 
        });

        var buildNumber = null;
        var buildURL = null;
        if (isShowingEndPoint) {
            buildNumber = currentMeasurement.buildNumber();
            var builder = App.Manifest.builder(currentMeasurement.builderId());
            if (builder)
                buildURL = builder.urlFromBuildNumber(buildNumber);
        }

        this._selectedPoints = points;
        this.set('details', Ember.Object.create({
            currentValue: currentMeasurement.mean().toFixed(2),
            oldValue: oldMeasurement && !isShowingEndPoint ? oldMeasurement.mean().toFixed(2) : null,
            buildNumber: buildNumber,
            buildURL: buildURL,
            buildTime: currentMeasurement.formattedBuildTime(),
            revisions: revisions,
        }));
        this._updateBugs();
    },
    _updateBugs: function ()
    {
        if (!this._selectedPoints)
            return;

        var bugTrackers = App.Manifest.get('bugTrackers');
        var trackerToBugNumbers = {};
        bugTrackers.forEach(function (tracker) { trackerToBugNumbers[tracker.get('id')] = new Array(); });

        var points = this._hasRange ? this._selectedPoints : [this._selectedPoints[1]];
        points.map(function (point) {
            var bugs = point.measurement.bugs();
            bugTrackers.forEach(function (tracker) {
                var bugNumber = bugs[tracker.get('id')];
                if (bugNumber)
                    trackerToBugNumbers[tracker.get('id')].push(bugNumber);
            });
        });

        this.set('details.bugTrackers', App.Manifest.get('bugTrackers').map(function (tracker) {
            var bugNumbers = trackerToBugNumbers[tracker.get('id')];
            return Ember.ObjectProxy.create({
                content: tracker,
                bugs: bugNumbers.map(function (bugNumber) {
                    return {
                        bugNumber: bugNumber,
                        bugUrl: bugNumber && tracker.get('bugUrl') ? tracker.get('bugUrl').replace(/\$number/g, bugNumber) : null
                    };
                }),
                editedBugNumber: this._hasRange ? null : bugNumbers[0],
            }); // FIXME: Create urls for new bugs.
        }));
    },
    _updateMarkedPoints: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData || !chartData.current) {
            this.set('markedPoints', {});
            return;
        }

        var series = chartData.current.timeSeriesByCommitTime().series();
        var markedPoints = {};
        for (var i = 0; i < series.length; i++) {
            var measurement = series[i].measurement;
            if (measurement.hasBugs())
                markedPoints[measurement.id()] = true;
        }
        this.set('markedPoints', markedPoints);
    }.observes('chartData'),
});

App.InteractiveChartComponent = Ember.Component.extend({
    chartData: null,
    showXAxis: true,
    showYAxis: true,
    interactive: false,
    enableSelection: true,
    classNames: ['chart'],
    init: function ()
    {
        this._super();
        this._needsConstruction = true;
        this._eventHandlers = [];
        $(window).resize(this._updateDimensionsIfNeeded.bind(this));
    },
    chartDataDidChange: function ()
    {
        var chartData = this.get('chartData');
        if (!chartData)
            return;
        this._needsConstruction = true;
        this._constructGraphIfPossible(chartData);
    }.observes('chartData').on('init'),
    didInsertElement: function ()
    {
        var chartData = this.get('chartData');
        if (chartData)
            this._constructGraphIfPossible(chartData);
    },
    willClearRender: function ()
    {
        this._eventHandlers.forEach(function (item) {
            $(item[0]).off(item[1], item[2]);
        })
    },
    _attachEventListener: function(target, eventName, listener)
    {
        this._eventHandlers.push([target, eventName, listener]);
        $(target).on(eventName, listener);
    },
    _constructGraphIfPossible: function (chartData)
    {
        if (!this._needsConstruction || !this.get('element'))
            return;

        var element = this.get('element');

        this._x = d3.time.scale();
        this._y = d3.scale.linear();

        // FIXME: Tear down the old SVG element.
        this._svgElement = d3.select(element).append("svg")
                .attr("width", "100%")
                .attr("height", "100%");

        var svg = this._svg = this._svgElement.append("g");

        var clipId = element.id + "-clip";
        this._clipPath = svg.append("defs").append("clipPath")
            .attr("id", clipId)
            .append("rect");

        if (this.get('showXAxis')) {
            this._xAxis = d3.svg.axis().scale(this._x).orient("bottom").ticks(6);
            this._xAxisLabels = svg.append("g")
                .attr("class", "x axis");
        }

        if (this.get('showYAxis')) {
            this._yAxis = d3.svg.axis().scale(this._y).orient("left").ticks(6).tickFormat(d3.format("s"));
            this._yAxisLabels = svg.append("g")
                .attr("class", "y axis");
        }

        this._clippedContainer = svg.append("g")
            .attr("clip-path", "url(#" + clipId + ")");

        var xScale = this._x;
        var yScale = this._y;
        this._timeLine = d3.svg.line()
            .x(function(point) { return xScale(point.time); })
            .y(function(point) { return yScale(point.value); });

        this._confidenceArea = d3.svg.area()
//            .interpolate("cardinal")
            .x(function(point) { return xScale(point.time); })
            .y0(function(point) { return point.interval ? yScale(point.interval[0]) : null; })
            .y1(function(point) { return point.interval ? yScale(point.interval[1]) : null; });

        if (this._paths)
            this._paths.forEach(function (path) { path.remove(); });
        this._paths = [];
        if (this._areas)
            this._areas.forEach(function (area) { area.remove(); });
        this._areas = [];
        if (this._dots)
            this._dots.forEach(function (dot) { dots.remove(); });
        this._dots = [];
        if (this._highlights)
            this._highlights.forEach(function (highlight) { _highlights.remove(); });
        this._highlights = [];

        this._currentTimeSeries = chartData.current.timeSeriesByCommitTime();
        this._currentTimeSeriesData = this._currentTimeSeries.series();
        this._baselineTimeSeries = chartData.baseline ? chartData.baseline.timeSeriesByCommitTime() : null;
        this._targetTimeSeries = chartData.target ? chartData.target.timeSeriesByCommitTime() : null;

        this._yAxisUnit = chartData.unit;

        var minMax = this._minMaxForAllTimeSeries();
        var smallEnoughValue = minMax[0] - (minMax[1] - minMax[0]) * 10;
        var largeEnoughValue = minMax[1] + (minMax[1] - minMax[0]) * 10;

        // FIXME: Flip the sides based on smallerIsBetter-ness.
        if (this._baselineTimeSeries) {
            var data = this._baselineTimeSeries.series();
            this._areas.push(this._clippedContainer
                .append("path")
                .datum(data.map(function (point) { return {time: point.time, value: point.value, interval: point.interval ? point.interval : [point.value, largeEnoughValue]}; }))
                .attr("class", "area baseline"));
        }
        if (this._targetTimeSeries) {
            var data = this._targetTimeSeries.series();
            this._areas.push(this._clippedContainer
                .append("path")
                .datum(data.map(function (point) { return {time: point.time, value: point.value, interval: point.interval ? point.interval : [smallEnoughValue, point.value]}; }))
                .attr("class", "area target"));
        }

        this._areas.push(this._clippedContainer
            .append("path")
            .datum(this._currentTimeSeriesData)
            .attr("class", "area"));

        this._paths.push(this._clippedContainer
            .append("path")
            .datum(this._currentTimeSeriesData)
            .attr("class", "commit-time-line"));

        this._dots.push(this._clippedContainer
            .selectAll(".dot")
                .data(this._currentTimeSeriesData)
            .enter().append("circle")
                .attr("class", "dot")
                .attr("r", this.get('chartPointRadius') || 1));

        if (this.get('interactive')) {
            this._attachEventListener(element, "mousemove", this._mouseMoved.bind(this));
            this._attachEventListener(element, "mouseleave", this._mouseLeft.bind(this));
            this._attachEventListener(element, "mousedown", this._mouseDown.bind(this));
            this._attachEventListener($(element).parents("[tabindex]"), "keydown", this._keyPressed.bind(this));

            this._currentItemLine = this._clippedContainer
                .append("line")
                .attr("class", "current-item");

            this._currentItemCircle = this._clippedContainer
                .append("circle")
                .attr("class", "dot current-item")
                .attr("r", 3);
        }

        this._brush = null;
        if (this.get('enableSelection')) {
            this._brush = d3.svg.brush()
                .x(this._x)
                .on("brush", this._brushChanged.bind(this));

            this._brushRect = this._clippedContainer
                .append("g")
                .attr("class", "x brush");
        }

        this._updateDomain();
        this._updateDimensionsIfNeeded();

        // Work around the fact the brush isn't set if we updated it synchronously here.
        setTimeout(this._selectionChanged.bind(this), 0);

        setTimeout(this._selectedItemChanged.bind(this), 0);

        this._needsConstruction = false;
    },
    _updateDomain: function ()
    {
        var xDomain = this.get('domain');
        var intrinsicXDomain = this._computeXAxisDomain(this._currentTimeSeriesData);
        if (!xDomain)
            xDomain = intrinsicXDomain;
        var currentDomain = this._x.domain();
        if (currentDomain && this._xDomainsAreSame(currentDomain, xDomain))
            return currentDomain;

        var yDomain = this._computeYAxisDomain(xDomain[0], xDomain[1]);
        this._x.domain(xDomain);
        this._y.domain(yDomain);
        this.sendAction('domainChanged', xDomain, intrinsicXDomain);
        return xDomain;
    },
    _updateDimensionsIfNeeded: function (newSelection)
    {
        var element = $(this.get('element'));

        var newTotalWidth = element.width();
        var newTotalHeight = element.height();
        if (this._totalWidth == newTotalWidth && this._totalHeight == newTotalHeight)
            return;

        this._totalWidth = newTotalWidth;
        this._totalHeight = newTotalHeight;

        if (!this._rem)
            this._rem = parseFloat(getComputedStyle(document.documentElement).fontSize);
        var rem = this._rem;

        var padding = 0.5 * rem;
        var margin = {top: padding, right: padding, bottom: padding, left: padding};
        if (this._xAxis)
            margin.bottom += rem;
        if (this._yAxis)
            margin.left += 3 * rem;

        this._margin = margin;
        this._contentWidth = this._totalWidth - margin.left - margin.right;
        this._contentHeight = this._totalHeight - margin.top - margin.bottom;

        this._svg.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

        this._clipPath
            .attr("width", this._contentWidth)
            .attr("height", this._contentHeight);

        this._x.range([0, this._contentWidth]);
        this._y.range([this._contentHeight, 0]);

        if (this._xAxis) {
            this._xAxis.tickSize(-this._contentHeight);
            this._xAxisLabels.attr("transform", "translate(0," + this._contentHeight + ")");
        }

        if (this._yAxis)
            this._yAxis.tickSize(-this._contentWidth);

        if (this._currentItemLine) {
            this._currentItemLine
                .attr("y1", 0)
                .attr("y2", margin.top + this._contentHeight);
        }

        this._relayoutDataAndAxes(this._currentSelection());
    },
    _updateBrush: function ()
    {
        this._brushRect
            .call(this._brush)
        .selectAll("rect")
            .attr("y", 1)
            .attr("height", this._contentHeight - 2);
        this._updateSelectionToolbar();
    },
    _relayoutDataAndAxes: function (selection)
    {
        var timeline = this._timeLine;
        this._paths.forEach(function (path) { path.attr("d", timeline); });

        var confidenceArea = this._confidenceArea;
        this._areas.forEach(function (path) { path.attr("d", confidenceArea); });

        var xScale = this._x;
        var yScale = this._y;
        this._dots.forEach(function (dot) {
            dot
                .attr("cx", function(measurement) { return xScale(measurement.time); })
                .attr("cy", function(measurement) { return yScale(measurement.value); });
        });
        this._updateMarkedDots();
        this._updateHighlightPositions();

        if (this._brush) {
            if (selection)
                this._brush.extent(selection);
            else
                this._brush.clear();
            this._updateBrush();
        }

        this._updateCurrentItemIndicators();

        if (this._xAxis)
            this._xAxisLabels.call(this._xAxis);
        if (!this._yAxis)
            return;

        this._yAxisLabels.call(this._yAxis);
        if (this._yAxisUnitContainer)
            this._yAxisUnitContainer.remove();
        this._yAxisUnitContainer = this._yAxisLabels.append("text")
            .attr("x", 0.5 * this._rem)
            .attr("y", this._rem)
            .attr("dy", '0.8rem')
            .style("text-anchor", "start")
            .style("z-index", "100")
            .text(this._yAxisUnit);
    },
    _updateMarkedDots: function () {
        var markedPoints = this.get('markedPoints') || {};
        var defaultDotRadius = this.get('chartPointRadius') || 1;
        this._dots.forEach(function (dot) {
            dot.classed('marked', function (point) { return markedPoints[point.measurement.id()]; });
            dot.attr('r', function (point) {
                return markedPoints[point.measurement.id()] ? defaultDotRadius * 1.5 : defaultDotRadius; });
        });
    }.observes('markedPoints'),
    _updateHighlightPositions: function () {
        var xScale = this._x;
        var yScale = this._y;
        var y2 = this._margin.top + this._contentHeight;
        this._highlights.forEach(function (highlight) {
            highlight
                .attr("y1", 0)
                .attr("y2", y2)
                .attr("y", function(measurement) { return yScale(measurement.value); })
                .attr("x1", function(measurement) { return xScale(measurement.time); })
                .attr("x2", function(measurement) { return xScale(measurement.time); });
        });
    },
    _computeXAxisDomain: function (timeSeries)
    {
        var extent = d3.extent(timeSeries, function(point) { return point.time; });
        var margin = 3600 * 1000; // Use x.inverse to compute the right amount from a margin.
        return [+extent[0] - margin, +extent[1] + margin];
    },
    _computeYAxisDomain: function (startTime, endTime)
    {
        var range = this._minMaxForAllTimeSeries(startTime, endTime);
        var min = range[0];
        var max = range[1];
        if (max < min)
            min = max = 0;
        var diff = max - min;
        var margin = diff * 0.05;

        yExtent = [min - margin, max + margin];
//        if (yMin !== undefined)
//            yExtent[0] = parseInt(yMin);
        return yExtent;
    },
    _minMaxForAllTimeSeries: function (startTime, endTime)
    {
        var currentRange = this._currentTimeSeries.minMaxForTimeRange(startTime, endTime);
        var baselineRange = this._baselineTimeSeries ? this._baselineTimeSeries.minMaxForTimeRange(startTime, endTime) : [Number.MAX_VALUE, Number.MIN_VALUE];
        var targetRange = this._targetTimeSeries ? this._targetTimeSeries.minMaxForTimeRange(startTime, endTime) : [Number.MAX_VALUE, Number.MIN_VALUE];
        return [
            Math.min(currentRange[0], baselineRange[0], targetRange[0]),
            Math.max(currentRange[1], baselineRange[1], targetRange[1]),
        ];
    },
    _currentSelection: function ()
    {
        return this._brush && !this._brush.empty() ? this._brush.extent() : null;
    },
    _domainChanged: function ()
    {
        var selection = this._currentSelection() || this.get('sharedSelection');
        var newXDomain = this._updateDomain();

        if (selection && newXDomain && selection[0] <= newXDomain[0] && newXDomain[1] <= selection[1])
            selection = null; // Otherwise the user has no way of clearing the selection.

        this._relayoutDataAndAxes(selection);
    }.observes('domain'),
    _selectionChanged: function ()
    {
        this._updateSelection(this.get('selection'));
    }.observes('selection'),
    _sharedSelectionChanged: function ()
    {
        if (this.get('selectionIsLocked'))
            return;
        this._updateSelection(this.get('sharedSelection'));
    }.observes('sharedSelection'),
    _updateSelection: function (newSelection)
    {
        if (!this._brush)
            return;

        var currentSelection = this._currentSelection();
        if (newSelection && currentSelection && this._xDomainsAreSame(newSelection, currentSelection))
            return;

        var domain = this._x.domain();
        if (!newSelection || this._xDomainsAreSame(domain, newSelection))
            this._brush.clear();
        else
            this._brush.extent(newSelection);
        this._updateBrush();

        this._setCurrentSelection(newSelection);
    },
    _xDomainsAreSame: function (domain1, domain2)
    {
        return !(domain1[0] - domain2[0]) && !(domain1[1] - domain2[1]);
    },
    _brushChanged: function ()
    {
        if (this._brush.empty()) {
            if (!this._brushExtent)
                return;

            this.set('selectionIsLocked', false);
            this._setCurrentSelection(undefined);

            // Avoid locking the indicator in _mouseDown when the brush was cleared in the same mousedown event.
            this._brushJustChanged = true;
            var self = this;
            setTimeout(function () {
                self._brushJustChanged = false;
            }, 0);

            return;
        }

        this.set('selectionIsLocked', true);
        this._setCurrentSelection(this._brush.extent());
    },
    _keyPressed: function (event)
    {
        if (!this._currentItemIndex || this._currentSelection())
            return;

        var newIndex;
        switch (event.keyCode) {
        case 37: // Left
            newIndex = this._currentItemIndex - 1;
            break;
        case 39: // Right
            newIndex = this._currentItemIndex + 1;
            break;
        case 38: // Up
        case 40: // Down
        default:
            return;
        }

        // Unlike mousemove, keydown shouldn't move off the edge.
        if (this._currentTimeSeriesData[newIndex])
            this._setCurrentItem(newIndex);
    },
    _mouseMoved: function (event)
    {
        if (!this._margin || this._currentSelection() || this._currentItemLocked)
            return;

        var point = this._mousePointInGraph(event);

        this._selectClosestPointToMouseAsCurrentItem(point);
    },
    _mouseLeft: function (event)
    {
        if (!this._margin || this._currentItemLocked)
            return;

        this._selectClosestPointToMouseAsCurrentItem(null);
    },
    _mouseDown: function (event)
    {
        if (!this._margin || this._currentSelection() || this._brushJustChanged)
            return;

        var point = this._mousePointInGraph(event);
        if (!point)
            return;

        if (this._currentItemLocked) {
            this._currentItemLocked = false;
            this.set('selectedItem', null);
            return;
        }

        this._currentItemLocked = true;
        this._selectClosestPointToMouseAsCurrentItem(point);
    },
    _mousePointInGraph: function (event)
    {
        var offset = $(this.get('element')).offset();
        if (!offset)
            return null;

        var point = {
            x: event.pageX - offset.left - this._margin.left,
            y: event.pageY - offset.top - this._margin.top
        };

        var xScale = this._x;
        var yScale = this._y;
        var xDomain = xScale.domain();
        var yDomain = yScale.domain();
        if (point.x >= xScale(xDomain[0]) && point.x <= xScale(xDomain[1])
            && point.y <= yScale(yDomain[0]) && point.y >= yScale(yDomain[1]))
            return point;

        return null;
    },
    _selectClosestPointToMouseAsCurrentItem: function (point)
    {
        var xScale = this._x;
        var yScale = this._y;
        var distanceHeuristics = function (m) {
            var mX = xScale(m.time);
            var mY = yScale(m.value);
            var xDiff = mX - point.x;
            var yDiff = mY - point.y;
            return xDiff * xDiff + yDiff * yDiff / 16; // Favor horizontal affinity.
        };
        distanceHeuristics = function (m) {
            return Math.abs(xScale(m.time) - point.x);
        }

        var newItemIndex;
        if (point && !this._currentSelection()) {
            var distances = this._currentTimeSeriesData.map(distanceHeuristics);
            var minDistance = Number.MAX_VALUE;
            for (var i = 0; i < distances.length; i++) {
                if (distances[i] < minDistance) {
                    newItemIndex = i;
                    minDistance = distances[i];
                }
            }
        }

        this._setCurrentItem(newItemIndex);
        this._updateSelectionToolbar();
    },
    _currentTimeChanged: function ()
    {
        if (!this._margin || this._currentSelection() || this._currentItemLocked)
            return

        var currentTime = this.get('currentTime');
        if (currentTime) {
            for (var i = 0; i < this._currentTimeSeriesData.length; i++) {
                var point = this._currentTimeSeriesData[i];
                if (point.time >= currentTime) {
                    this._setCurrentItem(i, /* doNotNotify */ true);
                    return;
                }
            }
        }
        this._setCurrentItem(undefined, /* doNotNotify */ true);
    }.observes('currentTime'),
    _setCurrentItem: function (newItemIndex, doNotNotify)
    {
        if (newItemIndex === this._currentItemIndex) {
            if (this._currentItemLocked)
                this.set('selectedItem', this.get('currentItem') ? this.get('currentItem').measurement.id() : null);
            return;
        }

        var newItem = this._currentTimeSeriesData[newItemIndex];
        this._brushExtent = undefined;
        this._currentItemIndex = newItemIndex;

        if (!newItem) {
            this._currentItemLocked = false;
            this.set('selectedItem', null);
        }

        this._updateCurrentItemIndicators();

        if (!doNotNotify)
            this.set('currentTime', newItem ? newItem.time : undefined);

        this.set('currentItem', newItem);
        if (this._currentItemLocked)
            this.set('selectedItem', newItem ? newItem.measurement.id() : null);
    },
    _selectedItemChanged: function ()
    {
        if (!this._margin)
            return;

        var selectedId = this.get('selectedItem');
        var currentItem = this.get('currentItem');
        if (currentItem && currentItem.measurement.id() == selectedId)
            return;

        var series = this._currentTimeSeriesData;
        var selectedItemIndex = undefined;
        for (var i = 0; i < series.length; i++) {
            if (series[i].measurement.id() == selectedId) {
                this._updateSelection(null);
                this._currentItemLocked = true;
                this._setCurrentItem(i);
                this._updateSelectionToolbar();
                return;
            }
        }
    }.observes('selectedItem').on('init'),
    _highlightedItemsChanged: function () {
        if (!this._margin)
            return;

        var highlightedItems = this.get('highlightedItems');

        var data = this._currentTimeSeriesData.filter(function (item) { return highlightedItems[item.measurement.id()]; });

        if (this._highlights)
            this._highlights.forEach(function (highlight) { highlight.remove(); });

        this._highlights.push(this._clippedContainer
            .selectAll(".highlight")
                .data(data)
            .enter().append("line")
                .attr("class", "highlight"));

        this._updateHighlightPositions();

    }.observes('highlightedItems'),
    _updateCurrentItemIndicators: function ()
    {
        if (!this._currentItemLine)
            return;

        var item = this._currentTimeSeriesData[this._currentItemIndex];
        if (!item) {
            this._currentItemLine.attr("x1", -1000).attr("x2", -1000);
            this._currentItemCircle.attr("cx", -1000);
            return;
        }

        var x = this._x(item.time);
        var y = this._y(item.value);

        this._currentItemLine
            .attr("x1", x)
            .attr("x2", x);

        this._currentItemCircle
            .attr("cx", x)
            .attr("cy", y);
    },
    _setCurrentSelection: function (newSelection)
    {
        if (this._brushExtent === newSelection)
            return;

        var points = null;
        if (newSelection) {
            points = this._currentTimeSeriesData
                .filter(function (point) { return point.time >= newSelection[0] && point.time <= newSelection[1]; });
            if (!points.length)
                points = null;
        }

        this._brushExtent = newSelection;
        this._setCurrentItem(undefined);
        this._updateSelectionToolbar();

        this.set('sharedSelection', newSelection);
        this.sendAction('selectionChanged', newSelection, points);
    },
    _updateSelectionToolbar: function ()
    {
        if (!this.get('interactive'))
            return;

        var selection = this._currentSelection();
        var selectionToolbar = $(this.get('element')).children('.selection-toolbar');
        if (selection) {
            var left = this._x(selection[0]);
            var right = this._x(selection[1]);
            selectionToolbar
                .css({left: this._margin.left + right, top: this._margin.top + this._contentHeight})
                .show();
        } else
            selectionToolbar.hide();
    },
    actions: {
        zoom: function ()
        {
            this.sendAction('zoom', this._currentSelection());
            this.set('selection', null);
        },
    },
});



App.CommitsViewerComponent = Ember.Component.extend({
    repository: null,
    revisionInfo: null,
    commits: null,
    commitsChanged: function ()
    {
        var revisionInfo = this.get('revisionInfo');

        var to = revisionInfo.get('currentRevision');
        var from = revisionInfo.get('previousRevision');
        var repository = this.get('repository');
        if (!from || !repository || !repository.get('hasReportedCommits'))
            return;

        var self = this;
        CommitLogs.fetchForTimeRange(repository.get('id'), from, to).then(function (commits) {
            if (self.isDestroyed)
                return;
            self.set('commits', commits.map(function (commit) {
                return Ember.Object.create({
                    repository: repository,
                    revision: commit.revision,
                    url: repository.urlForRevision(commit.revision),
                    author: commit.authorName || commit.authorEmail,
                    message: commit.message ? commit.message.substr(0, 75) : null,
                });
            }));
        }, function () {
            if (!self.isDestroyed)
                self.set('commits', []);
        })
    }.observes('repository').observes('revisionInfo').on('init'),
});


App.AnalysisRoute = Ember.Route.extend({
    model: function () {
        return this.store.findAll('analysisTask').then(function (tasks) {
            return Ember.Object.create({'tasks': tasks});
        });
    },
});

App.AnalysisTaskRoute = Ember.Route.extend({
    model: function (param) {
        var store = this.store;
        return this.store.find('analysisTask', param.taskId).then(function (task) {
            return App.AnalysisTaskViewModel.create({content: task});
        });
    },
});

App.AnalysisTaskViewModel = Ember.ObjectProxy.extend({
    testSets: [],
    roots: [],
    _taskUpdated: function ()
    {
        var platformId = this.get('platform').get('id');
        var metricId = this.get('metric').get('id');
        App.Manifest.fetchRunsWithPlatformAndMetric(this.store, platformId, metricId).then(this._fetchedRuns.bind(this));
    }.observes('platform', 'metric').on('init'),
    _fetchedRuns: function (data) {
        var runs = data.runs;

        var currentTimeSeries = runs.current.timeSeriesByCommitTime();
        if (!currentTimeSeries)
            return; // FIXME: Report an error.

        var start = currentTimeSeries.findPointByMeasurementId(this.get('startRun'));
        var end = currentTimeSeries.findPointByMeasurementId(this.get('endRun'));
        if (!start || !end)
            return; // FIXME: Report an error.

        var markedPoints = {};
        markedPoints[start.measurement.id()] = true;
        markedPoints[end.measurement.id()] = true;

        var formatedPoints = currentTimeSeries.seriesBetweenPoints(start, end).map(function (point, index) {
            return {
                id: point.measurement.id(),
                measurement: point.measurement,
                label: 'Point ' + (index + 1),
                value: point.value + (runs.unit ? ' ' + runs.unit : ''),
            };
        });

        var margin = (end.time - start.time) * 0.1;
        this.set('chartData', runs);
        this.set('chartDomain', [start.time - margin, +end.time + margin]);
        this.set('markedPoints', markedPoints);
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
    _rootChangedForTestSet: function () {
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
    _updateRoots: function ()
    {
        var analysisPoints = this.get('analysisPoints');
        if (!analysisPoints)
            return [];
        var repositoryToRevisions = {};
        analysisPoints.forEach(function (point, pointIndex) {
            var revisions = point.measurement.formattedRevisions();
            for (var repositoryName in revisions) {
                if (!repositoryToRevisions[repositoryName])
                    repositoryToRevisions[repositoryName] = new Array(analysisPoints.length);
                var revision = revisions[repositoryName];
                repositoryToRevisions[repositoryName][pointIndex] = {
                    label: point.label + ': ' + revision.label,
                    value: revision.currentRevision,
                };
            }
        });

        var roots = [];
        for (var repositoryName in repositoryToRevisions) {
            var revisions = [{value: ' ', label: 'None'}].concat(repositoryToRevisions[repositoryName]);
            roots.push(Ember.Object.create({
                name: repositoryName,
                sets: [
                    Ember.Object.create({name: 'A[' + repositoryName + ']',
                        revisions: revisions,
                        selection: revisions[1]}),
                    Ember.Object.create({name: 'B[' + repositoryName + ']',
                        revisions: revisions,
                        selection: revisions[revisions.length - 1]}),
                ],
            }));
        }
        return rooots;
    }.property('analysisPoints'),
});
