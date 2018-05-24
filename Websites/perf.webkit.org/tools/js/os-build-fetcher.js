'use strict';

let assert = require('assert');

function mapInSerialPromiseChain(list, callback)
{
    const results = [];
    return list.reduce((chainedPromise, item) => {
        return chainedPromise.then(() => callback(item)).then((result) => results.push(result));
    }, Promise.resolve()).then(() => results);
}

class OSBuildFetcher {

    constructor(osConfig, remoteAPI, slaveAuth, subprocess, logger)
    {
        this._osConfig = osConfig;
        this._logger = logger;
        this._slaveAuth = slaveAuth;
        this._remoteAPI = remoteAPI;
        this._subprocess = subprocess;
        this._maxSubmitCount = osConfig['maxSubmitCount'] || 20;
    }

    static fetchAndReportAllInOrder(fetcherList)
    {
        return mapInSerialPromiseChain(fetcherList, (fetcher) => fetcher.fetchAndReportNewBuilds());
    }

    fetchAndReportNewBuilds()
    {
        return this._fetchAvailableBuilds().then((results) => {
            this._logger.log(`Submitting ${results.length} builds for ${this._osConfig['name']}`);
            if (results.length == 0)
                return this._submitCommits(results);

            const splittedResults = [];
            for (let startIndex = 0; startIndex < results.length; startIndex += this._maxSubmitCount)
                splittedResults.push(results.slice(startIndex, startIndex + this._maxSubmitCount));

            return mapInSerialPromiseChain(splittedResults, this._submitCommits.bind(this)).then((responses) => {
                assert(responses.every((response) => response['status'] == 'OK'));
                assert(responses.length > 0);
                return responses[0];
            });
        });
    }

    _fetchAvailableBuilds()
    {
        const config = this._osConfig;
        const repositoryName = config['name'];
        let customCommands = config['customCommands'];

        return mapInSerialPromiseChain(customCommands, (command) => {
            assert(command['minRevision']);
            assert(command['maxRevision']);
            const minRevisionOrder = this._computeOrder(command['minRevision']);
            const maxRevisionOrder = this._computeOrder(command['maxRevision']);

            const url = `/api/commits/${escape(repositoryName)}/last-reported?from=${minRevisionOrder}&to=${maxRevisionOrder}`;

            return this._remoteAPI.getJSONWithStatus(url).then((result) => {
                const minOrder = result['commits'].length == 1 ? parseInt(result['commits'][0]['order']) + 1 : minRevisionOrder;
                return this._commitsForAvailableBuilds(repositoryName, command['command'], command['linesToIgnore'], minOrder, maxRevisionOrder);
            }).then((commits) => {
                const label = 'name' in command ? `"${command['name']}"` : `"${command['minRevision']}" to "${command['maxRevision']}"`;
                this._logger.log(`Found ${commits.length} builds for ${label}`);

                if ('ownedCommitCommand' in command) {
                    this._logger.log(`Resolving ownedCommits for ${label}`);
                    return this._addOwnedCommitsForBuild(commits, command['ownedCommitCommand']);
                }

                return commits;
            });
        }).then((results) => [].concat(...results));
    }

    _computeOrder(revision)
    {
        const buildNameRegex = /(\d+)([a-zA-Z])(\d+)([a-zA-Z]*)$/;
        const match = buildNameRegex.exec(revision);
        assert(match);
        const major = parseInt(match[1]);
        const kind = match[2].toUpperCase().charCodeAt(0) - "A".charCodeAt(0);
        const minor = parseInt(match[3]);
        const variant = match[4] ? match[4].toUpperCase().charCodeAt(0) - "A".charCodeAt(0) + 1 : 0;
        return ((major * 100 + kind) * 10000 + minor) * 100 + variant;
    }

    _commitsForAvailableBuilds(repository, command, linesToIgnore, minOrder, maxOrder)
    {
        return this._subprocess.execute(command).then((output) => {
            let lines = output.split('\n');
            if (linesToIgnore){
                const regex = new RegExp(linesToIgnore);
                lines = lines.filter((line) => !regex.exec(line));
            }
            return lines.map((revision) => ({repository, revision, 'order': this._computeOrder(revision)}))
                .filter((commit) => commit['order'] >= minOrder && commit['order'] <= maxOrder);
        });
    }

    _addOwnedCommitsForBuild(commits, command)
    {
        return mapInSerialPromiseChain(commits, (commit) => {
            return this._subprocess.execute(command.concat(commit['revision'])).then((ownedCommitOutput) => {
                const ownedCommits = JSON.parse(ownedCommitOutput);
                this._logger.log(`Got ${Object.keys((ownedCommits)).length} owned commits for "${commit['revision']}"`);
                for (let repositoryName in ownedCommits) {
                    const ownedCommit = ownedCommits[repositoryName];
                    assert.deepEqual(Object.keys(ownedCommit), ['revision']);
                    assert(typeof(ownedCommit['revision']) == 'string');
                }
                commit['ownedCommits'] = ownedCommits;
                return commit;
            });
        });
    }

    _submitCommits(commits)
    {
        const commitsToReport = {"slaveName": this._slaveAuth['name'], "slavePassword": this._slaveAuth['password'], 'commits': commits};
        return this._remoteAPI.postJSONWithStatus('/api/report-commits/', commitsToReport);
    }
}

if (typeof module != 'undefined')
    module.exports.OSBuildFetcher = OSBuildFetcher;
