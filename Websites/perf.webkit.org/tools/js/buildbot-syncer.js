'use strict';

let assert = require('assert');

require('./v3-models.js');

class BuildbotBuildEntry {
    constructor(syncer, rawData)
    {
        this.initialize(syncer, rawData);
    }

    initialize(syncer, rawData)
    {
        assert.strictEqual(parseInt(syncer.builderID()), parseInt(rawData['builderid']));

        this._syncer = syncer;
        this._buildbotBuildRequestId = rawData['buildrequestid'];
        this._hasFinished = rawData['complete'];
        this._isPending = 'claimed' in rawData && !rawData['claimed'];
        this._isInProgress = !this._isPending && !this._hasFinished;
        this._buildTag = rawData['number'];
        this._result = rawData['results'];
        this._workerName = rawData['properties'] && rawData['properties']['workername'] ? rawData['properties']['workername'][0] : null;
        this._buildRequestId = rawData['properties'] && rawData['properties'][syncer._buildRequestPropertyName]
            ? rawData['properties'][syncer._buildRequestPropertyName][0] : null;
        this._statusDescription = rawData['state_string'] && rawData['results'] !== 0 ? rawData['state_string'] : null;
    }

    syncer() { return this._syncer; }
    buildTag() { return this._buildTag; }
    workerName() { return this._workerName; }
    buildRequestId() { return this._buildRequestId; }
    isPending() { return this._isPending; }
    isInProgress() { return this._isInProgress; }
    hasFinished() { return this._hasFinished; }
    result() { return this._result; }
    statusDescription() { return this._statusDescription; }
    url() { return this.isPending() ? this._syncer.urlForPendingBuild(this._buildbotBuildRequestId) : this._syncer.urlForBuildSerial(this._buildTag); }

    buildRequestStatusIfUpdateIsNeeded(request)
    {
        assert.strictEqual(request.id(), this._buildRequestId);
        if (!request)
            return null;
        if (this.isPending()) {
            if (request.isPending())
                return 'scheduled';
        } else if (this.isInProgress()) {
            if (!request.hasStarted() || request.isScheduled())
                return 'running';
        } else if (this.hasFinished()) {
            if (!request.hasFinished())
                return 'failedIfNotCompleted';
        }
        return null;
    }
}


class BuildbotSyncer {

    constructor(remote, object, commonConfigurations)
    {
        this._remote = remote;
        this._type = null;
        this._configurations = [];
        this._repositoryGroups = commonConfigurations.repositoryGroups;
        this._workerPropertyName = commonConfigurations.workerArgument;
        this._platformPropertyName = commonConfigurations.platformArgument;
        this._buildRequestPropertyName = commonConfigurations.buildRequestArgument;
        this._builderSupportedRepetitionTypes = object.supportedRepetitionTypes;
        this._builderName = object.builder;
        this._builderID = object.builderID;
        this._workerList = object.workerList;
        this._entryList = null;
        this._lastCompletedBuild = null;
        this._workersWithNewRequests = new Set;
    }

    builderName() { return this._builderName; }
    builderID() { return this._builderID; }
    // Buildbot result codes: https://docs.buildbot.net/latest/developer/results.html
    lastCompletedBuildSuccessful() { return !this._lastCompletedBuild || !this._lastCompletedBuild.result() }

    addTestConfiguration(test, platform, supportedRepetitionTypes, propertiesTemplate)
    {
        assert(test instanceof Test);
        assert(platform instanceof Platform);
        assert(Array.isArray(supportedRepetitionTypes));
        assert(supportedRepetitionTypes.every(repetitionType => TriggerableConfiguration.isValidRepetitionType(repetitionType)));
        assert(this._type == null || this._type == 'tester');
        this._type = 'tester';
        this._configurations.push({test, platform, supportedRepetitionTypes, propertiesTemplate});
    }
    testConfigurations() { return this._type == 'tester' ? this._configurations : []; }

