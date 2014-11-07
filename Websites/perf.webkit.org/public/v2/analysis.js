App.AnalysisTask = App.NameLabelModel.extend({
    author: DS.attr('string'),
    createdAt: DS.attr('date'),
    platform: DS.belongsTo('platform'),
    metric: DS.belongsTo('metric'),
    startRun: DS.attr('number'),
    endRun: DS.attr('number'),
    testGroups: function () {
        return this.store.find('testGroup', {task: this.get('id')});
    }.property(),
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

App.BuildRequest = DS.Model.extend({
    group: DS.belongsTo('testGroup'),
    order: DS.attr('number'),
    rootSet: DS.attr('number'),
    build: DS.attr('number'),
    buildNumber: DS.attr('number'),
    buildBuilder: DS.belongsTo('builder'),
    buildTime: DS.attr('date'),
});
