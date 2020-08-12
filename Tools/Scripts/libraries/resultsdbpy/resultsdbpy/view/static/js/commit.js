// Copyright (C) 2019, 2020 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import {ErrorDisplay, escapeHTML, paramsToQuery, queryToParams} from '/assets/js/common.js';

const TIMESTAMP_TO_UUID_MULTIPLIER = 100;

function CommitTable(commits, repositoryIds = [], oneLine = false) {
    var rows = []
    if (!commits || !commits.length)
        return ErrorDisplay({error: 'No Commits Found', description: 'Cannot display the commit table, no commits found.'});
    var sorted_commits = commits.sort((a, b) => {
        return (b.timestamp * TIMESTAMP_TO_UUID_MULTIPLIER + b.order) - (a.timestamp * TIMESTAMP_TO_UUID_MULTIPLIER + a.order);
    });

    var repositories_with_commits = []
    if (!repositoryIds || !repositoryIds.length) {
        repositoryIds = [];
        commits.forEach((commit) => {
            if (repositoryIds.indexOf(commit.repository_id) < 0)
                repositoryIds.push(commit.repository_id);
        });
        repositoryIds.sort();
    }

    var lastValidCellForIndex = [0];
    var rows = [[{rowspan: 1, timestamp: sorted_commits[0].timestamp}]];

    for (const repository of repositoryIds) {
        for (let index = 0; index < sorted_commits.length; ++index) {
            if (sorted_commits[index].repository_id === repository) {
                repositories_with_commits.push(repository);

                lastValidCellForIndex.push(0);
                rows[0].push({rowspan: 1, commit: sorted_commits[index]});
                sorted_commits.splice(index, 1);
                break;
            }
        }
    }

    for (const commit of sorted_commits) {
        let repo_index = repositories_with_commits.indexOf(commit.repository_id);
        if (repo_index < 0)
            continue;
        if (!rows[rows.length - 1][repo_index + 1] && commit.timestamp === rows[lastValidCellForIndex[0]][0].timestamp) {
            rows[lastValidCellForIndex[repo_index + 1]][repo_index + 1].rowspan -= 1;
            rows[rows.length - 1][repo_index + 1] = {rowspan: 1, commit: commit};
            lastValidCellForIndex[repo_index + 1] = rows.length - 1;
            continue;
        }

        rows.push(new Array(repositories_with_commits.length + 1).fill(null));
        if (commit.timestamp === rows[lastValidCellForIndex[0]][0].timestamp)
            rows[lastValidCellForIndex[0]][0].rowspan += 1
        else {
            rows[rows.length - 1][0] = {rowspan: 1, timestamp: commit.timestamp};
            lastValidCellForIndex[0] = rows.length - 1;
        }
        rows[rows.length - 1][repo_index + 1] = {rowspan: 1, commit: commit};
        lastValidCellForIndex[repo_index + 1] = rows.length - 1;
        for (let index = 0; index < repositories_with_commits.length; ++index) {
            if (index != repo_index)
                rows[lastValidCellForIndex[index + 1]][index + 1].rowspan += 1;
        }
    }

    return `<table class="table full-width commit-table">
        <thead>
            <th></th>
            ${repositories_with_commits.map(id => `<th>${id}</th>`).join('')}
        </thead>
        <tbody>
            ${rows.map(row =>`<tr>
                ${row.map(function(cell) {
                    if (!cell)
                        return '';
                    if (cell.timestamp)
                        return `<td rowspan="${cell.rowspan}">${new Date(cell.timestamp * 1000).toLocaleString()}</td>`;

                    let commitArgs = paramsToQuery({
                        repository_id: [cell.commit.repository_id],
                        branch: [cell.commit.branch],
                        id: [cell.commit.id],
                    });
                    let investigateArgs = {id: [cell.commit.id]};
                    if (!['master', 'trunk'].includes(cell.commit.branch))
                        investigateArgs.branch = [cell.commit.branch];

                    return `<td rowspan="${cell.rowspan}">
                        <a href="/commit?${commitArgs}">${cell.commit.id}</a> <br>
                        Branch: ${cell.commit.branch} <br>
                        Committer: ${escapeHTML(cell.commit.committer)} <br>
                        <a href="/commit/info?${commitArgs}">More Info</a><br>
                        <a href="/investigate?${paramsToQuery(investigateArgs)}">Test results for commit</a>
                        ${function() {
                            if (!cell.commit.message)
                                return '';
                            if (oneLine)
                                return `<br><div>${escapeHTML(cell.commit.message.split('\n')[0])}</div>`;
                            return `<br><div>${escapeHTML(cell.commit.message)}</div>`;
                        }()}
                    </td>`;
                }).join('')}
            </tr>`).join('')}
        </tbody>
    </table>`;
}