    addBuildConfiguration(platform, supportedRepetitionTypes, propertiesTemplate)
    {
        assert(platform instanceof Platform);
        assert(this._type == null || this._type == 'builder');
        assert(Array.isArray(supportedRepetitionTypes));
        assert(supportedRepetitionTypes.every(repetitionType => TriggerableConfiguration.isValidRepetitionType(repetitionType)));
        this._type = 'builder';
        this._configurations.push({test: null, platform, supportedRepetitionTypes, propertiesTemplate});
    }
    buildConfigurations() { return this._type == 'builder' ? this._configurations : []; }

    isTester() { return this._type == 'tester'; }

    repositoryGroups() { return this._repositoryGroups; }

    matchesConfiguration(request)
    {
        const requestRepetitionType = request.testGroup().repetitionType();
        if (!this._builderSupportedRepetitionTypes.includes(requestRepetitionType))
            return false;

        return this._configurations.some((config) => config.platform == request.platform() && config.test == request.test()
            && config.supportedRepetitionTypes.includes(requestRepetitionType));
    }

    scheduleRequest(newRequest, requestsInGroup, workerName)
    {
        assert(!this._workersWithNewRequests.has(workerName));
        let properties = this._propertiesForBuildRequest(newRequest, requestsInGroup);

        assert(properties['forcescheduler'], `forcescheduler was not specified in buildbot properties for build request ${newRequest.id()} on platform "${newRequest.platform().name()}" for builder "${this.builderName()}"`);
        assert.strictEqual(!this._workerPropertyName, !workerName);
        if (this._workerPropertyName)
            properties[this._workerPropertyName] = workerName;

        if (this._platformPropertyName)
            properties[this._platformPropertyName] = newRequest.platform().name();

        this._workersWithNewRequests.add(workerName);
        return this.scheduleBuildOnBuildbot(properties);
    }

    scheduleBuildOnBuildbot(properties)
    {
        const data = {jsonrpc: '2.0', method: 'force', id: properties[this._buildRequestPropertyName], params: properties};
        const path = this.pathForForceBuild(properties['forcescheduler']);
        return this._remote.postJSON(path, data);
    }

    scheduleRequestInGroupIfAvailable(newRequest, requestsInGroup, workerName)
    {
        assert(newRequest instanceof BuildRequest);

        if (!this.matchesConfiguration(newRequest))
            return null;

        let hasPendingBuildsWithoutWorkerNameSpecified = false;
        let usedWorkers = new Set;
        const latestEntryByWorkerName = new Map;
        for (let entry of this._entryList) {
            let entryPreventsNewRequest = entry.isPending();
            if (entry.isInProgress()) {
                const requestInProgress = BuildRequest.findById(entry.buildRequestId());
                if (!requestInProgress || requestInProgress.testGroupId() != newRequest.testGroupId())
                    entryPreventsNewRequest = true;
            }
            if (entryPreventsNewRequest) {
                if (!entry.workerName())
                    hasPendingBuildsWithoutWorkerNameSpecified = true;
                usedWorkers.add(entry.workerName());
            }
            if (!entry.isPending()) {
                console.assert(entry.workerName(), 'Non-pending entry must have a worker name');
                const lastFinishedEntry = latestEntryByWorkerName.get(entry.workerName());
                if (!lastFinishedEntry || +lastFinishedEntry.buildTag() < +entry.buildTag())
                    latestEntryByWorkerName.set(entry.workerName(), entry)
            }
        }

        for (let [name, entry] of latestEntryByWorkerName.entries()) {
            const latestActiveBuildRequest = BuildRequest.findById(entry.buildRequestId());
            if (!latestActiveBuildRequest)
                continue;
            if (latestActiveBuildRequest.testGroupId() == newRequest.testGroupId())
                continue;
            const testGroup = latestActiveBuildRequest.testGroup();
            if (!testGroup) {
                // The request might be for a very old test group we didn't fetch.
                continue;
            }
            if (testGroup.hasFinished() && !testGroup.mayNeedMoreRequests())
                continue;
            usedWorkers.add(name);
        }

        if (!this._workerList || hasPendingBuildsWithoutWorkerNameSpecified) {
            if (usedWorkers.size || this._workersWithNewRequests.size)
                return null;
            return this.scheduleRequest(newRequest, requestsInGroup, null);
        }

        if (workerName) {
            if (!usedWorkers.has(workerName) && !this._workersWithNewRequests.has(workerName))
                return this.scheduleRequest(newRequest, requestsInGroup, workerName);
            return null;
        }

        for (let workerName of this._workerList) {
            if (!usedWorkers.has(workerName) && !this._workersWithNewRequests.has(workerName))
                return this.scheduleRequest(newRequest, requestsInGroup, workerName);
        }

        return null;
    }

