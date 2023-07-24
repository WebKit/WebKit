'use strict';

let assert = require('assert');

require('./v3-models.js');

let BuildbotSyncer = require('./buildbot-syncer').BuildbotSyncer;

class BuildbotTriggerable {
    constructor(config, remote, buildbotRemote, workerInfo, maxRetryFactor, logger)
    {
        this._name = config.triggerableName;
        assert(typeof(this._name) == 'string', 'triggerableName must be specified');

        this._lookbackCount = config.lookbackCount;
        assert(typeof(this._lookbackCount) == 'number' && this._lookbackCount > 0, 'lookbackCount must be a number greater than 0');

        this._remote = remote;
        this._config = config;
        this._buildbotRemote = buildbotRemote;

        this._workerInfo = workerInfo;
        assert(typeof(workerInfo.name) == 'string', 'worker name must be specified');
        assert(typeof(workerInfo.password) == 'string', 'worker password must be specified');

        this._syncers = null;
        this._logger = logger || {log: () => { }, error: () => { }};
        this._maxRetryFactor = maxRetryFactor;
    }

    getBuilderNameToIDMap()
    {
        return this._buildbotRemote.getJSON("/api/v2/builders").then((content) => {
            assert(content.builders instanceof Array);

            const builderNameToIDMap = {};
            for (const builder of content.builders)
                builderNameToIDMap[builder.name] = builder.builderid;

            return builderNameToIDMap;
        });
    }

    initSyncers()
    {
        return this.getBuilderNameToIDMap().then((builderNameToIDMap) => {
            this._syncers = BuildbotSyncer._loadConfig(this._buildbotRemote, this._config, builderNameToIDMap);
        });
    }

    name() { return this._name; }

    updateTriggerable()
    {
        const map = new Map;
        let repositoryGroups = [];
        for (const syncer of this._syncers) {
            for (const config of syncer.testConfigurations()) {
                const entry = {test: config.test.id(), platform: config.platform.id(),
                    supportedRepetitionTypes: config.supportedRepetitionTypes,
                    testParameters: config.testParameters.map(parameter => parameter.id())};
                map.set(entry.test + '-' + entry.platform, entry);
            }
            // FIXME: Move BuildbotSyncer._loadConfig here and store repository groups directly.
            repositoryGroups = syncer.repositoryGroups();
        }
        return this._remote.postJSONWithStatus(`/api/update-triggerable/`, {
            'workerName': this._workerInfo.name,
            'workerPassword': this._workerInfo.password,
            'triggerable': this._name,
            'configurations': Array.from(map.values()),
            'repositoryGroups': Object.keys(repositoryGroups).map((groupName) => {
                const group = repositoryGroups[groupName];
                return {
                    name: groupName,
                    description: group.description,
                    acceptsRoots: group.acceptsRoots,
                    repositories: group.repositoryList,
                };
            })});
    }

    async syncOnce()
    {
        this._logger.log(`Fetching build requests for ${this._name}...`);

        const buildRequests = await BuildRequest.fetchForTriggerable(this._name);
        const testGroupIds = new Set(buildRequests.map(request => request.testGroupId()));
        await Promise.all(Array.from(testGroupIds).map(testGroupId => TestGroup.fetchById(testGroupId, /* ignoreCache */ true)));
        const validRequests = this._validateRequests(buildRequests);
        const buildRequestsByGroup = BuildbotTriggerable._testGroupMapForBuildRequests(buildRequests);
        let updates = await this._pullBuildbotOnAllSyncers(buildRequestsByGroup);
        let rootReuseUpdates = { };
        this._logger.log('Scheduling builds');
        const testGroupList = Array.from(buildRequestsByGroup.values()).sort(function (a, b) { return a.groupOrder - b.groupOrder; });

        for (const group of testGroupList) {
            const request = this._nextRequestInGroup(group, updates);
            if (!validRequests.has(request))
                continue;

            const shouldDefer = this._shouldDeferSequentialTestRequestForNewCommitSet(request);
            if (shouldDefer)
                this._logger.log(`Defer scheduling build request "${request.id()}" until completed build requests for previous configuration meet the expectation or exhaust retry.`);
            else
                await this._scheduleRequest(group, request, rootReuseUpdates);
        }

        // Pull all buildbots for the second time since the previous step may have scheduled more builds
        updates = await this._pullBuildbotOnAllSyncers(buildRequestsByGroup);

        // rootReuseUpdates will be overridden by status fetched from buildbot.
        updates = {
            ...rootReuseUpdates,
            ...updates
        };
        return await this._remote.postJSONWithStatus(`/api/build-requests/${this._name}`, {
            'workerName': this._workerInfo.name,
            'workerPassword': this._workerInfo.password,
            'buildRequestUpdates': updates});
    }

