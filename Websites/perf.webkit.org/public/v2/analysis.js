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

App.TestGroup = App.NameLabelModel.extend({
    analysisTask: DS.belongsTo('analysisTask'),
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    buildRequests: DS.hasMany('buildRequests'),
    rootSets: function ()
    {
        var rootSetIds = [];
        this.get('buildRequests').forEach(function (request) {
            var rootSet = request.get('rootSet');
            if (!rootSetIds.contains(rootSet))
                rootSetIds.push(rootSet);
        });
        return rootSetIds;
    }.property('buildRequests'),
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
    rootSet: DS.attr('number'),
    config: function ()
    {
        var rootSets = this.get('testGroup').get('rootSets');
        var index = rootSets.indexOf(this.get('rootSet'));
        return String.fromCharCode('A'.charCodeAt(0) + index);
    }.property('testGroup', 'testGroup.rootSets'),
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
        case 'completed':
            return 'Finished';
        }
    }.property('status'),
    build: DS.attr('number'),
});
