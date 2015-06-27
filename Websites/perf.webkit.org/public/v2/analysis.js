App.AnalysisTask = App.NameLabelModel.extend({
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    formattedCreatedAt: function () {
        var format = d3.time.format("%Y-%m-%d");
        return format(this.get('createdAt'));
    }.property('createdAt'),
    platform: DS.belongsTo('platform'),
    metric: DS.belongsTo('metric'),
    startRun: DS.attr('number'),
    endRun: DS.attr('number'),
    bugs: DS.hasMany('bugs'),
    buildRequestCount: DS.attr('number'),
    finishedBuildRequestCount: DS.attr('number'),
    result: DS.attr('string'),
    needed: DS.attr('number'), // DS.attr('boolean') treats null as false.
    saveStatus: function ()
    {
        return PrivilegedAPI.sendRequest('update-analysis-task', {
            task: this.get('id'),
            result: this.get('result'),
            needed: this.get('needed'),
        });
    },
    statusLabel: function ()
    {
        var total = this.get('buildRequestCount');
        var finished = this.get('finishedBuildRequestCount');
        var result = this.get('result');
        if (result && total == finished)
            return result.capitalize();
        if (!total)
            return 'Empty';
        if (total != finished)
            return finished + ' out of ' + total;
        return 'Done';
    }.property('buildRequestCount', 'finishedBuildRequestCount'),
    testGroups: function (key, value, oldValue)
    {
        return this.store.find('testGroup', {task: this.get('id')});
    }.property(),
    triggerable: function ()
    {
        return this.store.find('triggerable', {task: this.get('id')}).then(function (triggerables) {
            return triggerables.objectAt(0);
        }, function (error) {
            console.log('Failed to fetch triggerables', error);
            return null;
        });
    }.property(),
    label: function ()
    {
        var label = this.get('name');
        var bugs = this.get('bugs').map(function (bug) { return bug.get('label'); }).join(' / ');
        return bugs ? label + ' (' + bugs + ')' : label;
    }.property('name', 'bugs'),
});

App.Bug = App.Model.extend({
    task: DS.belongsTo('AnalysisTask'),
    bugTracker: DS.belongsTo('BugTracker'),
    createdAt: DS.attr('date'),
    number: DS.attr('number'),
    url: function () {
        return this.get('bugTracker').urlFromBugNumber(this.get('number'));
    }.property('bugTracker.bugUrl', 'number'),
    label: function () {
        return this.get('bugTracker').get('label') + ': ' + this.get('number');
    }.property('name', 'bugTracker'),
});

// FIXME: Use DS.RESTAdapter instead.
App.AnalysisTask.create = function (name, startMeasurement, endMeasurement)
{
    return PrivilegedAPI.sendRequest('create-analysis-task', {
        name: name,
        startRun: startMeasurement.id(),
        endRun: endMeasurement.id(),
    });
}

App.AnalysisTaskAdapter = DS.RESTAdapter.extend({
    buildURL: function (type, id)
    {
        return '../api/analysis-tasks/' + (id ? id : '');
    },
});

App.BugAdapter = DS.RESTAdapter.extend({
    createRecord: function (store, type, record)
    {
        var param = {
            task: record.get('task').get('id'),
            bugTracker: record.get('bugTracker').get('id'),
            number: record.get('number'),
        };
        return PrivilegedAPI.sendRequest('associate-bug', param).then(function (data) {
            param['id'] = data['bugId'];
            return {'bug': param};
        });
    },
    deleteRecord: function (store, type, record)
    {
        return PrivilegedAPI.sendRequest('associate-bug', {bugToDelete: record.get('id')}).then(function () {
            return {};
        });
    }
});

App.Root = App.Model.extend({
    repository: DS.belongsTo('repository'),
    revision: DS.attr('string'),
});

App.RootSet = App.Model.extend({
    roots: DS.hasMany('roots'),
    revisionForRepository: function (repository)
    {
        var root = this.get('roots').findBy('repository', repository);
        if (!root)
            return null;
        return root.get('revision');
    }
});

App.TestGroup = App.NameLabelModel.extend({
    task: DS.belongsTo('analysisTask'),
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    buildRequests: DS.hasMany('buildRequests'),
    rootSets: DS.hasMany('rootSets'),
    platform: DS.belongsTo('platform'),
    _fetchTestResults: function ()
    {
        var task = this.get('task');
        var platform = this.get('platform') || (task ? task.get('platform') : null)
        if (!task || !platform)
            return null;
        var self = this;
        return App.Manifest.fetchRunsWithPlatformAndMetric(this.store,
            platform.get('id'), task.get('metric').get('id'), this.get('id')).then(
            function (result) { self.set('testResults', result.data); },
            function (error) {
                // FIXME: Somehow this never gets called.
                alert('Failed to fetch the results:' + error);
                return null;
            });
    }.observes('task', 'task.platform', 'task.metric', 'platform').on('init'),
});

App.TestGroup.create = function (analysisTask, name, rootSets, repetitionCount)
{
    var param = {
        task: analysisTask.get('id'),
        name: name,
        rootSets: rootSets,
        repetitionCount: repetitionCount,
    };
    return PrivilegedAPI.sendRequest('create-test-group', param).then(function (data) {
        analysisTask.set('testGroups'); // Refetch test groups.
        return analysisTask.store.find('testGroup', data['testGroupId']);
    });
}

App.TestGroupAdapter = DS.RESTAdapter.extend({
    buildURL: function (type, id)
    {
        return '../api/test-groups/' + (id ? id : '');
    },
});

App.Triggerable = App.NameLabelModel.extend({
    acceptedRepositories: DS.hasMany('repositories'),
});

App.TriggerableAdapter = DS.RESTAdapter.extend({
    buildURL: function (type, id)
    {
        return '../api/triggerables/' + (id ? id : '');
    },
});

App.AnalysisTaskSerializer = App.TestGroupSerializer = App.TriggerableSerializer = DS.RESTSerializer.extend({
    normalizePayload: function (payload)
    {
        delete payload['status'];
        return payload;
    }
});

App.BuildRequest = App.Model.extend({
    testGroup: DS.belongsTo('testGroup'),
    order: DS.attr('number'),
    orderLabel: function ()
    {
        return this.get('order') + 1;
    }.property('order'),
    rootSet: DS.belongsTo('rootSet'),
    status: DS.attr('string'),
    statusLabel: function ()
    {
        switch (this.get('status')) {
        case 'pending':
            return 'Waiting to be scheduled';
        case 'scheduled':
            return 'Scheduled';
        case 'running':
            return 'Running';
        case 'failed':
            return 'Failed';
        case 'completed':
            return 'Completed';
        }
    }.property('status'),
    url: DS.attr('string'),
    build: DS.attr('number'),
});

App.BuildRequest.aggregateStatuses = function (requests)
{
    var completeCount = 0;
    var failureCount = 0;
    requests.forEach(function (request) {
        switch (request.get('status')) {
        case 'failed':
            failureCount++;
            break;
        case 'completed':
            completeCount++;
            break;
        }
    });
    if (completeCount == requests.length)
        return 'Done';
    if (failureCount == requests.length)
        return 'All failed';
    var status = completeCount + ' out of ' + requests.length + ' completed';
    if (failureCount)
        status += ', ' + failureCount + ' failed';
    return status;
}