    async _scheduleRequest(testGroup, buildRequest, updates)
    {
        const buildRequestForRootReuse = await buildRequest.findBuildRequestWithSameRoots();
        if (buildRequestForRootReuse) {
            if (!buildRequestForRootReuse.hasCompleted()) {
                this._logger.log(`Found build request ${buildRequestForRootReuse.id()} is building the same root, will wait until it finishes.`);
                return;
            }

            this._logger.log(`Will reuse existing root built from ${buildRequestForRootReuse.id()} for ${buildRequest.id()}`);
            updates[buildRequest.id()] = {status: 'completed', url: buildRequestForRootReuse.statusUrl(),
                statusDescription: buildRequestForRootReuse.statusDescription(),
                buildRequestForRootReuse: buildRequestForRootReuse.id()};
            return;
        }

        return this._scheduleRequestIfWorkerIsAvailable(buildRequest, testGroup.requests,
            buildRequest.isBuild() ? testGroup.buildSyncer : testGroup.testSyncer,
            buildRequest.isBuild() ? testGroup.buildWorkerName : testGroup.testWorkerName);
    }

    _shouldDeferSequentialTestRequestForNewCommitSet(buildRequest)
    {
        if (buildRequest.isBuild())
            return false;

        const testGroup = buildRequest.testGroup();
        if (testGroup.repetitionType() != 'sequential')
            return false;

        if (testGroup.isFirstTestRequest(buildRequest))
            return false;

        const precedingBuildRequest = testGroup.precedingBuildRequest(buildRequest);
        console.assert(precedingBuildRequest && !precedingBuildRequest.isBuild());

        const previousCommitSet = precedingBuildRequest.commitSet()
        if (previousCommitSet === buildRequest.commitSet())
            return false;

        const allRequestsFailedForPreviousCommitSet = testGroup.requestsForCommitSet(previousCommitSet).every(request => !request.isTest() || request.hasFailed());
        if (allRequestsFailedForPreviousCommitSet)
            return false;

        const repetitionLimit = testGroup.initialRepetitionCount() * this._maxRetryFactor;
        if (testGroup.repetitionCountForCommitSet(previousCommitSet) >= repetitionLimit)
            return false;

        return testGroup.initialRepetitionCount() > testGroup.successfulTestCount(previousCommitSet);
    }

    _validateRequests(buildRequests)
    {
        const testPlatformPairs = {};
        const validatedRequests = new Set;
        for (let request of buildRequests) {
            if (!this._syncers.some((syncer) => syncer.matchesConfiguration(request))) {
                const key = request.platform().id + '-' + (request.isBuild() ? 'build' : request.test().id());
                const kind = request.isBuild() ? 'Building' : `"${request.test().fullName()}"`;
                if (!(key in testPlatformPairs))
                    this._logger.error(`Build request ${request.id()} has no matching configuration: ${kind} on "${request.platform().name()}".`);
                testPlatformPairs[key] = true;
                continue;
            }
            const triggerable = request.triggerable();
            if (!triggerable) {
                this._logger.error(`Build request ${request.id()} does not specify a valid triggerable`);
                continue;
            }
            assert(triggerable instanceof Triggerable, 'Must specify a valid triggerable');
            assert.strictEqual(triggerable.name(), this._name, 'Must specify the triggerable of this syncer');
            const repositoryGroup = request.repositoryGroup();
            if (!repositoryGroup) {
                this._logger.error(`Build request ${request.id()} does not specify a repository group. Such a build request is no longer supported.`);
                continue;
            }
            const acceptedGroups = triggerable.repositoryGroups();
            if (!acceptedGroups.includes(repositoryGroup)) {
                const acceptedNames = acceptedGroups.map((group) => group.name()).join(', ');
                this._logger.error(`Build request ${request.id()} specifies ${repositoryGroup.name()} but triggerable ${this._name} only accepts ${acceptedNames}`);
                continue;
            }
            validatedRequests.add(request);
        }

        return validatedRequests;
    }