    pullBuildbot(count)
    {
        return this._remote.getJSON(this.pathForPendingBuilds()).then((content) => {
            const pendingEntries = (content.buildrequests || []).map((entry) => new BuildbotBuildEntry(this, entry));
            return this._pullRecentBuilds(count).then((entries) => {
                let entryByRequest = {};

                for (let entry of pendingEntries)
                    entryByRequest[entry.buildRequestId()] = entry;

                for (let entry of entries) {
                    entryByRequest[entry.buildRequestId()] = entry;
                    if ((!this._lastCompletedBuild || this._lastCompletedBuild.buildTag() < entry.buildTag()) && entry.hasFinished())
                        this._lastCompletedBuild = entry;
                }

                let entryList = [];
                for (let id in entryByRequest)
                    entryList.push(entryByRequest[id]);

                this._entryList = entryList;
                this._workersWithNewRequests.clear();

                return entryList;
            });
        });
    }

    _pullRecentBuilds(count)
    {
        if (!count)
            return Promise.resolve([]);

        return this._remote.getJSON(this.pathForRecentBuilds(count)).then((content) => {
            if (!('builds' in content))
                return [];
            return content.builds.map((build) => new BuildbotBuildEntry(this, build));
        });
    }

    pathForPendingBuilds() { return `/api/v2/builders/${this._builderID}/buildrequests?complete=false&claimed=false&property=*`; }
    pathForRecentBuilds(count) { return `/api/v2/builders/${this._builderID}/builds?limit=${count}&order=-number&property=*`; }
    pathForForceBuild(schedulerName) { return `/api/v2/forceschedulers/${schedulerName}`; }

    urlForBuildSerial(serail) { return this._remote.url(`/#/builders/${this._builderID}/builds/${serail}`); }
    urlForPendingBuild(buildRequestId) { return this._remote.url(`/#/buildrequests/${buildRequestId}`); }

