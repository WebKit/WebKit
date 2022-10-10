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
        this._initialRepetitionCount = +object.initialRepetitionCount;
        this._repetitionType = object.repetitionType;
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
        this._initialRepetitionCount = +object.initialRepetitionCount;
        this._repetitionType = object.repetitionType;
    }

    task() { return AnalysisTask.findById(this._taskId); }
    createdAt() { return this._createdAt; }
    isHidden() { return this._isHidden; }
    buildRequests() { return this._buildRequests; }
    needsNotification() { return this._needsNotification; }
    mayNeedMoreRequests() { return this._mayNeedMoreRequests; }
    initialRepetitionCount() { return this._initialRepetitionCount; }
    repetitionType() { return this._repetitionType; }
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

    repetitionCountForCommitSet(commitSet)
    {
        if (!this._buildRequests.length)
            return 0;
        let count = 0;
        for (const request of this._buildRequests) {
            if (request.isTest() && request.commitSet() == commitSet)
                count++;
        }
        return count;
    }

    hasRetries()
    {
        return this.requestedCommitSets().some(commitSet => this.repetitionCountForCommitSet(commitSet) > this.initialRepetitionCount());
    }

    additionalRepetitionNeededToReachInitialRepetitionCount(commitSet)
    {
        const potentiallySuccessfulCount = this.requestsForCommitSet(commitSet).filter(
            (request) => request.isTest() && (request.hasCompleted() || !request.hasFinished())).length;
        return Math.max(0, this.initialRepetitionCount() - potentiallySuccessfulCount);
    }

    successfulTestCount(commitSet)
    {
        return this.requestsForCommitSet(commitSet).filter((request) => request.isTest() && request.hasCompleted()).length;
    }

    isFirstTestRequest(buildRequest)
    {
        return this._orderedBuildRequests().filter((request) => request.isTest())[0] == buildRequest;
    }

    precedingBuildRequest(buildRequest)
    {
        const orderedBuildRequests = this._orderedBuildRequests();
        const buildRequestIndex = orderedBuildRequests.indexOf(buildRequest);
        console.assert(buildRequestIndex >= 0);
        return orderedBuildRequests[buildRequestIndex - 1];
    }

    retryCountForCommitSet(commitSet)
    {
        const retryCount = this.repetitionCountForCommitSet(commitSet) - this.initialRepetitionCount();
        console.assert(retryCount >= 0);
        return retryCount;
    }

    retryCountsAreSameForAllCommitSets()
    {
        const retryCountForFirstCommitSet = this.retryCountForCommitSet(this.requestedCommitSets()[0]);
        return this.requestedCommitSets().every(commitSet => this.retryCountForCommitSet(commitSet) == retryCountForFirstCommitSet);
    }

    requestedCommitSets()
    {
        return this._computeRequestedCommitSetsLazily.evaluate(...this._orderedBuildRequests());
    }

    static _computeRequestedCommitSets(...orderedBuildRequests)
    {
        const requestedCommitSets = [];
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

            const isStatisticallySignificant = (probabilityRange) => probabilityRange && probabilityRange.range && probabilityRange.range[0] && probabilityRange.range[0] >= 0.95;
            const constructSignificanceLabel = (probabilityRange) => isStatisticallySignificant(probabilityRange) ? `significant with ${(probabilityRange.range[0] * 100).toFixed()}% probability` : 'insignificant';

            const probabilityRangeForMean = Statistics.probabilityRangeForWelchsT(beforeValues, afterValues);
            const significanceLabelForMean = constructSignificanceLabel(probabilityRangeForMean);
            result.fullLabelForMean = `${result.label} (${significanceLabelForMean})`;
            result.isStatisticallySignificantForMean = isStatisticallySignificant(probabilityRangeForMean);
            result.probabilityRangeForMean = probabilityRangeForMean.range;

            const adaptMeasurementToSamples = (measurement) => ({sum: measurement.sum, squareSum: measurement.squareSum, sampleSize: measurement.iterationCount});
            const probabilityRangeForIndividual = Statistics.probabilityRangeForWelchsTForMultipleSamples(beforeMeasurements.map(adaptMeasurementToSamples), afterMeasurements.map(adaptMeasurementToSamples));
            const significanceLabelForIndividual = constructSignificanceLabel(probabilityRangeForIndividual);
            result.fullLabelForIndividual = `${result.label} (${significanceLabelForIndividual})`;
            result.isStatisticallySignificantForIndividual = isStatisticallySignificant(probabilityRangeForIndividual.range[0]);
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

    async cancelPendingRequests()
    {
        return this._updateBuildRequest({
            group: this.id(),
            cancel: true,
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

    async addMoreBuildRequests(addCount, commitSet = null)
    {
        console.assert(!commitSet || this.repetitionType() == 'sequential');
        console.assert(!commitSet || commitSet instanceof CommitSet);
        return await this._updateBuildRequest({
            group: this.id(),
            addCount,
            commitSet: commitSet ? commitSet.id() : null
        }, 'add-build-requests');
    }

    async clearMayNeedMoreBuildRequests()
    {
        return await this._updateBuildRequest({
            group: this.id(),
            mayNeedMoreRequests: false
        });
    }

    static async createWithTask(taskName, platform, test, groupName, repetitionCount, repetitionType, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const data = await PrivilegedAPI.sendRequest('create-test-group', {
            taskName, name: groupName, platform: platform.id(), test: test.id(), repetitionCount, revisionSets,
            repetitionType, needsNotification: !!notifyOnCompletion});
        const task = await AnalysisTask.fetchById(data['taskId'], /* ignoreCache */ true);
        await this.fetchForTask(task.id());
        return task;
    }

    static async createWithCustomConfiguration(task, platform, test, groupName, repetitionCount, repetitionType, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const data = await PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(), name: groupName, platform: platform.id(), test: test.id(), repetitionCount, repetitionType,
            revisionSets, needsNotification: !!notifyOnCompletion});
        await this.fetchById(data['testGroupId'], /* ignoreCache */ true);
        return this.findAllByTask(data['taskId']);
    }

    static async createAndRefetchTestGroups(task, name, repetitionCount, repetitionType, commitSets, notifyOnCompletion)
    {
        console.assert(commitSets.length == 2);
        const revisionSets = CommitSet.revisionSetsFromCommitSets(commitSets);
        const data = await PrivilegedAPI.sendRequest('create-test-group', {
            task: task.id(), name, repetitionCount, repetitionType, revisionSets,
            needsNotification: !!notifyOnCompletion
        });
        await this.fetchById(data['testGroupId'], /* ignoreCache */ true);
        return this.findAllByTask(data['taskId']);
    }

    async scheduleMoreRequestsOrClearFlag(maxRetryFactor)
    {
        if (!this.mayNeedMoreRequests())
            return 0;

        if (this.isHidden()) {
            await this.clearMayNeedMoreBuildRequests();
            return 0;
        }

        console.assert(TriggerableConfiguration.isValidRepetitionType(this.repetitionType()));
        const repetitionLimit = maxRetryFactor * this.initialRepetitionCount();
        const {retryCount, stopFutureRetries} = await (this.repetitionType() == 'sequential' ?
            this._createSequentialRetriesForTestGroup(repetitionLimit): this._createAlternatingRetriesForTestGroup(repetitionLimit));

        if (stopFutureRetries)
            await this.clearMayNeedMoreBuildRequests();

        return retryCount;
    }

    async _createAlternatingRetriesForTestGroup(repetitionLimit)
    {
        const additionalRepetitionNeeded = this.requestedCommitSets().reduce(
            (currentMax, commitSet) => Math.max(this.additionalRepetitionNeededToReachInitialRepetitionCount(commitSet), currentMax), 0);
        console.assert(additionalRepetitionNeeded <= this.initialRepetitionCount());

        const retryCount = this.requestedCommitSets().reduce(
            (currentMin, commitSet) => Math.min(Math.floor(repetitionLimit - this.repetitionCountForCommitSet(commitSet)), currentMin), additionalRepetitionNeeded);

        if (retryCount <= 0)
            return {retryCount: 0, stopFutureRetries: true};

        const eachCommitSetHasCompletedAtLeastOneTest = this.requestedCommitSets().every(this.successfulTestCount.bind(this));
        if (!eachCommitSetHasCompletedAtLeastOneTest) {
            const hasUnfinishedBuildRequest = this.requestedCommitSets().some((set) =>
                this.requestsForCommitSet(set).some((request) => request.isTest() && !request.hasFinished()));
            return {retryCount: 0, stopFutureRetries: !hasUnfinishedBuildRequest};
        }

        await this.addMoreBuildRequests(retryCount);
        return {retryCount, stopFutureRetries: false};
    }

    async _createSequentialRetriesForTestGroup(repetitionLimit)
    {
        const lastItem = (array) => array[array.length - 1];
        const commitSets = this.requestedCommitSets();
        console.assert(commitSets.every((currentSet, index) =>
            !index || lastItem(this.requestsForCommitSet(commitSets[index - 1])).order() < this.requestsForCommitSet(currentSet)[0].order()));

        const allTestRequestsHaveFailedForSomeCommitSet = commitSets.some(commitSet =>
            this.requestsForCommitSet(commitSet).every(request => !request.isTest() || request.hasFailed()));

        if (allTestRequestsHaveFailedForSomeCommitSet)
            return {retryCount: 0, stopFutureRetries: true};

        for (const commitSet of commitSets) {
            if (this.successfulTestCount(commitSet) >= this.initialRepetitionCount())
                continue;

            const additionalRepetition = this.additionalRepetitionNeededToReachInitialRepetitionCount(commitSet);
            const retryCount = Math.min(Math.floor(repetitionLimit - this.repetitionCountForCommitSet(commitSet)), additionalRepetition);
            if (retryCount <= 0)
                continue;

            await this.addMoreBuildRequests(retryCount, commitSet);
            return {retryCount, stopFutureRetries: false};
        }

        return {retryCount: 0, stopFutureRetries: true};
    }

    static findAllByTask(taskId)
    {
        return TestGroup.all().filter((testGroup) => testGroup._taskId == taskId);
    }

    static async fetchById(testGroupId, ignoreCache = false)
    {
        const data = await this.cachedFetch(`/api/test-groups/${testGroupId}`, { }, ignoreCache);
        this._createModelsFromFetchedTestGroups(data);
        return this.findById(testGroupId);
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
