
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
        this._requestedRootSets = null;
        this._rootSetToLabel = new Map;
        this._allRootSets = null;
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
        this._requestedRootSets = null;
        this._rootSetToLabel.clear();
    }

    repetitionCount()
    {
        if (!this._buildRequests.length)
            return 0;
        var rootSet = this._buildRequests[0].rootSet();
        var count = 0;
        for (var request of this._buildRequests) {
            if (request.rootSet() == rootSet)
                count++;
        }
        return count;
    }

    requestedRootSets()
    {
        if (!this._requestedRootSets) {
            this._orderBuildRequests();
            this._requestedRootSets = [];
            for (var request of this._buildRequests) {
                var set = request.rootSet();
                if (!this._requestedRootSets.includes(set))
                    this._requestedRootSets.push(set);
            }
            this._requestedRootSets.sort(function (a, b) { return a.latestCommitTime() - b.latestCommitTime(); });
            var setIndex = 0;
            for (var set of this._requestedRootSets) {
                this._rootSetToLabel.set(set, String.fromCharCode('A'.charCodeAt(0) + setIndex));
                setIndex++;
            }

        }
        return this._requestedRootSets;
    }

    requestsForRootSet(rootSet)
    {
        this._orderBuildRequests();
        return this._buildRequests.filter(function (request) { return request.rootSet() == rootSet; });
    }

    labelForRootSet(rootSet)
    {
        console.assert(this._requestedRootSets);
        return this._rootSetToLabel.get(rootSet);
    }

    _orderBuildRequests()
    {
        if (this._requestsAreInOrder)
            return;
        this._buildRequests = this._buildRequests.sort(function (a, b) { return a.order() - b.order(); });
        this._requestsAreInOrder = true;
    }

    didSetResult(request)
    {
        this._allRootSets = null;
    }

    hasCompleted()
    {
        return this._buildRequests.every(function (request) { return request.hasCompleted(); });
    }

    hasStarted()
    {
        return this._buildRequests.some(function (request) { return request.hasStarted(); });
    }

    hasPending()
    {
        return this._buildRequests.some(function (request) { return request.hasPending(); });
    }

    compareTestResults(rootSetA, rootSetB)
    {
        var beforeValues = this._valuesForRootSet(rootSetA);
        var afterValues = this._valuesForRootSet(rootSetB);
        var beforeMean = Statistics.sum(beforeValues) / beforeValues.length;
        var afterMean = Statistics.sum(afterValues) / afterValues.length;

        var metric = AnalysisTask.findById(this._taskId).metric();
        console.assert(metric);

        var result = {changeType: null, status: 'failed', label: 'Failed', fullLabel: 'Failed', isStatisticallySignificant: false};

        var hasCompleted = this.hasCompleted();
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

    _valuesForRootSet(rootSet)
    {
        var requests = this.requestsForRootSet(rootSet);
        var values = [];
        for (var request of requests) {
            if (request.result())
                values.push(request.result().value);
        }
        return values;
    }

    updateName(newName)
    {
        var self = this;
        var id = this.id();
        return PrivilegedAPI.sendRequest('update-test-group', {
            group: id,
            name: newName,
        }).then(function (data) {
            return TestGroup.cachedFetch(`../api/test-groups/${id}`, {}, true)
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
            return TestGroup.cachedFetch(`../api/test-groups/${id}`, {}, true)
                .then(TestGroup._createModelsFromFetchedTestGroups.bind(TestGroup));
        });
    }

    static createAndRefetchTestGroups(task, name, repetitionCount, rootSets)
    {
        var self = this;
        return PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(),
            name: name,
            repetitionCount: repetitionCount,
            rootSets: rootSets,
        }).then(function (data) {
            return self.cachedFetch('../api/test-groups', {task: task.id()}, true).then(self._createModelsFromFetchedTestGroups.bind(self));
        });
    }

    static fetchByTask(taskId)
    {
        return this.cachedFetch('../api/test-groups', {task: taskId}).then(this._createModelsFromFetchedTestGroups.bind(this));
    }

    static _createModelsFromFetchedTestGroups(data)
    {
        var testGroups = data['testGroups'].map(function (row) {
            row.platform = Platform.findById(row.platform);
            return TestGroup.ensureSingleton(row.id, row);
        });

        var rootIdMap = {};
        for (var root of data['roots'])
            rootIdMap[root.id] = root;

        var rootSets = data['rootSets'].map(function (row) {
            row.roots = row.roots.map(function (rootId) { return rootIdMap[rootId]; });
            return RootSet.ensureSingleton(row.id, row);
        });

        var buildRequests = data['buildRequests'].map(function (rawData) {
            rawData.testGroup = TestGroup.findById(rawData.testGroup);
            rawData.rootSet = RootSet.findById(rawData.rootSet);
            return BuildRequest.ensureSingleton(rawData.id, rawData);
        });

        return testGroups;
    }
}