    _propertiesForBuildRequest(buildRequest, requestsInGroup)
    {
        assert(buildRequest instanceof BuildRequest);
        assert(requestsInGroup[0] instanceof BuildRequest);

        const commitSet = buildRequest.commitSet();
        assert(commitSet instanceof CommitSet);

        const repositoryByName = {};
        for (let repository of commitSet.repositories())
            repositoryByName[repository.name()] = repository;

        const matchingConfiguration = this._configurations.find((config) => config.platform == buildRequest.platform() && config.test == buildRequest.test());
        assert(matchingConfiguration, `Build request ${buildRequest.id()} does not match a configuration in the builder "${this._builderName}"`);
        const propertiesTemplate = matchingConfiguration.propertiesTemplate;

        const repositoryGroup = buildRequest.repositoryGroup();
        assert(repositoryGroup.accepts(commitSet), `Build request ${buildRequest.id()} does not specify a commit set accepted by the repository group ${repositoryGroup.id()}`);

        const repositoryGroupConfiguration = this._repositoryGroups[repositoryGroup.name()];
        assert(repositoryGroupConfiguration, `Build request ${buildRequest.id()} uses an unsupported repository group "${repositoryGroup.name()}"`);

        const properties = {};
        for (let propertyName in propertiesTemplate)
            properties[propertyName] = propertiesTemplate[propertyName];

        const repositoryGroupTemplate = buildRequest.isBuild() ? repositoryGroupConfiguration.buildPropertiesTemplate : repositoryGroupConfiguration.testPropertiesTemplate;
        assert(!buildRequest.isBuild() || repositoryGroupTemplate, 'Repository group template cannot be null for build type build request');
        for (let propertyName in repositoryGroupTemplate) {
            let value = repositoryGroupTemplate[propertyName];
            const type = typeof(value) == 'object' ? value.type : 'string';
            switch (type) {
            case 'string':
                break;
            case 'revision':
                value = commitSet.revisionForRepository(value.repository);
                break;
            case 'patch':
                const patch = commitSet.patchForRepository(value.repository);
                if (!patch)
                    continue;
                value = patch.url();
                break;
            case 'roots':
                const rootFiles = commitSet.allRootFiles();
                if (!rootFiles.length)
                    continue;
                value = JSON.stringify(rootFiles.map((file) => ({url: file.url()})));
                break;
            case 'ownedRevisions':
                const ownedRepositories = commitSet.ownedRepositoriesForOwnerRepository(value.ownerRepository);
                if (!ownedRepositories)
                    continue;

                const revisionInfo = {};
                revisionInfo[value.ownerRepository.name()] = ownedRepositories.map((ownedRepository) => {
                    return {
                        'revision': commitSet.revisionForRepository(ownedRepository),
                        'repository': ownedRepository.name(),
                        'ownerRevision': commitSet.ownerRevisionForRepository(ownedRepository)
                    };
                });
                value = JSON.stringify(revisionInfo);
                break;
            case 'conditional':
                switch (value.condition) {
                case 'built':
                    const repositoryRequirement = value.repositoryRequirement;
                    const meetRepositoryRequirement = !repositoryRequirement.length || repositoryRequirement.some((repository) => commitSet.requiresBuildForRepository(repository));
                    if (!meetRepositoryRequirement || !requestsInGroup.some((otherRequest) => otherRequest.isBuild() && otherRequest.commitSet() == buildRequest.commitSet()))
                        continue;
                    break;
                case 'requiresBuild':
                    const requiresBuild = value.repositoriesToCheck.some((repository) => commitSet.requiresBuildForRepository(repository));
                    if (!requiresBuild)
                        continue;
                    break;
                }
                value = value.value;
            }
            properties[propertyName] = value;
        }
        properties[this._buildRequestPropertyName] = buildRequest.id();

        return properties;
    }

    _revisionSetFromCommitSetWithExclusionList(commitSet, exclusionList)
    {
        const revisionSet = {};
        for (let repository of commitSet.repositories()) {
            if (exclusionList.indexOf(repository.name()) >= 0)
                continue;
            const commit = commitSet.commitForRepository(repository);
            revisionSet[repository.name()] = {
                id: commit.id(),
                time: +commit.time(),
                repository: repository.name(),
                revision: commit.revision(),
            };
        }
        return revisionSet;
    }

