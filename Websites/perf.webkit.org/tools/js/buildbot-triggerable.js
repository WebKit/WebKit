'use strict';

let assert = require('assert');

require('./v3-models.js');

let BuildbotSyncer = require('./buildbot-syncer').BuildbotSyncer;

class BuildbotTriggerable {
    constructor(config, remote, buildbotRemote, slaveInfo, logger)
    {
        this._name = config.triggerableName;
        assert(typeof(this._name) == 'string', 'triggerableName must be specified');

        this._lookbackCount = config.lookbackCount;
        assert(typeof(this._lookbackCount) == 'number' && this._lookbackCount > 0, 'lookbackCount must be a number greater than 0');

        this._remote = remote;

        this._slaveInfo = slaveInfo;
        assert(typeof(slaveInfo.name) == 'string', 'slave name must be specified');
        assert(typeof(slaveInfo.password) == 'string', 'slave password must be specified');

        this._syncers = BuildbotSyncer._loadConfig(buildbotRemote, config);
        this._logger = logger || {log: function () { }, error: function () { }};
    }

    name() { return this._name; }

    syncOnce()
    {
        let syncerList = this._syncers;
        let buildReqeustsByGroup = new Map;

        let self = this;
        this._logger.log(`Fetching build requests for ${this._name}...`);
        return BuildRequest.fetchForTriggerable(this._name).then(function () {
            let buildRequests = BuildRequest.all();
            self._validateRequests(buildRequests);
            buildReqeustsByGroup = BuildbotTriggerable._testGroupMapForBuildRequests(buildRequests);
            return self._pullBuildbotOnAllSyncers(buildReqeustsByGroup);
        }).then(function (updates) {
            self._logger.log('Scheduling builds');
            let promistList = [];
            let testGroupList = Array.from(buildReqeustsByGroup.values()).sort(function (a, b) { return a.id - b.id; });
            for (let group of testGroupList) {
                let promise = self._scheduleNextRequestInGroupIfSlaveIsAvailable(group, updates);
                if (promise)
                    promistList.push(promise);
            }
            return Promise.all(promistList);
        }).then(function () {
            // Pull all buildbots for the second time since the previous step may have scheduled more builds.
            return self._pullBuildbotOnAllSyncers(buildReqeustsByGroup);
        }).then(function (updates) {
            // FIXME: Add a new API that just updates the requests.
            return self._remote.postJSON(`/api/build-requests/${self._name}`, {
                'slaveName': self._slaveInfo.name,
                'slavePassword': self._slaveInfo.password,
                'buildRequestUpdates': updates});
        }).then(function (response) {
            if (response['status'] != 'OK')
                self._logger.log('Failed to update the build requests status: ' + response['status']);
        })
    }

    _validateRequests(buildRequests)
    {
        let testPlatformPairs = {};
        for (let request of buildRequests) {
            if (!this._syncers.some(function (syncer) { return syncer.matchesConfiguration(request); })) {
                let key = request.platform().id + '-' + request.test().id();
                if (!(key in testPlatformPairs))
                    this._logger.error(`No matching configuration for "${request.test().fullName()}" on "${request.platform().name()}".`);                
                testPlatformPairs[key] = true;
            }
        }
    }

    _pullBuildbotOnAllSyncers(buildReqeustsByGroup)
    {
        let updates = {};
        let associatedRequests = new Set;
        let self = this;
        return Promise.all(this._syncers.map(function (syncer) {
            return syncer.pullBuildbot(self._lookbackCount).then(function (entryList) {
                for (let entry of entryList) {
                    let request = BuildRequest.findById(entry.buildRequestId());
                    if (!request)
                        continue;
                    associatedRequests.add(request);

                    let info = buildReqeustsByGroup.get(request.testGroupId());
                    assert(!info.syncer || info.syncer == syncer);
                    info.syncer = syncer;
                    if (entry.slaveName()) {
                        assert(!info.slaveName || info.slaveName == entry.slaveName());
                        info.slaveName = entry.slaveName();
                    }

                    let newStatus = entry.buildRequestStatusIfUpdateIsNeeded(request);
                    if (newStatus) {
                        self._logger.log(`Updating the status of build request ${request.id()} from ${request.status()} to ${newStatus}`);
                        updates[entry.buildRequestId()] = {status: newStatus, url: entry.url()};
                    } else if (!request.statusUrl()) {
                        self._logger.log(`Setting the status URL of build request ${request.id()} to ${entry.url()}`);
                        updates[entry.buildRequestId()] = {status: request.status(), url: entry.url()};
                    }
                }
            });
        })).then(function () {
            for (let request of BuildRequest.all()) {
                if (request.hasStarted() && !request.hasFinished() && !associatedRequests.has(request)) {
                    self._logger.log(`Updating the status of build request ${request.id()} from ${request.status()} to failedIfNotCompleted`);
                    assert(!(request.id() in updates));
                    updates[request.id()] = {status: 'failedIfNotCompleted'};
                }
            }
        }).then(function () { return updates; });
    }

    _scheduleNextRequestInGroupIfSlaveIsAvailable(groupInfo, pendingUpdates)
    {
        let orderedRequests = groupInfo.requests.sort(function (a, b) { return a.order() - b.order(); });
        let nextRequest = null;
        for (let request of orderedRequests) {
            if (request.isScheduled() || (request.id() in pendingUpdates && pendingUpdates[request.id()]['status'] == 'scheduled'))
                break;
            if (request.isPending() && !(request.id() in pendingUpdates)) {
                nextRequest = request;
                break;
            }
        }
        if (!nextRequest)
            return null;

        let firstRequest = !nextRequest.order();
        if (firstRequest) {
            this._logger.log(`Scheduling build request ${nextRequest.id()} on ${groupInfo.slaveName} in ${groupInfo.syncer.builderName()}`);
            return groupInfo.syncer.scheduleRequest(request, groupInfo.slaveName);
        }

        for (let syncer of this._syncers) {
            let promise = syncer.scheduleFirstRequestInGroupIfAvailable(nextRequest);
            if (promise) {
                let slaveName = groupInfo.slaveName ? ` on ${groupInfo.slaveName}` : '';
                this._logger.log(`Scheduling build request ${nextRequest.id()}${slaveName} in ${syncer.builderName()}`);
                return promise;
            }
        }
        return null;
    }

    static _testGroupMapForBuildRequests(buildRequests)
    {
        let map = new Map;
        for (let request of buildRequests) {
            let groupId = request.testGroupId();
            if (!map.has(groupId)) // Don't use real TestGroup objects to avoid executing postgres query in the server
                map.set(groupId, {id: groupId, requests: [request], syncer: null, slaveName: null});
            else
                map.get(groupId).requests.push(request);
        }
        return map;
    }
}

if (typeof module != 'undefined')
    module.exports.BuildbotTriggerable = BuildbotTriggerable;
