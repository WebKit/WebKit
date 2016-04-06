'use strict';

class BuildRequest extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        this._testGroupId = object.testGroupId;
        console.assert(!object.testGroup || object.testGroup instanceof TestGroup);
        this._testGroup = object.testGroup;
        if (this._testGroup)
            this._testGroup.addBuildRequest(this);
        console.assert(object.platform instanceof Platform);
        this._platform = object.platform;
        console.assert(object.test instanceof Test);
        this._test = object.test;
        this._order = object.order;
        console.assert(object.rootSet instanceof RootSet);
        this._rootSet = object.rootSet;
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
        this._result = null;
    }

    updateSingleton(object)
    {
        console.assert(this._testGroup == object.testGroup);
        console.assert(this._order == object.order);
        console.assert(this._rootSet == object.rootSet);
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
    }

    testGroupId() { return this._testGroupId; }
    testGroup() { return this._testGroup; }
    platform() { return this._platform; }
    test() { return this._test; }
    order() { return this._order; }
    rootSet() { return this._rootSet; }

    status() { return this._status; }
    hasFinished() { return this._status == 'failed' || this._status == 'completed' || this._status == 'canceled'; }
    hasStarted() { return this._status != 'pending'; }
    isScheduled() { return this._status == 'scheduled'; }
    isPending() { return this._status == 'pending'; }
    statusLabel()
    {
        switch (this._status) {
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
        case 'canceled':
            return 'Canceled';
        }
    }
    statusUrl() { return this._statusUrl; }

    buildId() { return this._buildId; }

    result() { return this._result; }
    setResult(result)
    {
        this._result = result;
        this._testGroup.didSetResult(this);
    }

    static fetchForTriggerable(triggerable)
    {
        return RemoteAPI.getJSONWithStatus('/api/build-requests/' + triggerable).then(function (data) {
            return BuildRequest.constructBuildRequestsFromData(data);
        });
    }

    static constructBuildRequestsFromData(data)
    {
        var rootIdMap = {};
        for (var root of data['roots']) {
            rootIdMap[root.id] = root;
            root.repository = Repository.findById(root.repository);
        }

        var rootSets = data['rootSets'].map(function (row) {
            row.roots = row.roots.map(function (rootId) { return rootIdMap[rootId]; });
            return RootSet.ensureSingleton(row.id, row);
        });

        return data['buildRequests'].map(function (rawData) {
            rawData.platform = Platform.findById(rawData.platform);
            rawData.test = Test.findById(rawData.test);
            rawData.testGroupId = rawData.testGroup;
            rawData.testGroup = TestGroup.findById(rawData.testGroup);
            rawData.rootSet = RootSet.findById(rawData.rootSet);
            return BuildRequest.ensureSingleton(rawData.id, rawData);
        });
    }
}

if (typeof module != 'undefined')
    module.exports.BuildRequest = BuildRequest;