    static _loadConfig(remote, config, builderNameToIDMap)
    {
        assert(builderNameToIDMap);
        const types = config['types'] || {};
        const builders = config['builders'] || {};

        assert(config.buildRequestArgument, 'buildRequestArgument must specify the name of the property used to store the build request ID');

        assert.strictEqual(typeof(config.repositoryGroups), 'object', 'repositoryGroups must specify a dictionary from the name to its definition');

        const repositoryGroups = {};
        for (const name in config.repositoryGroups)
            repositoryGroups[name] = this._parseRepositoryGroup(name, config.repositoryGroups[name]);

        const commonConfigurations = {
            repositoryGroups,
            workerArgument: config.workerArgument,
            buildRequestArgument: config.buildRequestArgument,
            platformArgument: config.platformArgument,
        };

        const syncerByBuilder = new Map;
        const ensureBuildbotSyncer = (builderInfo) => {
            let builderSyncer = syncerByBuilder.get(builderInfo.builder);
            if (!builderSyncer) {
                builderSyncer = new BuildbotSyncer(remote, builderInfo, commonConfigurations);
                syncerByBuilder.set(builderInfo.builder, builderSyncer);
            }
            return builderSyncer;
        }

        assert(Array.isArray(config['testConfigurations']), `The test configuration must be an array`);
        this._resolveBuildersWithPlatforms('test', config['testConfigurations'], builders, builderNameToIDMap).forEach((entry, configurationIndex) => {
            assert(Array.isArray(entry['types']), `The test configuration ${configurationIndex} does not specify "types" as an array`);
            for (const type of entry['types']) {
                const typeConfig = this._validateAndMergeConfig({}, entry.builderConfig);
                assert(types[type], `"${type}" is not a valid type in the configuration`);
                this._validateAndMergeConfig(typeConfig, types[type]);

                const testPath = typeConfig.test;
                const test = Test.findByPath(testPath);
                assert(test, `"${testPath.join('", "')}" is not a valid test path in the test configuration ${configurationIndex}`);

                ensureBuildbotSyncer(entry.builderConfig).addTestConfiguration(test, entry.platform, entry.supportedRepetitionTypes, typeConfig.properties);
            }
        });

        const buildConfigurations = config['buildConfigurations'];
        if (buildConfigurations) {
            assert(Array.isArray(buildConfigurations), `The test configuration must be an array`);
            this._resolveBuildersWithPlatforms('test', buildConfigurations, builders, builderNameToIDMap).forEach((entry, configurationIndex) => {
                const syncer = ensureBuildbotSyncer(entry.builderConfig);
                assert(!syncer.isTester(), `The build configuration ${configurationIndex} uses a tester: ${syncer.builderName()}`);
                syncer.addBuildConfiguration(entry.platform, entry.supportedRepetitionTypes, entry.builderConfig.properties);
            });
        }

        return Array.from(syncerByBuilder.values());
    }

    static _resolveBuildersWithPlatforms(configurationType, configurationList, builders, builderNameToIDMap)
    {
        const resolvedConfigurations = [];
        let configurationIndex = 0;
        for (const entry of configurationList) {
            configurationIndex++;
            assert(Array.isArray(entry['builders']), `The ${configurationType} configuration ${configurationIndex} does not specify "builders" as an array`);
            assert(Array.isArray(entry['platforms']), `The ${configurationType} configuration ${configurationIndex} does not specify "platforms" as an array`);
            assert(Array.isArray(entry['supportedRepetitionTypes']), `The ${configurationType} configuration ${configurationIndex} does not specify "supportedRepetitionTypes" as an array`);
            assert(entry['supportedRepetitionTypes'].every(TriggerableConfiguration.isValidRepetitionType.bind(TriggerableConfiguration)),
                `The ${configurationType} configuration ${configurationIndex} specifies invalid repetitionType in "supportedRepetitionTypes": ${entry['supportedRepetitionTypes']}`);
            for (const builderKey of entry['builders']) {
                const matchingBuilder = builders[builderKey];
                assert(matchingBuilder, `"${builderKey}" is not a valid builder in the configuration`);
                assert('builder' in matchingBuilder, `Builder ${builderKey} does not specify a buildbot builder name`);
                assert(matchingBuilder.builder in builderNameToIDMap, `Builder ${matchingBuilder.builder} not found in Buildbot configuration.`);
                assert(Array.isArray(matchingBuilder.supportedRepetitionTypes), `Builder ${matchingBuilder.builder} does not specify "supportedRepetitionTypes" as an array`);
                assert(matchingBuilder.supportedRepetitionTypes.every(TriggerableConfiguration.isValidRepetitionType.bind(TriggerableConfiguration)),
                    `Builder ${matchingBuilder.builder} specifies repetitionType in "supportedRepetitionTypes": ${matchingBuilder.supportedRepetitionTypes}`);
                matchingBuilder['builderID'] = builderNameToIDMap[matchingBuilder.builder];
                const builderConfig = this._validateAndMergeConfig({}, matchingBuilder);
                for (const platformName of entry['platforms']) {
                    const platform = Platform.findByName(platformName);
                    assert(platform, `${platformName} is not a valid platform name`);
                    resolvedConfigurations.push({types: entry.types, builderConfig, platform, supportedRepetitionTypes: entry['supportedRepetitionTypes']});
                }
            }
        }
        return resolvedConfigurations;
    }

