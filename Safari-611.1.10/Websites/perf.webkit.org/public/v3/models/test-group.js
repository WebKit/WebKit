'use strict';

class TestGroup extends LabeledObject {

    constructor(id, object)
    {
        super(id, object);
        this._taskId = object.task;
        this._authorName = object.author;
        this._createdAt = new Date(object.createdAt);
        this._isHidden = object.hidden;
        this._needsNotification = object.needsNotification;
        this._mayNeedMoreRequests = object.mayNeedMoreRequests;
        this._initialRepetitionCount = object.initialRepetitionCount;
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
        this._needsNotification = object.needsNotification;
        this._notificationSentAt = object.notificationSentAt ? new Date(object.notificationSentAt) : null;
        this._mayNeedMoreRequests = object.mayNeedMoreRequests;
        this._initialRepetitionCount = object.initialRepetitionCount;
    }

    task() { return AnalysisTask.findById(this._taskId); }
    createdAt() { return this._createdAt; }
    isHidden() { return this._isHidden; }
    buildRequests() { return this._buildRequests; }
    needsNotification() { return this._needsNotification; }
    mayNeedMoreRequests() { return this._mayNeedMoreRequests; }
    initialRepetitionCount() { return this._initialRepetitionCount; }
    notificationSentAt() { return this._notificationSentAt; }
    author() { return this._authorName; }
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

    async fetchTask()
    {
        if (this.task())
            return this.task();
        return await AnalysisTask.fetchById(this._taskId);
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

    compareTestResults(metric, beforeMeasurements, afterMeasurements)
    {
        console.assert(metric);
        const beforeValues = beforeMeasurements.map((measurment) => measurment.value);
        const afterValues = afterMeasurements.map((measurement) => measurement.value);
        const beforeMean = Statistics.sum(beforeValues) / beforeValues.length;
        const afterMean = Statistics.sum(afterValues) / afterValues.length;

        const result = {changeType: null, status: 'failed', label: 'Failed', fullLabelForMean: 'Failed',
            isStatisticallySignificantForMean: false, fullLabelForIndividual: 'Failed', isStatisticallySignificantForIndividual: false,
            probabilityRangeForMean: [null, null], probabilityRangeForIndividual: [null, null]};

        const hasCompleted = this.hasFinished();
        if (!hasCompleted) {
            if (this.hasStarted()) {
                result.status = 'running';
                result.label = 'Running';
                result.fullLabelForMean = 'Running';
                result.fullLabelForIndividual = 'Running';
            } else {
                console.assert(result.changeType === null);
                result.status = 'pending';
                result.label = 'Pending';
                result.fullLabelForMean = 'Pending';
                result.fullLabelForIndividual = 'Pending';
            }
        }

        if (beforeValues.length && afterValues.length) {
            const summary = metric.labelForDifference(beforeMean, afterMean, 'better', 'worse');
            result.changeType = summary.changeType;
            result.label = summary.changeLabel;


            const constructSignificanceLabel = (probabilityRange) => !!probabilityRange.range[0] ? `significant with ${(probabilityRange.range[0] * 100).toFixed()}% probability` : 'insignificant';

            const probabilityRangeForMean = Statistics.probabilityRangeForWelchsT(beforeValues, afterValues);
            const significanceLabelForMean = constructSignificanceLabel(probabilityRangeForMean);
            result.fullLabelForMean = `${result.label} (${significanceLabelForMean})`;
            result.isStatisticallySignificantForMean = !!probabilityRangeForMean.range[0];
            result.probabilityRangeForMean = probabilityRangeForMean.range;

            const adaptMeasurementToSamples = (measurement) => ({sum: measurement.sum, squareSum: measurement.squareSum, sampleSize: measurement.iterationCount});
            const probabilityRangeForIndividual = Statistics.probabilityRangeForWelchsTForMultipleSamples(beforeMeasurements.map(adaptMeasurementToSamples), afterMeasurements.map(adaptMeasurementToSamples));
            const significanceLabelForIndividual = constructSignificanceLabel(probabilityRangeForIndividual);
            result.fullLabelForIndividual = `${result.label} (${significanceLabelForIndividual})`;
            result.isStatisticallySignificantForIndividual = !!probabilityRangeForIndividual.range[0];
            result.probabilityRangeForIndividual = probabilityRangeForIndividual.range;

            if (hasCompleted)
                result.status = result.isStatisticallySignificantForMean ? result.changeType : 'unchanged';
        }

        return result;
    }

    async _updateBuildRequest(content, endPoint='update-test-group')
    {
        await PrivilegedAPI.sendRequest(endPoint, content);
        const data = await TestGroup.cachedFetch(`/api/test-groups/${this.id()}`, {}, true);
        return TestGroup._createModelsFromFetchedTestGroups(data);
    }

    updateName(newName)
    {
        return this._updateBuildRequest({
            group: this.id(),
            name: newName,
        });
    }

    updateHiddenFlag(hidden)
    {
        return this._updateBuildRequest({
            group: this.id(),
            hidden: !!hidden,
        });
    }

    async didSendNotification()
    {
        return await this._updateBuildRequest({
            group: this.id(),
            needsNotification: false,
            notificationSentAt: (new Date).toISOString()
        });
    }

    async addMoreBuildRequests(addCount)
    {
        return await this._updateBuildRequest({
            group: this.id(),
            addCount,
        }, 'add-build-requests');
    }

    async clearMayNeedMoreBuildRequests()
    {
        return await this._updateBuildRequest({
            group: this.id(),
            mayNeedMoreRequests: false
        });
    }

    static createWithTask(taskName, platform, test, groupName, repetitionCount, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const params = {taskName, name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets, needsNotification: !!notifyOnCompletion};
        return PrivilegedAPI.sendRequest('create-test-group', params).then((data) => {
            return AnalysisTask.fetchById(data['taskId'], true);
        }).then((task) => {
            return this.fetchForTask(task.id()).then(() => task);
        });
    }

    static createWithCustomConfiguration(task, platform, test, groupName, repetitionCount, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const params = {task: task.id(), name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets, needsNotification: !!notifyOnCompletion};
        return PrivilegedAPI.sendRequest('create-test-group', params).then((data) => {
            return this.fetchForTask(data['taskId'], true);
        });
    }

    static createAndRefetchTestGroups(task, name, repetitionCount, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        return PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(),
            name: name,
            repetitionCount: repetitionCount,
            revisionSets: revisionSets,
            needsNotification: !!notifyOnCompletion,
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

    static fetchAllWithNotificationReady()
    {
        return this.cachedFetch('/api/test-groups/ready-for-notification', null, true).then(this._createModelsFromFetchedTestGroups.bind(this));
    }

    static fetchAllThatMayNeedMoreRequests()
    {
        return this.cachedFetch('/api/test-groups/need-more-requests', null, true).then(this._createModelsFromFetchedTestGroups.bind(this));
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
