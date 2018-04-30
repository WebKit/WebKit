'use strict';

class TestGroup extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._taskId = object.task;
        this._authorName = object.author;
        this._createdAt = new Date(object.createdAt);
        this._isHidden = object.hidden;
        this._buildRequests = [];
        this._orderBuildRequestsLazily = new LazilyEvaluatedFunction((...buildRequests) => {
            return buildRequests.sort((a, b) => a.order() - b.order());
        });
        this._repositories = null;
        this._computeRequestedCommitSetsLazily = new LazilyEvaluatedFunction(TestGroup._computeRequestedCommitSets);
        this._requestedCommitSets = null;
        this._commitSetToLabel = new Map;
        console.assert(!object.platform || object.platform instanceof Platform);
        this._platform = object.platform;
    }

    updateSingleton(object)
    {
        super.updateSingleton(object);

        console.assert(this._taskId == object.task);
        console.assert(+this._createdAt == +object.createdAt);
        console.assert(this._platform == object.platform);

        this._isHidden = object.hidden;
    }

    task() { return AnalysisTask.findById(this._taskId); }
    createdAt() { return this._createdAt; }
    isHidden() { return this._isHidden; }
    buildRequests() { return this._buildRequests; }
    addBuildRequest(request)
    {
        this._buildRequests.push(request);
        this._requestedCommitSets = null;
        this._commitSetToLabel.clear();
    }

    test()
    {
        const request = this._lastRequest();
        return request ? request.test() : null;
    }

    platform() { return this._platform; }

    _lastRequest()
    {
        const requests = this._orderedBuildRequests();
        return requests.length ? requests[requests.length - 1] : null;
    }

    _orderedBuildRequests()
    {
        return this._orderBuildRequestsLazily.evaluate(...this._buildRequests);
    }

    repetitionCount()
    {
        if (!this._buildRequests.length)
            return 0;
        const commitSet = this._buildRequests[0].commitSet();
        let count = 0;
        for (const request of this._buildRequests) {
            if (request.isTest() && request.commitSet() == commitSet)
                count++;
        }
        return count;
    }

    requestedCommitSets()
    {
        return this._computeRequestedCommitSetsLazily.evaluate(...this._orderedBuildRequests());
    }

    static _computeRequestedCommitSets(...orderedBuildRequests)
    {
        const requestedCommitSets = [];
        const commitSetLabelMap = new Map;
        for (const request of orderedBuildRequests) {
            const set = request.commitSet();
            if (!requestedCommitSets.includes(set))
                requestedCommitSets.push(set);
        }
        return requestedCommitSets;
    }

    requestsForCommitSet(commitSet)
    {
        return this._orderedBuildRequests().filter((request) => request.commitSet() == commitSet);
    }

    labelForCommitSet(commitSet)
    {
        const requestedSets = this.requestedCommitSets();
        const setIndex = requestedSets.indexOf(commitSet);
        if (setIndex < 0)
            return null;
        return String.fromCharCode('A'.charCodeAt(0) + setIndex);
    }

    hasFinished()
    {
        return this._buildRequests.every(function (request) { return request.hasFinished(); });
    }

    hasStarted()
    {
        return this._buildRequests.some(function (request) { return request.hasStarted(); });
    }

    hasPending()
    {
        return this._buildRequests.some(function (request) { return request.isPending(); });
    }

    compareTestResults(metric, beforeValues, afterValues)
    {
        console.assert(metric);
        const beforeMean = Statistics.sum(beforeValues) / beforeValues.length;
        const afterMean = Statistics.sum(afterValues) / afterValues.length;

        var result = {changeType: null, status: 'failed', label: 'Failed', fullLabel: 'Failed', isStatisticallySignificant: false};

        var hasCompleted = this.hasFinished();
        if (!hasCompleted) {
            if (this.hasStarted()) {
                result.status = 'running';
                result.label = 'Running';
                result.fullLabel = 'Running';
            } else {
                console.assert(result.changeType === null);
                result.status = 'pending';
                result.label = 'Pending';
                result.fullLabel = 'Pending';
            }
        }

        if (beforeValues.length && afterValues.length) {
            const summary = metric.labelForDifference(beforeMean, afterMean, 'better', 'worse');
            result.changeType = summary.changeType;
            result.label = summary.changeLabel;
            var isSignificant = Statistics.testWelchsT(beforeValues, afterValues);
            var significanceLabel = isSignificant ? 'significant' : 'insignificant';

            if (hasCompleted)
                result.status = isSignificant ? result.changeType : 'unchanged';
            result.fullLabel = `${result.label} (statistically ${significanceLabel})`;
            result.isStatisticallySignificant = isSignificant;
        }

        return result;
    }

    updateName(newName)
    {
        var self = this;
        var id = this.id();
        return PrivilegedAPI.sendRequest('update-test-group', {
            group: id,
            name: newName,
        }).then(function (data) {
            return TestGroup.cachedFetch(`/api/test-groups/${id}`, {}, true)
                .then(TestGroup._createModelsFromFetchedTestGroups.bind(TestGroup));
        });
    }

    updateHiddenFlag(hidden)
    {
        var self = this;
        var id = this.id();
        return PrivilegedAPI.sendRequest('update-test-group', {
            group: id,
            hidden: !!hidden,
        }).then(function (data) {
            return TestGroup.cachedFetch(`/api/test-groups/${id}`, {}, true)
                .then(TestGroup._createModelsFromFetchedTestGroups.bind(TestGroup));
        });
    }

    static createWithTask(taskName, platform, test, groupName, repetitionCount, commitSets)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const params = {taskName, name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets};
        return PrivilegedAPI.sendRequest('create-test-group', params).then((data) => {
            return AnalysisTask.fetchById(data['taskId'], true);
        }).then((task) => {
            return this.fetchForTask(task.id()).then(() => task);
        });
    }

    static createWithCustomConfiguration(task, platform, test, groupName, repetitionCount, commitSets)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const params = {task: task.id(), name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets};
        return PrivilegedAPI.sendRequest('create-test-group', params).then((data) => {
            return this.fetchForTask(data['taskId'], true);
        });
    }

    static createAndRefetchTestGroups(task, name, repetitionCount, commitSets)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        return PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(),
            name: name,
            repetitionCount: repetitionCount,
            revisionSets: revisionSets,
        }).then((data) => this.fetchForTask(data['taskId'], true));
    }

    static findAllByTask(taskId)
    {
        return TestGroup.all().filter((testGroup) => testGroup._taskId == taskId);
    }

    static fetchForTask(taskId, ignoreCache = false)
    {
        return this.cachedFetch('/api/test-groups', {task: taskId}, ignoreCache).then(this._createModelsFromFetchedTestGroups.bind(this));
    }

    static _createModelsFromFetchedTestGroups(data)
    {
        var testGroups = data['testGroups'].map(function (row) {
            row.platform = Platform.findById(row.platform);
            return TestGroup.ensureSingleton(row.id, row);
        });

        BuildRequest.constructBuildRequestsFromData(data);

        return testGroups;
    }
}

if (typeof module != 'undefined')
    module.exports.TestGroup = TestGroup;
