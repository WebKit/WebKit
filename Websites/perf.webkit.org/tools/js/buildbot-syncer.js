'use strict';

let assert = require('assert');

require('./v3-models.js');

class BuildbotBuildEntry {
    constructor(syncer, rawData)
    {
        assert.equal(syncer.builderName(), rawData['builderName']);

        this._syncer = syncer;
        this._slaveName = null;
        this._buildRequestId = null;
        this._isInProgress = rawData['currentStep'] || (rawData['times'] && !rawData['times'][1]);
        this._buildNumber = rawData['number'];

        for (let propertyTuple of (rawData['properties'] || [])) {
            // e.g. ['build_request_id', '16733', 'Force Build Form']
            const name = propertyTuple[0];
            const value = propertyTuple[1];
            if (name == syncer._slavePropertyName)
                this._slaveName = value;
            else if (name == syncer._buildRequestPropertyName)
                this._buildRequestId = value;
        }
    }

    syncer() { return this._syncer; }
    buildNumber() { return this._buildNumber; }
    slaveName() { return this._slaveName; }
    buildRequestId() { return this._buildRequestId; }
    isPending() { return typeof(this._buildNumber) != 'number'; }
    isInProgress() { return this._isInProgress; }
    hasFinished() { return !this.isPending() && !this.isInProgress(); }
    url() { return this.isPending() ? this._syncer.url() : this._syncer.urlForBuildNumber(this._buildNumber); }

    buildRequestStatusIfUpdateIsNeeded(request)
    {
        assert.equal(request.id(), this._buildRequestId);
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
        this._testConfigurations = [];
        this._repositoryGroups = commonConfigurations.repositoryGroups;
        this._slavePropertyName = commonConfigurations.slaveArgument;
        this._buildRequestPropertyName = commonConfigurations.buildRequestArgument;
        this._builderName = object.builder;
        this._slaveList = object.slaveList;
        this._entryList = null;
        this._slavesWithNewRequests = new Set;
    }

    builderName() { return this._builderName; }

    addTestConfiguration(test, platform, propertiesTemplate)
    {
        assert(test instanceof Test);
        assert(platform instanceof Platform);
        this._testConfigurations.push({test, platform, propertiesTemplate});
    }
    testConfigurations() { return this._testConfigurations; }
    repositoryGroups() { return this._repositoryGroups; }

    matchesConfiguration(request)
    {
        return this._testConfigurations.some((config) => config.platform == request.platform() && config.test == request.test());
    }

    scheduleRequest(newRequest, slaveName)
    {
        assert(!this._slavesWithNewRequests.has(slaveName));
        let properties = this._propertiesForBuildRequest(newRequest);

        assert.equal(!this._slavePropertyName, !slaveName);
        if (this._slavePropertyName)
            properties[this._slavePropertyName] = slaveName;

        this._slavesWithNewRequests.add(slaveName);
        return this._remote.postFormUrlencodedData(this.pathForForceBuild(), properties);
    }

    scheduleRequestInGroupIfAvailable(newRequest, slaveName)
    {
        assert(newRequest instanceof BuildRequest);

        if (!this.matchesConfiguration(newRequest))
            return null;

        let hasPendingBuildsWithoutSlaveNameSpecified = false;
        let usedSlaves = new Set;
        for (let entry of this._entryList) {
            if (entry.isPending()) {
                if (!entry.slaveName())
                    hasPendingBuildsWithoutSlaveNameSpecified = true;
                usedSlaves.add(entry.slaveName());
            }
        }

        if (!this._slaveList || hasPendingBuildsWithoutSlaveNameSpecified) {
            if (usedSlaves.size || this._slavesWithNewRequests.size)
                return null;
            return this.scheduleRequest(newRequest, null);
        }

        if (slaveName) {
            if (!usedSlaves.has(slaveName) && !this._slavesWithNewRequests.has(slaveName))
                return this.scheduleRequest(newRequest, slaveName);
            return null;
        }

        for (let slaveName of this._slaveList) {
            if (!usedSlaves.has(slaveName) && !this._slavesWithNewRequests.has(slaveName))
                return this.scheduleRequest(newRequest, slaveName);
        }

        return null;
    }

    pullBuildbot(count)
    {
        return this._remote.getJSON(this.pathForPendingBuildsJSON()).then((content) => {
            let pendingEntries = content.map((entry) => new BuildbotBuildEntry(this, entry));
            return this._pullRecentBuilds(count).then((entries) => {
                let entryByRequest = {};

                for (let entry of pendingEntries)
                    entryByRequest[entry.buildRequestId()] = entry;

                for (let entry of entries)
                    entryByRequest[entry.buildRequestId()] = entry;

                let entryList = [];
                for (let id in entryByRequest)
                    entryList.push(entryByRequest[id]);

                this._entryList = entryList;
                this._slavesWithNewRequests.clear();

                return entryList;
            });
        });
    }

