App.AnalysisTask = App.NameLabelModel.extend({
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    platform: DS.belongsTo('platform'),
    metric: DS.belongsTo('metric'),
    startRun: DS.attr('number'),
    endRun: DS.attr('number'),
    bugs: DS.hasMany('bugs'),
    testGroups: function () {
        return this.store.find('testGroup', {task: this.get('id')});
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
});

App.TestGroupAdapter = DS.RESTAdapter.extend({
    buildURL: function (type, id)
    {
        return '../api/test-groups/' + (id ? id : '');
    },
});

App.AnalysisTaskSerializer = App.TestGroupSerializer = DS.RESTSerializer.extend({
    normalizePayload: function (payload)
    {
        delete payload['status'];
        return payload;
    }
});

App.BuildRequest = App.Model.extend({
    group: DS.belongsTo('testGroup'),
    order: DS.attr('number'),
    rootSet: DS.attr('number'),
    build: DS.attr('number'),
    buildNumber: DS.attr('number'),
    buildBuilder: DS.belongsTo('builder'),
    buildTime: DS.attr('date'),
});
