
class TestGroup extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._taskId = object.task;
        this._authorName = object.author;
        this._createdAt = new Date(object.createdAt);
        this._buildRequests = [];
        this._requestsAreInOrder = false;
        this._repositories = null;
        this._requestedRootSets = null;
        this._allRootSets = null;
        console.assert(!object.platform || object.platform instanceof Platform);
        this._platform = object.platform;
    }

    createdAt() { return this._createdAt; }
    buildRequests() { return this._buildRequests; }
    addBuildRequest(request)
    {
        this._buildRequests.push(request);
        this._requestsAreInOrder = false;
        this._requestedRootSets = null;
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
        }
        return this._requestedRootSets;
    }

    requestsForRootSet(rootSet)
    {
        this._orderBuildRequests();
        return this._buildRequests.filter(function (request) { return request.rootSet() == rootSet; });
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

    compareTestResults(rootSetA, rootSetB)
    {
        var beforeValues = this._valuesForRootSet(rootSetA);
        var afterValues = this._valuesForRootSet(rootSetB);
        var beforeMean = Statistics.sum(beforeValues) / beforeValues.length;
        var afterMean = Statistics.sum(afterValues) / afterValues.length;

        var metric = AnalysisTask.findById(this._taskId).metric();
        console.assert(metric);

        var result = {changeType: null, status: 'failed', label: 'Failed', fullLabel: 'Failed', isStatisticallySignificant: false};
        if (beforeValues.length && afterValues.length) {
            var diff = afterMean - beforeMean;
            var smallerIsBetter = metric.isSmallerBetter();
            result.changeType = diff < 0 == smallerIsBetter ? 'better' : 'worse';
            result.label = Math.abs(diff / beforeMean * 100).toFixed(2) + '% ' + result.changeType;
            result.isStatisticallySignificant = Statistics.testWelchsT(beforeValues, afterValues);
            result.status = result.isStatisticallySignificant ? result.changeType : 'unchanged';
        }

        if (!this.hasCompleted()) {
            result.status = 'incomplete';
            result.label = 'Running';
            result.fullLabel = 'Running';
        } else if (result.changeType) {
            var significance = result.isStatisticallySignificant ? 'significant' : 'insignificant';
            result.fullLabel = `${result.label} (statistically ${significance})`;
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

    static fetchByTask(taskId)
    {
        return this.cachedFetch('../api/test-groups', {task: taskId}).then(function (data) {
            var testGroups = data['testGroups'].map(function (row) {
                row.platform = Platform.findById(row.platform);
                return new TestGroup(row.id, row);
            });

            var rootIdMap = {};
            for (var root of data['roots'])
                rootIdMap[root.id] = root;

            var rootSets = data['rootSets'].map(function (row) {
                row.roots = row.roots.map(function (rootId) { return rootIdMap[rootId]; });
                row.testGroup = RootSet.findById(row.testGroup);
                return new RootSet(row.id, row);
            });

            var buildRequests = data['buildRequests'].map(function (rawData) {
                rawData.testGroup = TestGroup.findById(rawData.testGroup);
                rawData.rootSet = RootSet.findById(rawData.rootSet);
                return new BuildRequest(rawData.id, rawData);
            });

            return testGroups;
        });
    }

}