    _pullRecentBuilds(count)
    {
        if (!count)
            return Promise.resolve([]);

        let selectedBuilds = new Array(count);
        for (let i = 0; i < count; i++)
            selectedBuilds[i] = -i - 1;

        return this._remote.getJSON(this.pathForBuildJSON(selectedBuilds)).then((content) => {
            const entries = [];
            for (let index of selectedBuilds) {
                const entry = content[index];
                if (entry && !entry['error'])
                    entries.push(new BuildbotBuildEntry(this, entry));
            }
            return entries;
        });
    }

    pathForPendingBuildsJSON() { return `/json/builders/${escape(this._builderName)}/pendingBuilds`; }
    pathForBuildJSON(selectedBuilds)
    {
        return `/json/builders/${escape(this._builderName)}/builds/?` + selectedBuilds.map((number) => 'select=' + number).join('&');
    }
    pathForForceBuild() { return `/builders/${escape(this._builderName)}/force`; }

    url() { return this._remote.url(`/builders/${escape(this._builderName)}/`); }
    urlForBuildNumber(number) { return this._remote.url(`/builders/${escape(this._builderName)}/builds/${number}`); }

    _propertiesForBuildRequest(buildRequest)
    {
        assert(buildRequest instanceof BuildRequest);

        const commitSet = buildRequest.commitSet();
        assert(commitSet instanceof CommitSet);

        const repositoryByName = {};
        for (let repository of commitSet.repositories())
            repositoryByName[repository.name()] = repository;

        const matchingConfiguration = this._testConfigurations.find((config) => config.platform == buildRequest.platform() && config.test == buildRequest.test());
        assert(matchingConfiguration, `Build request ${buildRequest.id()} does not match a configuration in the builder "${this._builderName}"`);
        const propertiesTemplate = matchingConfiguration.propertiesTemplate;

        const repositoryGroup = buildRequest.repositoryGroup();
        assert(repositoryGroup.accepts(commitSet), `Build request ${buildRequest.id()} does not specify a commit set accepted by the repository group ${repositoryGroup.id()}`);

        const repositoryGroupConfiguration = this._repositoryGroups[repositoryGroup.name()];
        assert(repositoryGroupConfiguration, `Build request ${buildRequest.id()} uses an unsupported repository group "${repositoryGroup.name()}"`);

        const properties = {};
        for (let propertyName in propertiesTemplate)
            properties[propertyName] = propertiesTemplate[propertyName];

        const repositoryGroupTemplate = repositoryGroupConfiguration.propertiesTemplate;
        for (let propertyName in repositoryGroupTemplate) {
            const value = repositoryGroupTemplate[propertyName];
            properties[propertyName] = value instanceof Repository ? commitSet.revisionForRepository(value) : value;
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

    static _loadConfig(remote, config)
    {
        const types = config['types'] || {};
        const builders = config['builders'] || {};

        assert(config.buildRequestArgument, 'buildRequestArgument must specify the name of the property used to store the build request ID');

        assert.equal(typeof(config.repositoryGroups), 'object', 'repositoryGroups must specify a dictionary from the name to its definition');

        const repositoryGroups = {};
        for (const name in config.repositoryGroups)
            repositoryGroups[name] = this._parseRepositoryGroup(name, config.repositoryGroups[name]);

        const commonConfigurations = {
            repositoryGroups,
            slaveArgument: config.slaveArgument,
            buildRequestArgument: config.buildRequestArgument,
        };

        let syncerByBuilder = new Map;
        const expandedConfigurations = [];
        for (let entry of config['configurations']) {
            for (const expandedConfig of this._expandTypesAndPlatforms(entry))
                expandedConfigurations.push(expandedConfig);
        }

        for (let entry of expandedConfigurations) {
            const mergedConfig = {};
            this._validateAndMergeConfig(mergedConfig, entry);

            if ('type' in mergedConfig) {
                const type = mergedConfig['type'];
                assert(type, `${type} is not a valid type in the configuration`);
                this._validateAndMergeConfig(mergedConfig, types[type]);
            }

            const builder = entry['builder'];
            if (builders[builder])
                this._validateAndMergeConfig(mergedConfig, builders[builder]);

            this._createTestConfiguration(remote, syncerByBuilder, mergedConfig, commonConfigurations);
        }

        return Array.from(syncerByBuilder.values());
    }

    static _parseRepositoryGroup(name, group)
    {
        assert.equal(typeof(group.repositories), 'object',
            `Repository group "${name}" does not specify a dictionary of repositories`);
        assert(!('description' in group) || typeof(group['description']) == 'string',
            `Repository group "${name}" have an invalid description`);
        assert.equal(typeof(group.properties), 'object', `Repository group "${name}" specifies an invalid dictionary of properties`);
        assert([undefined, true, false].includes(group.acceptsRoots),
            `Repository group "${name}" contains invalid acceptsRoots value: ${group.acceptsRoots}`);

        const repositoryByName = {};
        const parsedRepositoryList = [];
        for (const repositoryName in group.repositories) {
            const options = group.repositories[repositoryName];
            const repository = Repository.findTopLevelByName(repositoryName);
            assert(repository, `"${repositoryName}" is not a valid repository name`);
            repositoryByName[repositoryName] = repository;
            assert.equal(typeof(options), 'object', `"${repositoryName}" does not specify a valid option`);
            assert([undefined, true, false].includes(options.acceptsPatch),
                `"${repositoryName}" contains invalid acceptsPatch value: ${options.acceptsPatch}`);
            repositoryByName[repositoryName] = repository;
            parsedRepositoryList.push({repository: repository.id(), acceptsPatch: options.acceptsPatch});
        }

        const propertiesTemplate = {};
        const usedRepositories = [];
        for (const propertyName in group.properties) {
            let value = group.properties[propertyName];
            const match = value.match(/^<(.+)>$/);
            if (match) {
                const repositoryName = match[1];
                value = repositoryByName[repositoryName];
                assert(value, `Repository group "${name}" uses "${repositoryName}" in its property but does not list in the list of repositories`);
                usedRepositories.push(value);
            }
            propertiesTemplate[propertyName] = value;
        }
        assert(parsedRepositoryList.length, `Repository group "${name}" does not specify any repository`);
        assert.equal(parsedRepositoryList.length, usedRepositories.length,
            `Repository group "${name}" does not use some of the repositories listed`);
        return {
            name: group.name,
            description: group.description,
            acceptsRoots: group.acceptsRoots,
            propertiesTemplate,
            repositoryList: parsedRepositoryList,
        };
    }

    static _expandTypesAndPlatforms(unresolvedConfig)
    {
        const typeExpanded = [];
        if ('types' in unresolvedConfig) {
            for (let type of unresolvedConfig['types'])
                typeExpanded.push(this._validateAndMergeConfig({'type': type}, unresolvedConfig, 'types'));
        } else
            typeExpanded.push(unresolvedConfig);

        const configurations = [];
        for (let config of typeExpanded) {
            if ('platforms' in config) {
                for (let platform of config['platforms'])
                    configurations.push(this._validateAndMergeConfig({'platform': platform}, config, 'platforms'));
            } else
                configurations.push(config);
        }

        return configurations;
    }

    static _createTestConfiguration(remote, syncerByBuilder, newConfig, commonConfigurations)
    {
        assert('platform' in newConfig, 'configuration must specify a platform');
        assert('test' in newConfig, 'configuration must specify a test');
        assert('builder' in newConfig, 'configuration must specify a builder');

        const test = Test.findByPath(newConfig.test);
        assert(test, `${newConfig.test} is not a valid test path`);

        const platform = Platform.findByName(newConfig.platform);
        assert(platform, `${newConfig.platform} is not a valid platform name`);

        let syncer = syncerByBuilder.get(newConfig.builder);
        if (!syncer) {
            syncer = new BuildbotSyncer(remote, newConfig, commonConfigurations);
            syncerByBuilder.set(newConfig.builder, syncer);
        }
        syncer.addTestConfiguration(test, platform, newConfig.properties || {});
    }

    static _validateAndMergeConfig(config, valuesToMerge, excludedProperty)
    {
        for (const name in valuesToMerge) {
            const value = valuesToMerge[name];
            if (name == excludedProperty)
                continue;

            switch (name) {
            case 'properties': // Fallthrough
                assert.equal(typeof(value), 'object', 'properties should be a dictionary');
                if (!config['properties'])
                    config['properties'] = {};
                const properties = config['properties'];
                for (const name in value) {
                    assert.equal(typeof(value[name]), 'string', 'A argument value must be a string');
                    properties[name] = value[name];
                }
                break;
            case 'test': // Fallthrough
            case 'slaveList': // Fallthrough
            case 'platforms': // Fallthrough
            case 'types':
                assert(value instanceof Array, `${name} should be an array`);
                assert(value.every(function (part) { return typeof part == 'string'; }), `${name} should be an array of strings`);
                config[name] = value.slice();
                break;
            case 'type': // Fallthrough
            case 'builder': // Fallthrough
            case 'platform': // Fallthrough
                assert.equal(typeof(value), 'string', `${name} should be of string type`);
                config[name] = value;
                break;
            default:
                assert(false, `Unrecognized parameter ${name}`);
            }
        }
        return config;
    }

}

if (typeof module != 'undefined') {
    module.exports.BuildbotSyncer = BuildbotSyncer;
    module.exports.BuildbotBuildEntry = BuildbotBuildEntry;
}