    static _parseRepositoryGroup(name, group)
    {
        assert.strictEqual(typeof(group.repositories), 'object',
            `Repository group "${name}" does not specify a dictionary of repositories`);
        assert(!('description' in group) || typeof(group['description']) == 'string',
            `Repository group "${name}" have an invalid description`);
        assert([undefined, true, false].includes(group.acceptsRoots),
            `Repository group "${name}" contains invalid acceptsRoots value: ${JSON.stringify(group.acceptsRoots)}`);

        const repositoryByName = {};
        const parsedRepositoryList = [];
        const patchAcceptingRepositoryList = new Set;
        for (const repositoryName in group.repositories) {
            const options = group.repositories[repositoryName];
            const repository = Repository.findTopLevelByName(repositoryName);
            assert(repository, `"${repositoryName}" is not a valid repository name`);
            repositoryByName[repositoryName] = repository;
            assert.strictEqual(typeof(options), 'object', `"${repositoryName}" specifies a non-dictionary value`);
            assert([undefined, true, false].includes(options.acceptsPatch),
                `"${repositoryName}" contains invalid acceptsPatch value: ${JSON.stringify(options.acceptsPatch)}`);
            if (options.acceptsPatch)
                patchAcceptingRepositoryList.add(repository);
            repositoryByName[repositoryName] = repository;
            parsedRepositoryList.push({repository: repository.id(), acceptsPatch: options.acceptsPatch});
        }
        assert(parsedRepositoryList.length, `Repository group "${name}" does not specify any repository`);

        assert.strictEqual(typeof(group.testProperties), 'object', `Repository group "${name}" specifies the test configurations with an invalid type`);

        const resolveRepository = (repositoryName) => {
            const repository = repositoryByName[repositoryName];
            assert(repository, `Repository group "${name}" an invalid repository "${repositoryName}"`);
            return repository;
        }

        const testRepositories = new Set;
        let specifiesRoots = false;
        const testPropertiesTemplate = this._parseRepositoryGroupPropertyTemplate('test', name, group.testProperties, (type, value, condition) => {
            assert(type != 'patch', `Repository group "${name}" specifies a patch for "${value}" in the properties for testing`);
            switch (type) {
            case 'revision':
                const repository = resolveRepository(value);
                testRepositories.add(repository);
                return {type, repository};
            case 'roots':
                assert(group.acceptsRoots, `Repository group "${name}" specifies roots in a property but it does not accept roots`);
                specifiesRoots = true;
                return {type};
            case 'ifBuilt':
                assert('condition', 'condition must set if type is "ifBuilt"');
                return {type: 'conditional', condition: 'built', value, repositoryRequirement: condition.map(resolveRepository)};
            }
            return null;
        });
        assert(!group.acceptsRoots == !specifiesRoots,
            `Repository group "${name}" accepts roots but does not specify roots in testProperties`);
        assert.strictEqual(parsedRepositoryList.length, testRepositories.size,
            `Repository group "${name}" does not use some of the repositories listed in testing`);

        let buildPropertiesTemplate = null;
        if ('buildProperties' in group) {
            assert(group.acceptsRoots, `Repository group "${name}" specifies the properties for building but does not accept roots in testing`);
            const revisionRepositories = new Set;
            const patchRepositories = new Set;
            let hasOwnedRevisions = false;
            buildPropertiesTemplate = this._parseRepositoryGroupPropertyTemplate('build', name, group.buildProperties, (type, value, condition) => {
                assert(type != 'roots', `Repository group "${name}" specifies roots in the properties for building`);
                let repository = null;
                switch (type) {
                case 'patch':
                    repository = resolveRepository(value);
                    assert(patchAcceptingRepositoryList.has(repository), `Repository group "${name}" specifies a patch for "${value}" but it does not accept a patch`);
                    patchRepositories.add(repository);
                    return {type, repository};
                case 'revision':
                    repository = resolveRepository(value);
                    revisionRepositories.add(repository);
                    return {type, repository};
                case 'ownedRevisions':
                    hasOwnedRevisions = true;
                    return {type, ownerRepository: resolveRepository(value)};
                case 'ifRepositorySet':
                    assert(condition, 'condition must set if type is "ifRepositorySet"');
                    return {type: 'conditional', condition: 'requiresBuild', value, repositoriesToCheck: condition.map(resolveRepository)};
                }
                return null;
            });
            assert(patchAcceptingRepositoryList.size || hasOwnedRevisions, `Repository group "${name}" specifies the properties for building but does not accept any patches or need to build owned components`);
            for (const repository of patchRepositories)
                assert(revisionRepositories.has(repository), `Repository group "${name}" specifies a patch for "${repository.name()}" but does not specify a revision`);
            assert.strictEqual(patchAcceptingRepositoryList.size, patchRepositories.size,
                `Repository group "${name}" does not use some of the repositories listed in building a patch`);
        }

        return {
            name: group.name,
            description: group.description,
            acceptsRoots: group.acceptsRoots,
            testPropertiesTemplate: testPropertiesTemplate,
            buildPropertiesTemplate: buildPropertiesTemplate,
            repositoryList: parsedRepositoryList,
        };
    }

