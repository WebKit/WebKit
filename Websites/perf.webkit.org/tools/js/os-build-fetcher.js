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

    static async fetchReportAndUpdateCommits(fetcherList)
    {
        for (const fetcher of fetcherList)
            await fetcher.fetchReportAndUpdateBuilds();
    }

    async fetchReportAndUpdateBuilds()
    {
        const {newCommitsToReport, commitsToUpdate} = await this._fetchAvailableBuilds();

        this._logger.log(`Submitting ${newCommitsToReport.length} builds for ${this._osConfig['name']}`);
        await this._reportCommits(newCommitsToReport, true);

        this._logger.log(`Updating ${commitsToUpdate.length} builds for ${this._osConfig['name']}`);
        await this._reportCommits(commitsToUpdate, false);
    }

    async _fetchAvailableBuilds()
    {
        const config = this._osConfig;
        const repositoryName = config['name'];
        const newCommitsToReport = [];
        const commitsToUpdate = [];
        const  customCommands = config['customCommands'];

        for (const command of customCommands) {
            assert(command['minRevision']);
            assert(command['maxRevision']);
            const minRevisionOrder = this._computeOrder(command['minRevision']);
            const maxRevisionOrder = this._computeOrder(command['maxRevision']);

            const url = `/api/commits/${escape(repositoryName)}/last-reported?from=${minRevisionOrder}&to=${maxRevisionOrder}`;
            const result = await this._remoteAPI.getJSONWithStatus(url);
            const minOrder = result['commits'].length == 1 ? parseInt(result['commits'][0]['order']) + 1 : minRevisionOrder;

            const commitInfo = await this._commitsForAvailableBuilds(command['command'], command['linesToIgnore']);
            const commits = this._commitsWithinRange(commitInfo.allRevisions, repositoryName, minOrder, maxRevisionOrder);

            const label = 'name' in command ? `"${command['name']}"` : `"${command['minRevision']}" to "${command['maxRevision']}"`;
            this._logger.log(`Found ${commits.length} builds for ${label}`);
            if ('ownedCommitCommand' in command) {
                this._logger.log(`Resolving ownedCommits for ${label}`);
                await this._addOwnedCommitsForBuild(commits, command['ownedCommitCommand']);
            }
            newCommitsToReport.push(...commits);

            for (const [revision, testability] of Object.entries(commitInfo.commitsWithTestability)) {
                const order = this._computeOrder(revision);
                if (order > maxRevisionOrder || order < minRevisionOrder)
                    continue;
                commitsToUpdate.push({repository: repositoryName, revision, testability});
            }
        }
        return {newCommitsToReport, commitsToUpdate};
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

    async _commitsForAvailableBuilds(command, linesToIgnore)
    {
        const output = await this._subprocess.execute(command);
        try {
            return JSON.parse(output);
        } catch (error) {
            if (!(error instanceof SyntaxError))
                throw error;
            let lines = output.split('\n');
            if (linesToIgnore) {
                const regex = new RegExp(linesToIgnore);
                lines = lines.filter((line) => !regex.exec(line));
            }
            return {allRevisions: lines, commitsWithTestability: {}};
        }
    }

    _commitsWithinRange(revisions, repository, minOrder, maxOrder)
    {
        return revisions.map((revision) => ({repository, revision, 'order': this._computeOrder(revision)}))
            .filter((commit) => commit['order'] >= minOrder && commit['order'] <= maxOrder);
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

    async _reportCommits(commitsToPost, insert)
    {
        if (!commitsToPost.length)
            return;

        for(let commitsToSubmit = commitsToPost.splice(0, this._maxSubmitCount); commitsToSubmit.length; commitsToSubmit = commitsToPost.splice(0, this._maxSubmitCount)) {
            const report = {"slaveName": this._slaveAuth['name'], "slavePassword": this._slaveAuth['password'], 'commits': commitsToSubmit, insert};
            const response = await this._remoteAPI.postJSONWithStatus('/api/report-commits/', report);
            assert(response['status'] === 'OK');
        }
    }
}

if (typeof module != 'undefined')
    module.exports.OSBuildFetcher = OSBuildFetcher;