class Commit {
    constructor(json) {
        this.branch = json.branch;
        this.committer = json.committer;
        this.id = json.id;
        this.message = json.message;
        this.order = json.order;
        this.repository_id = json.repository_id;
        this.timestamp = json.timestamp;
        this.uuid = this.timestamp * TIMESTAMP_TO_UUID_MULTIPLIER + this.order;
    }
    compare(commit) {
        return this.uuid - commit.uuid;
    }
};

class _CommitBank {
    constructor() {
        this.commits = [];

        const params = queryToParams(document.URL.split('?')[1]);

        this._branches = new Set(params.branch);
        this._repositories = new Set(params.repository_id);
        this._beginUuid = null;
        this._endUuid = null;
        this.callbacks = [];

    }
    latest(params) {
        this._branches = new Set(params.branch);
        this._repositories = new Set(params.repository_id);
        this._beginUuid = null;
        this._endUuid = null;

        // Only provide the latest commits
        const self = this;
        return new Promise(function(resolve, reject) {
            fetch(Object.keys(params).length ? `api/commits?${paramsToQuery(params)}` : 'api/commits').then(response => {
                response.json().then(json => {
                    self._beginUuid = Math.floor(Date.now() / 1000);
                    self._endUuid = 0;

                    const oldestIdForRepo = {};
                    const candidates = [];
                    json.forEach(datum =>{
                        const commit = new Commit(datum);
                        candidates.push(commit);

                        if (oldestIdForRepo[commit.repository_id])
                            oldestIdForRepo[commit.repository_id] = Math.min(commit.uuid, oldestIdForRepo[commit.repository_id]);
                        else
                            oldestIdForRepo[commit.repository_id] = commit.uuid;
                    });

                    let oldestValidUuid = 0;
                    Object.keys(oldestIdForRepo).forEach(id => {
                        oldestValidUuid = Math.max(oldestValidUuid, id);
                    });

                    candidates.forEach(commit => {
                        if (commit.uuid <= oldestValidUuid)
                            return;
                        self.commits.push(commit);
                        self._beginUuid = Math.min(commit.uuid, self._beginUuid);
                        self._endUuid = Math.max(commit.uuid, self._endUuid);
                    });

                    if (self._beginUuid > self._endUuid) {
                        self._beginUuid = null;
                        self._endUuid = null;
                    }

                    resolve(this);
                });
            }).catch(error => {
                // If the load fails, log the error and continue
                console.error(JSON.stringify(error, null, 4));
                reject(error);
            });
        });
    }
    commitByUuid(uuid) {
        let begin = 0;
        let end = this.commits.length - 1;
        while (begin <= end) {
            const mid = Math.ceil((begin + end) / 2);
            const candidate = this.commits[mid];
            if (candidate.uuid === uuid)
                return candidate;
            if (candidate.uuid < uuid)
                begin = mid + 1;
            else
                end = mid - 1;
        }
        return null;
    }
    commitsDuring(minUuid, maxUuid = 0) {
        let commits = [];
        let begin = 0;
        let end = this.commits.length - 1;
        let counter = 0;
        let index = this.commits.length - 1;

        if (maxUuid == 0)
            maxUuid = minUuid;
        if (maxUuid < minUuid) {
            console.error('Invalid uuid range');
            return commits;
        }

        while (begin <= end) {
            counter = Math.ceil((begin + end) / 2);
            const candidate = this.commits[counter];
            if (minUuid <= candidate.uuid && candidate.uuid <= maxUuid) {
                index = counter - 1;
                break;
            }

            if (candidate.uuid < minUuid)
                begin = counter + 1;
            else
                end = counter - 1;
        }
        while (counter < this.commits.length) {
            const candidate = this.commits[counter];
            if (minUuid > candidate.uuid || candidate.uuid > maxUuid)
                break
            commits.push(candidate);
            ++counter;
        }

        let repositories = new Set();
        if (commits.length)
            repositories.add(commits[0].repository_id);

        while (index >= 0) {
            if (repositories.has(this.commits[index].repository_id)) {
                --index;
                continue;
            }
            if (this._repositories.length && !this._repositories.has(this.commits[index].repository_id)) {
                --index;
                continue;
            }

            commits.unshift(this.commits[index]);
            repositories.add(this.commits[index].repository_id);
            if (repositories.length == this._repositories.length)
                break;

            --index;
        }
        return commits;
    }
    _loadSiblings(commit) {
        const query = paramsToQuery({
            branch: [commit.branch],
            repository_id: [commit.repository_id],
            id: [commit.id],
        });
        return fetch('api/commits/siblings?' + query).then(response => {
            let self = this;
            const originalLength = self.commits.length;
            response.json().then(json => {
                Object.keys(json).forEach(key => {
                    if (this._repositories.size > 0 && !this._repositories.has(key))
                        return;

                    // We get sibiling commits backwards
                    let index = self.commits.length - 1;
                    json[key].forEach(commitJson => {
                        const commit = new Commit(commitJson);
                        while (index >= 0) {
                            if (self.commits[index].uuid < commit.uuid) {
                                self.commits.splice(index, 0, commit);
                                --index;
                                break;
                            }
                            if (self.commits[index].uuid === commit.uuid)
                                break;
                            --index;
                        }
                        if (index < 0)
                            self.commits.splice(0, 0, commit);
                    });
                });

                if (originalLength != self.commits.length)
                    self.callbacks.forEach(callback => {
                        callback();
                    });
            });
        });
    }
    _load(beginUuid, endUuid) {
        if (endUuid < beginUuid)
            return;

        const limit = 2500;
        const query = paramsToQuery({
            branch: [...this._branches],
            limit: [limit],
            repository_id: [...this._repositories],
            after_uuid: [beginUuid],
            before_uuid: [endUuid]
        });

        return fetch(query ? 'api/commits?' + query : 'api/commits').then(response => {
            let self = this;
            response.json().then(json => {
                // We should have gotten a list of commits, if not, log the result and continue.
                if (!Array.isArray(json)) {
                    console.error(JSON.stringify(json, null, 4));
                    return;
                }

                let originalLength = self.commits.length;
                let commitsIndex = 0;
                let countForRepository = new Map();
                let firstIndexForRepository = new Map();
                for (let index = 0; index < json.length; ++index) {
                    const commit = new Commit(json[index]);
                    if (!firstIndexForRepository.has(commit.repository_id))
                        firstIndexForRepository.set(commit.repository_id, index);
                    if (!countForRepository.has(commit.repository_id))
                        countForRepository.set(commit.repository_id, 1);
                    else
                        countForRepository.set(commit.repository_id, countForRepository.get(commit.repository_id) + 1);

                    while (commitsIndex < self.commits.length) {
                        if (self.commits[commitsIndex].uuid > commit.uuid) {
                            self.commits.splice(commitsIndex, 0, commit);
                            ++commitsIndex;
                            break;
                        }
                        if (self.commits[commitsIndex].uuid === commit.uuid)
                            break;
                        ++commitsIndex;
                    }
                    if (commitsIndex === self.commits.length) {
                        self.commits.push(commit);
                        ++commitsIndex;
                        continue;
                    }
                }

                let minFound = beginUuid;
                countForRepository.forEach((count, repo) => {
                    if (count === limit) {
                        let commit = new Commit(json[firstIndexForRepository.get(repo)]);
                        minFound = Math.max(minFound, commit.uuid);
                    }
                });
                if (minFound != beginUuid)
                    self._load(beginUuid, minFound);
                else
                    self._loadSiblings(new Commit(json[0]));

                if (originalLength != self.commits.length)
                    self.callbacks.forEach(callback => {
                        callback();
                    });
            });
        }).catch(error => {
            // If the load fails, log the error and continue
            console.error(JSON.stringify(error, null, 4));
        });
    }
    add(beginUuid, endUuid) {
        if (!this._beginUuid && !this._endUuid) {
            this._beginUuid = beginUuid;
            this._endUuid = endUuid;
            return this._load(beginUuid, endUuid);
        }

        let promises = [];
        if (this._beginUuid && beginUuid < this._beginUuid)
            promises.push(this._load(beginUuid, this._beginUuid));
        if (this._endUuid && endUuid > this._endUuid)
            promises.push(this._load(this._endUuid, endUuid));
        return Promise.all(promises);
    }
    reload() {
        let needReload = false;
        const params = queryToParams(document.URL.split('?')[1]);

        function equal(a, b) {
            if (a.size !== b.size)
                return false;
            for (let elm of a) {
                if (!b.has(elm))
                    return false;
            }
            return true;
        }

        const branchSet = new Set(params.branch);
        if (!equal(branchSet, this._branches)) {
            this._branches = branchSet;
            needReload = true;
        }
        const repoSet = new Set(params.repository_id);
        if (!equal(repoSet, this._repositories)) {
            this._repositories = repoSet;
            needReload = true;
        }

        if (needReload) {
            this.commits = [];
            return this._load(this._beginUuid, this._endUuid);
        }
    }
}

const CommitBank = new _CommitBank();

export {Commit, CommitBank, CommitTable, TIMESTAMP_TO_UUID_MULTIPLIER};
