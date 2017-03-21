'use strict';

class BuildRequest extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        this._triggerable = object.triggerable;
        this._analysisTaskId = object.task;
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
        console.assert(object.commitSet instanceof CommitSet);
        this._commitSet = object.commitSet;
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
        this._createdAt = new Date(object.createdAt);
        this._result = null;
    }

    updateSingleton(object)
    {
        console.assert(this._testGroup == object.testGroup);
        console.assert(this._order == object.order);
        console.assert(this._commitSet == object.commitSet);
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
    }

    analysisTaskId() { return this._analysisTaskId; }
    testGroupId() { return this._testGroupId; }
    testGroup() { return this._testGroup; }
    platform() { return this._platform; }
    test() { return this._test; }
    order() { return +this._order; }
    commitSet() { return this._commitSet; }

    status() { return this._status; }
    hasFinished() { return this._status == 'failed' || this._status == 'completed' || this._status == 'canceled'; }
    hasStarted() { return this._status != 'pending'; }
    isScheduled() { return this._status == 'scheduled'; }
    isPending() { return this._status == 'pending'; }
    statusLabel()
    {
        switch (this._status) {
        case 'pending':
            return 'Waiting';
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
    createdAt() { return this._createdAt; }

    waitingTime(referenceTime)
    {
        var units = [
            {unit: 'week', length: 7 * 24 * 3600},
            {unit: 'day', length: 24 * 3600},
            {unit: 'hour', length: 3600},
            {unit: 'minute', length: 60},
        ];

        var diff = (referenceTime - this.createdAt()) / 1000;

        var indexOfFirstSmallEnoughUnit = units.length - 1;
        for (var i = 0; i < units.length; i++) {
            if (diff > 1.5 * units[i].length) {
                indexOfFirstSmallEnoughUnit = i;
                break;
            }
        }

        var label = '';
        var lastUnit = false;
        for (var i = indexOfFirstSmallEnoughUnit; !lastUnit; i++) {
            lastUnit = i == indexOfFirstSmallEnoughUnit + 1 || i == units.length - 1;
            var length = units[i].length;
            var valueForUnit = lastUnit ? Math.round(diff / length) : Math.floor(diff / length);

            var unit = units[i].unit + (valueForUnit == 1 ? '' : 's');
            if (label)
                label += ' ';
            label += `${valueForUnit} ${unit}`;

            diff = diff - valueForUnit * length;
        }

        return label;
    }

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
        const commitIdMap = {};
        for (let commit of data['commits']) {
            commitIdMap[commit.id] = commit;
            commit.repository = Repository.findById(commit.repository);
        }

        const commitSets = data['commitSets'].map((row) => {
            row.commits = row.commits.map((commitId) => commitIdMap[commitId]);
            return CommitSet.ensureSingleton(row.id, row);
        });

        return data['buildRequests'].map(function (rawData) {
            rawData.platform = Platform.findById(rawData.platform);
            rawData.test = Test.findById(rawData.test);
            rawData.testGroupId = rawData.testGroup;
            rawData.testGroup = TestGroup.findById(rawData.testGroup);
            rawData.commitSet = CommitSet.findById(rawData.commitSet);
            return BuildRequest.ensureSingleton(rawData.id, rawData);
        });
    }
}

if (typeof module != 'undefined')
    module.exports.BuildRequest = BuildRequest;