    async _pullBuildbotOnAllSyncers(buildRequestsByGroup)
    {
        let updates = {};
        let associatedRequests = new Set;
        await Promise.all(this._syncers.map(async (syncer) => {
            const entryList = await syncer.pullBuildbot(this._lookbackCount);
            for (const entry of entryList) {
                const request = BuildRequest.findById(entry.buildRequestId());
                if (!request)
                    continue;

                const info = buildRequestsByGroup.get(request.testGroupId());
                if (!info) {
                    assert(request.testGroup().hasFinished());
                    continue;
                }

                associatedRequests.add(request);

                if (request.isBuild()) {
                    assert(!info.buildSyncer || info.buildSyncer == syncer);
                    if (entry.workerName()) {
                        assert(!info.buildWorkerName || info.buildWorkerName == entry.workerName());
                        info.buildWorkerName = entry.workerName();
                    }
                    info.buildSyncer = syncer;
                } else {
                    assert(!info.testSyncer || info.testSyncer == syncer);
                    if (entry.workerName()) {
                        assert(!info.testWorkerName || info.testWorkerName == entry.workerName());
                        info.testWorkerName = entry.workerName();
                    }
                    info.testSyncer = syncer;
                }

                const newStatus = entry.buildRequestStatusIfUpdateIsNeeded(request);
                if (newStatus) {
                    this._logger.log(`Updating the status of build request ${request.id()} from ${request.status()} to ${newStatus}`);
                    updates[entry.buildRequestId()] = {status: newStatus, url: entry.url(), statusDescription: entry.statusDescription()};
                } else if (!request.statusUrl() || request.statusDescription() != entry.statusDescription()) {
                    this._logger.log(`Updating build request ${request.id()} status URL to ${entry.url()} and status detail from ${request.statusDescription()} to ${entry.statusDescription()}`);
                    updates[entry.buildRequestId()] = {status: request.status(), url: entry.url(), statusDescription: entry.statusDescription()};
                }
            }
        }));

        for (const group of buildRequestsByGroup.values()) {
            for (const request of group.requests) {
                if (request.hasStarted() && !request.hasFinished() && !associatedRequests.has(request)) {
                    this._logger.log(`Updating the status of build request ${request.id()} from ${request.status()} to failedIfNotCompleted`);
                    assert(!(request.id() in updates));
                    updates[request.id()] = {status: 'failedIfNotCompleted'};
                }
            }
        }

        return updates;
    }

    _nextRequestInGroup(groupInfo, pendingUpdates)
    {
        for (const request of groupInfo.requests) {
            if (request.isScheduled() || (request.id() in pendingUpdates && pendingUpdates[request.id()]['status'] == 'scheduled'))
                return null;
            if (request.isPending() && !(request.id() in pendingUpdates))
                return request;
            if (request.isBuild() && !request.hasCompleted())
                return null; // A build request is still pending, scheduled, running, or failed.
        }
        return null;
    }

    _scheduleRequestIfWorkerIsAvailable(nextRequest, requestsInGroup, syncer, workerName)
    {
        if (!nextRequest)
            return null;

        const isFirstRequest = nextRequest == requestsInGroup[0] || !nextRequest.order();
        if (!isFirstRequest) {
            if (syncer)
                return this._scheduleRequestWithLog(syncer, nextRequest, requestsInGroup, workerName);
            this._logger.error(`Could not identify the syncer for ${nextRequest.id()}.`);
        }

        // Pick a new syncer for the first test.
        for (const syncer of this._syncers) {
            // Only schedule A/B tests to queues whose last job was successful.
            if (syncer.isTester() && !nextRequest.order() && !syncer.lastCompletedBuildSuccessful())
                continue;

            const promise = this._scheduleRequestWithLog(syncer, nextRequest, requestsInGroup, null);
            if (promise)
                return promise;
        }
        return null;
    }

    _scheduleRequestWithLog(syncer, request, requestsInGroup, workerName)
    {
        const promise = syncer.scheduleRequestInGroupIfAvailable(request, requestsInGroup, workerName);
        if (!promise)
            return promise;
        this._logger.log(`Scheduling build request ${request.id()}${workerName ? ' on ' + workerName : ''} in ${syncer.builderName()}`);
        return promise;
    }

    static _testGroupMapForBuildRequests(buildRequests)
    {
        const map = new Map;
        let groupOrder = 0;
        for (let request of buildRequests) {
            let groupId = request.testGroupId();
            if (!map.has(groupId)) // Don't use real TestGroup objects to avoid executing postgres query in the server
                map.set(groupId, {id: groupId, groupOrder: groupOrder++, requests: [request], buildSyncer: null, testSyncer: null, workerName: null});
            else
                map.get(groupId).requests.push(request);
        }
        return map;
    }
}

if (typeof module != 'undefined')
    module.exports.BuildbotTriggerable = BuildbotTriggerable;
