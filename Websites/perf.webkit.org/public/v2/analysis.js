App.AnalysisTask = App.NameLabelModel.extend({
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    platform: DS.belongsTo('platform'),
    metric: DS.belongsTo('metric'),
    startRun: DS.attr('number'),
    endRun: DS.attr('number'),
    bugs: DS.hasMany('bugs'),
    testGroups: function (key, value, oldValue) {
        return this.store.find('testGroup', {task: this.get('id')});
    }.property(),
    triggerable: function () {
        return this.store.find('triggerable', {task: this.get('id')}).then(function (triggerables) {
            return triggerables.objectAt(0);
        }, function () {
            return null;
        });
    }.property(),
    label: function () {
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
    _fetchTestResults: function ()
    {
        var task = this.get('task');
        if (!task)
            return null;
        var self = this;
        return App.Manifest.fetchRunsWithPlatformAndMetric(this.store,
            task.get('platform').get('id'), task.get('metric').get('id'), this.get('id')).then(
            function (result) { self.set('testResults', result.data); },
            function (error) {
                // FIXME: Somehow this never gets called.
                alert('Failed to fetch the results:' + error);
                return null;
            });
    }.observes('task', 'task.platform', 'task.metric').on('init'),
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
