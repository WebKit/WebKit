'use strict';

let assert = require('assert');

class OSBuildFetcher {

    constructor(osConfig, remoteAPI, slaveAuth, subprocess, logger)
    {
        this._osConfig = osConfig;
        this._reportedRevisions = new Set();
        this._logger = logger;
        this._slaveAuth = slaveAuth;
        this._remoteAPI = remoteAPI;
        this._subprocess = subprocess;
    }

    fetchAndReportNewBuilds()
    {
        return this._fetchAvailableBuilds().then((results) =>{
            return this._submitCommits(results);
        });
    }

    _fetchAvailableBuilds()
    {
        const config = this._osConfig;
        const repositoryName = config['name'];
        let customCommands = config['customCommands'];

        return Promise.all(customCommands.map((command) => {
            assert(command['minRevision']);
            assert(command['maxRevision']);
            const minRevisionOrder = this._computeOrder(command['minRevision']);
            const maxRevisionOrder = this._computeOrder(command['maxRevision']);

            let fetchCommitsPromise = this._remoteAPI.getJSONWithStatus(`/api/commits/${escape(repositoryName)}/last-reported?from=${minRevisionOrder}&to=${maxRevisionOrder}`).then((result) => {
                const minOrder = result['commits'].length == 1 ? parseInt(result['commits'][0]['order']) : 0;
                return this._commitsForAvailableBuilds(repositoryName, command['command'], command['linesToIgnore'], minOrder);
            })

            if ('subCommitCommand' in command)
                fetchCommitsPromise = fetchCommitsPromise.then((commits) => this._addSubCommitsForBuild(commits, command['subCommitCommand']));

            return fetchCommitsPromise;
        })).then(results => {
            return Array.prototype.concat.apply([], results);
        });
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

    _commitsForAvailableBuilds(repository, command, linesToIgnore, minOrder)
    {
        return this._subprocess.execute(command).then((output) => {
            let lines = output.split('\n');
            if (linesToIgnore){
                const regex = new RegExp(linesToIgnore);
                lines = lines.filter(function(line) {return !regex.exec(line);});
            }
            return lines.map(revision => ({repository, revision, 'order': this._computeOrder(revision)}))
                .filter(commit => commit['order'] > minOrder);
        });
    }

    _addSubCommitsForBuild(commits, command)
    {
        return commits.reduce((promise, commit) => {
            return promise.then(() => {
                return this._subprocess.execute(command.concat(commit['revision']));
            }).then((subCommitOutput) => {
                const subCommits = JSON.parse(subCommitOutput);
                for (let repositoryName in subCommits) {
                    const subCommit = subCommits[repositoryName];
                    assert(subCommit['revision']);
                }
                commit['subCommits'] = subCommits;
            });
        }, Promise.resolve()).then(() => commits);
    }

    _submitCommits(commits)
    {
        const commitsToReport = {"slaveName": this._slaveAuth['name'], "slavePassword": this._slaveAuth['password'], 'commits': commits};
        return this._remoteAPI.postJSONWithStatus('/api/report-commits/', commitsToReport);
    }
}
if (typeof module != 'undefined')
    module.exports.OSBuildFetcher = OSBuildFetcher;
