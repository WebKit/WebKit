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
        this._requestsAreInOrder = false;
        this._repositories = null;
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

    createdAt() { return this._createdAt; }
    isHidden() { return this._isHidden; }
    buildRequests() { return this._buildRequests; }
    addBuildRequest(request)
    {
        this._buildRequests.push(request);
        this._requestsAreInOrder = false;
        this._requestedCommitSets = null;
        this._commitSetToLabel.clear();
    }

    repetitionCount()
    {
        if (!this._buildRequests.length)
            return 0;
        var commitSet = this._buildRequests[0].commitSet();
        var count = 0;
        for (var request of this._buildRequests) {
            if (request.commitSet() == commitSet)
                count++;
        }
        return count;
    }

    requestedCommitSets()
    {
        if (!this._requestedCommitSets) {
            this._orderBuildRequests();
            this._requestedCommitSets = [];
            for (var request of this._buildRequests) {
                var set = request.commitSet();
                if (!this._requestedCommitSets.includes(set))
                    this._requestedCommitSets.push(set);
            }
            this._requestedCommitSets.sort(function (a, b) { return a.latestCommitTime() - b.latestCommitTime(); });
            var setIndex = 0;
            for (var set of this._requestedCommitSets) {
                this._commitSetToLabel.set(set, String.fromCharCode('A'.charCodeAt(0) + setIndex));
                setIndex++;
            }

        }
        return this._requestedCommitSets;
    }

    requestsForCommitSet(commitSet)
    {
        this._orderBuildRequests();
        return this._buildRequests.filter(function (request) { return request.commitSet() == commitSet; });
    }

    labelForCommitSet(commitSet)
    {
        console.assert(this._requestedCommitSets);
        return this._commitSetToLabel.get(commitSet);
    }

    _orderBuildRequests()
    {
        if (this._requestsAreInOrder)
            return;
        this._buildRequests = this._buildRequests.sort(function (a, b) { return a.order() - b.order(); });
        this._requestsAreInOrder = true;
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
            var diff = afterMean - beforeMean;
            var smallerIsBetter = metric.isSmallerBetter();
            var changeType = diff < 0 == smallerIsBetter ? 'better' : 'worse';
            var changeLabel = Math.abs(diff / beforeMean * 100).toFixed(2) + '% ' + changeType;
            var isSignificant = Statistics.testWelchsT(beforeValues, afterValues);
            var significanceLabel = isSignificant ? 'significant' : 'insignificant';

            result.changeType = changeType;
            result.label = changeLabel;
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
        const revisionSets = this._revisionSetsFromCommitSets(commitSets);
        const params = {taskName, name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets};
        return PrivilegedAPI.sendRequest('create-test-group', params).then((data) => {
            return AnalysisTask.fetchById(data['taskId']);
        }).then((task) => {
            return this._fetchTestGroupsForTask(task.id()).then(() => task);
        });
    }

    static createAndRefetchTestGroups(task, name, repetitionCount, commitSets)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = this._revisionSetsFromCommitSets(commitSets);
        return PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(),
            name: name,
            repetitionCount: repetitionCount,
            revisionSets: revisionSets,
        }).then((data) => this._fetchTestGroupsForTask(task.id()));
    }

    static _revisionSetsFromCommitSets(commitSets)
    {
        return commitSets.map((commitSet) => {
            console.assert(commitSet instanceof CustomCommitSet || commitSet instanceof CommitSet);
            const revisionSet = {};
            for (let repository of commitSet.repositories())
                revisionSet[repository.id()] = commitSet.revisionForRepository(repository);
            const customRoots = commitSet.customRoots();
            if (customRoots && customRoots.length)
                revisionSet['customRoots'] = customRoots.map((uploadedFile) => uploadedFile.id());
            return revisionSet;
        });
    }

    static _fetchTestGroupsForTask(taskId)
    {
        return this.cachedFetch('/api/test-groups', {task: taskId}, true).then((data) => this._createModelsFromFetchedTestGroups(data));
    }

    static fetchByTask(taskId)
    {
        return this.cachedFetch('/api/test-groups', {task: taskId}).then(this._createModelsFromFetchedTestGroups.bind(this));
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