    static _parseRepositoryGroupPropertyTemplate(parsingMode, groupName, properties, makeOption)
    {
        const propertiesTemplate = {};
        for (const propertyName in properties) {
            let value = properties[propertyName];
            const isDictionary = typeof(value) == 'object';
            assert(isDictionary || typeof(value) == 'string' || typeof(value) == 'boolean',
                `Repository group "${groupName}" uses an invalid value "${value}" in property "${propertyName}"`);

            if (!isDictionary) {
                propertiesTemplate[propertyName] = value;
                continue;
            }

            const keys = Object.keys(value);
            assert(keys.length == 1 || keys.length == 2,
                `Repository group "${groupName}" specifies more than two types in property "${propertyName}": "${keys.join('", "')}"`);
            let type;
            let condition = null;
            let optionValue;
            if (keys.length == 2) {
                assert(keys.includes('value'), `Repository group "${groupName}" with two types in property "${propertyName}": "${keys.join('", "')}" should contains 'value' as one type`);
                type = keys.find((key) => key != 'value');
                optionValue = value.value;
                condition = value[type];
            }
            else {
                type = keys[0];
                optionValue = value[type];
            }
            const option = makeOption(type, optionValue, condition);
            assert(option, `Repository group "${groupName}" specifies an invalid type "${type}" in property "${propertyName}"`);
            propertiesTemplate[propertyName] = option;
        }
        return propertiesTemplate;
    }

    static _validateAndMergeConfig(config, valuesToMerge, excludedProperty)
    {
        for (const name in valuesToMerge) {
            const value = valuesToMerge[name];
            if (name == excludedProperty)
                continue;

            switch (name) {
            case 'properties': // Fallthrough
                assert.strictEqual(typeof(value), 'object', 'Build properties should be a dictionary');
                if (!config['properties'])
                    config['properties'] = {};
                const properties = config['properties'];
                for (const name in value) {
                    assert.strictEqual(typeof(value[name]), 'string', `Build properties "${name}" specifies a non-string value of type "${typeof(value)}"`);
                    properties[name] = value[name];
                }
                break;
            case 'supportedRepetitionTypes':
            case 'test': // Fallthrough
            case 'workerList': // Fallthrough
                assert(value instanceof Array, `${name} should be an array`);
                assert(value.every(function (part) { return typeof part == 'string'; }), `${name} should be an array of strings`);
                config[name] = value.slice();
                break;
            case 'builder': // Fallthrough
                assert.strictEqual(typeof(value), 'string', `${name} should be of string type`);
                config[name] = value;
                break;
            case 'builderID':
                assert(value, 'builderID should not be undefined.');
                config[name] = value;
                break;
            default:
                assert(false, `Unrecognized parameter "${name}"`);
            }
        }
        return config;
    }

}

if (typeof module != 'undefined') {
    module.exports.BuildbotSyncer = BuildbotSyncer;
    module.exports.BuildbotBuildEntry = BuildbotBuildEntry;
}
