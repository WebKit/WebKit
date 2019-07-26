// Copyright (C) 2019 Apple Inc. All rights reserved.
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

import {CommitBank} from '/assets/js/commit.js';
import {Configuration} from '/assets/js/configuration.js';
import {deepCompare, ErrorDisplay, paramsToQuery, queryToParams} from '/assets/js/common.js';
import {DOM, REF} from '/library/js/Ref.js';


const DEFAULT_LIMIT = 100;

const stateToIDMapping = {
    CRASH: 0x00,
    TIMEOUT: 0x08,
    IMAGE: 0x10,
    AUDIO: 0x18,
    TEXT: 0x20,
    FAIL: 0x28,
    ERROR: 0x30,
    WARNING: 0x38,
    PASS: 0x40,
};

class Expectations
{
    static stringToStateId(string) {
        return stateToIDMapping[string];
    }

    static unexpectedResults(results, expectations)
    {
        let r = results.split('.');
        expectations.split(' ').forEach(expectation => {
            const i = r.indexOf(expectation);
            if (i > -1)
                r.splice(i, 1);
            if (expectation === 'FAIL')
                ['TEXT', 'AUDIO', 'IMAGE'].forEach(expectation => {
                    const i = r.indexOf(expectation);
                    if (i > -1)
                        r.splice(i, 1);
                });
        });
        let result = 'PASS';
        r.forEach(candidate => {
            if (Expectations.stringToStateId(candidate) < Expectations.stringToStateId(result))
                result = candidate;
        });
        return result;
    }
}

function tickForCommit(commit, scale) {
    let params = {
        branch: commit.branch ? [commit.branch] : queryToParams(document.URL.split('?')[1]).branch,
        uuid: [commit.uuid()],
    }
    if (!params.branch)
        delete params.branch;
    const query = paramsToQuery(params);

    if (scale <= 0)
        return '';
    if (scale === 1)
        return `<div class="scale">
                <div class="line"></div>
                    <div class="text">
                        <a href="/commit?${query}" target="_blank">${String(commit.id).substring(0,10)}</a>
                    </div>
                </div>`;
    let result = '';
    while (scale > 0) {
        let countToUse = scale;
        if (countToUse === 11 || countToUse == 12)
            countToUse = 6;
        else if (countToUse > 10)
            countToUse = 10;
        result += `<div class="scale group-${countToUse}">
                <div class="border-line-left"></div>
                <div class="line"></div>
                <div class="text">
                    <a href="/commit?${query}" target="_blank">${String(commit.id).substring(0,10)}</a>
                </div>
                <div class="border-line-right"></div>
            </div>`;
        scale -= countToUse;
    }
    return result;
}

function renderTimeline(commits, repositories = [], top = false) {
    // FIXME: This function breaks with more than 3 repositories because of <rdar://problem/51042981>
    if (repositories.length === 0) {
        if (top)
            return '';
        repositories = [null];
    }
    const start = top ? Math.ceil(repositories.length / 2) : 0;
    const end = top ? repositories.length : Math.ceil(repositories.length / 2);
    const numberOfElements = commits.length - Math.max(repositories.length - 1, 0)
    return repositories.slice(start, end).map(repository => {
        let commitsFromOtherRepos = 0;
        let renderedElements = 0;
        return `<div class="x-axis ${top ? 'top' : ''}">
                ${commits.map(commit => {
                    if (commit.repository_id && commit.repository_id !== repository) {
                        ++commitsFromOtherRepos;
                        return '';
                    }
                    const scale =  Math.min(commitsFromOtherRepos + 1, numberOfElements - renderedElements);
                    commitsFromOtherRepos = 0;
                    renderedElements += scale;
                    return tickForCommit(commit, scale);
                }).join('')}
            </div>`;
    }).join('');
}

class Dot {
    static merge(dots = {}) {
        let result = [];
        let index = 0;
        let hasData = true;

        while (hasData) {
            let dot = new Dot();
            hasData = false;

            Object.keys(dots).forEach(key => {
                if (dots[key].length <= index)
                    return;
                hasData = true;
                if (!dots[key][index].count)
                    return;
                if (dot.count)
                    dot.combined = true;

                dot.count += dots[key][index].count;
                dot.failed += dots[key][index].failed;
                dot.timeout += dots[key][index].timeout;
                dot.crash += dots[key][index].crash;
                if (dot.combined)
                    dot.link = null;
                else
                    dot.link = dots[key][index].link;
            });
            if (hasData)
                result.push(dot);
            ++index;
        }
        return result;
    }
    constructor(count = 0, failed = 0, timeout = 0, crash = 0, combined = false, link = null) {
        this.count = count;
        this.failed = failed;
        this.timeout = timeout;
        this.crash = crash;
        this.combined = combined;
        this.link = link;
    }
    toString() {
        if (!this.count)
            return `<div class="dot empty"></div>`;

        const self = this;

        function render(cssClass, inside='') {
            if (self.link)
                return `<a href="${self.link}" target="_blank" class="${cssClass}">${inside}</a>`;
            return `<div class="${cssClass}">${inside}</div>`;
        }

        if (!this.failed)
            return render('dot success');

        let key = 'failed';
        if (this.timeout)
            key = 'timeout';
        if (this.crash)
            key = 'crash';

        if (!this.combined)
            return render(`dot ${key}`, `<div class="tag" style="color:var(--boldInverseColor)">${this.failed}</div>`);

        return render(`dot ${key}`, `<div class="tag" style="color:var(--boldInverseColor)">
                ${function() {
                    let percent = Math.ceil(this.failed / self.count * 100 - .5);
                    if (percent > 0)
                        return percent;
                    return '<1';
                }()} %
            </div>`);
    }
}

class Timeline {
    constructor(endpoint, suite = null) {
        this.endpoint = endpoint;
        this.displayAllCommits = true;

        this.configurations = Configuration.fromQuery();
        this.results = {};
        this.collapsed = {};
        this.expandedSDKs = {};
        this.suite = suite;  // Suite is often implied by the endpoint, but trying to determine suite from endpoint is not trivial.

        const self = this;
        this.configurations.forEach(configuration => {
            this.results[configuration.toKey()] = [];
            this.collapsed[configuration.toKey()] = true;
        });

        this.latestDispatch = Date.now();
        this.ref = REF.createRef({
            state: {},
            onStateUpdate: (element, state) => {
                if (state.error)
                    element.innerHTML = ErrorDisplay(state);
                else if (state > 0)
                    DOM.inject(element, this.render(state));
                else
                    element.innerHTML = this.placeholder();
            }
        });

        CommitBank.callbacks.push(() => {
            const params = queryToParams(document.URL.split('?')[1]);
            self.ref.setState(params.limit ? parseInt(params.limit[params.limit.length - 1]) : DEFAULT_LIMIT);
        });

        this.reload();
    }
    rerender() {
        let params = queryToParams(document.URL.split('?')[1]);
        this.ref.setState(params.limit ? parseInt(params.limit[params.limit.length - 1]) : DEFAULT_LIMIT);
    }
    reload() {
        let myDispatch = Date.now();
        this.latestDispatch = Math.max(this.latestDispatch, myDispatch);
        this.ref.setState(-1);

        const self = this;
        let sharedParams = queryToParams(document.URL.split('?')[1]);
        Configuration.members().forEach(member => {
            delete sharedParams[member];
        });
        delete sharedParams.suite;
        delete sharedParams.test;
        delete sharedParams.repository_id;

        let newConfigs = Configuration.fromQuery();
        if (!deepCompare(newConfigs, this.configurations)) {
            this.configurations = newConfigs;
            this.results = {};
            this.collapsed = {};
            this.expandedSDKs = {};
            this.configurations.forEach(configuration => {
                this.results[configuration.toKey()] = [];
                this.collapsed[configuration.toKey()] = true;
            });
        }

        this.configurations.forEach(configuration => {
            let params = configuration.toParams();
            for (let key in sharedParams)
                params[key] = sharedParams[key];
            const query = paramsToQuery(params);

            fetch(query ? this.endpoint + '?' + query : this.endpoint).then(response => {
                response.json().then(json => {
                    if (myDispatch !== this.latestDispatch)
                        return;

                    let oldestUuid = Date.now() / 10;
                    let newestUuid = 0;
                    self.results[configuration.toKey()] = json;
                    self.results[configuration.toKey()].sort((a, b) => {
                        const aConfig = new Configuration(a.configuration);
                        const bConfig = new Configuration(b.configuration);
                        let configCompare = aConfig.compare(bConfig);
                        if (configCompare === 0)
                            configCompare = aConfig.compareSDKs(bConfig);
                        return configCompare;
                    });
                    self.results[configuration.toKey()].forEach(keyValue => {
                        keyValue.results.forEach(result => {
                            oldestUuid = Math.min(oldestUuid, result.uuid);
                            newestUuid = Math.max(newestUuid, result.uuid);
                        });
                    });

                    if (oldestUuid < newestUuid)
                        CommitBank.add(oldestUuid, newestUuid);

                    self.ref.setState(params.limit ? parseInt(params.limit[params.limit.length - 1]) : DEFAULT_LIMIT);
                });
            }).catch(error => {
                if (myDispatch === this.latestDispatch)
                    this.ref.setState({error: "Connection Error", description: error});
            });
        });
    }
    placeholder() {
        return `<div class="loader">
                <div class="spinner"></div>
            </div>`;
    }
    toString() {
        this.ref = REF.createRef({
            state: this.ref.state,
            onStateUpdate: (element, state) => {
                if (state.error)
                    element.innerHTML = ErrorDisplay(state);
                else if (state > 0)
                    DOM.inject(element, this.render(state));
                else
                    element.innerHTML = this.placeholder();
            }
        });

        return `<div class="content" ref="${this.ref}"></div>`;
    }
    render(limit) {
        let now = Math.floor(Date.now() / 10);
        let minDisplayedUuid = now;
        let maxLimitedUuid = 0;
        this.configurations.forEach(configuration => {
            this.results[configuration.toKey()].forEach(pair => {
                if (!pair.results.length)
                    return;
                if (limit !== 1 && limit === pair.results.length)
                    maxLimitedUuid = Math.max(pair.results[0].uuid, maxLimitedUuid);
                else if (limit === 1)
                    minDisplayedUuid = Math.min(pair.results[pair.results.length - 1].uuid, minDisplayedUuid);
                else
                    minDisplayedUuid = Math.min(pair.results[0].uuid, minDisplayedUuid);
            });
        });
        if (minDisplayedUuid === now)
            minDisplayedUuid = maxLimitedUuid;
        else
            minDisplayedUuid = Math.max(minDisplayedUuid, maxLimitedUuid);

        let commits = [];
        let repositories = new Set();
        let currentCommitIndex = CommitBank.commits.length - 1;
        this.configurations.forEach(configuration => {
            this.results[configuration.toKey()].forEach(pair => {
                pair.results.forEach(result => {
                    if (result.uuid < minDisplayedUuid)
                        return;
                    let candidateCommits = [];

                    if (!this.displayAllCommits)
                        currentCommitIndex = CommitBank.commits.length - 1;
                    while (currentCommitIndex >= 0) {
                        if (CommitBank.commits[currentCommitIndex].uuid() < result.uuid)
                            break;
                        if (this.displayAllCommits || CommitBank.commits[currentCommitIndex].uuid() == result.uuid)
                            candidateCommits.push(CommitBank.commits[currentCommitIndex]);
                        --currentCommitIndex;
                    }
                    if (candidateCommits.length === 0 || candidateCommits[candidateCommits.length - 1].uuid() !== result.uuid)
                        candidateCommits.push({
                            'id': '?',
                            'uuid': () => {return result.uuid;}
                        });

                    let index = 0;
                    candidateCommits.forEach(commit => {
                        if (commit.repository_id)
                            repositories.add(commit.repository_id);
                        while (index < commits.length) {
                            if (commit.uuid() === commits[index].uuid())
                                return;
                            if (commit.uuid() > commits[index].uuid()) {
                                commits.splice(index, 0, commit);
                                return;
                            }
                            ++index;
                        }
                        commits.push(commit);
                    });
                });
            });
        });
        if (currentCommitIndex >= 0 && commits.length) {
            let trailingRepositories = new Set(repositories);
            trailingRepositories.delete(commits[commits.length - 1].repository_id);
            while (currentCommitIndex >= 0 && trailingRepositories.size) {
                const commit = CommitBank.commits[currentCommitIndex];
                if (trailingRepositories.has(commit.repository_id)) {
                    commits.push(commit);
                    trailingRepositories.delete(commit.repository_id);
                }
                --currentCommitIndex;
            }

        }

        repositories = [...repositories];
        repositories.sort();

        return `<div class="timeline">
                <div ${repositories.length > 1 ? `class="header with-top-x-axis"` : `class="header"`}>
                    ${this.configurations.map(configuration => {
                        if (Object.keys(this.results[configuration.toKey()]).length === 0)
                            return '';
                        const self = this;
                        const expander = REF.createRef({
                            onElementMount: element => {
                                element.onclick = () => {
                                    self.collapsed[configuration.toKey()] = !self.collapsed[configuration.toKey()];
                                    self.rerender();
                                }
                            },
                        });
                        let result = `<div class="series">
                                <a ref="${expander}" style="cursor: pointer;">
                                    ${function() {return self.collapsed[configuration.toKey()] ? '+' : '-'}()}
                                </a>
                                ${configuration}
                            </div>`;
                        if (!this.collapsed[configuration.toKey()]) {
                            const seriesHeaderForSDKLessConfig = config => {
                                if (typeof config === 'string')
                                    return `<div class="series">${config}</div>`;

                                let queueParams = config.toParams();
                                queueParams['suite'] = [this.suite];
                                const configLink = `<a class="text tiny" href="/urls/queue?${paramsToQuery(queueParams)}" target="_blank">${config}</a>`;
                                const key = config.toKey();
                                const expander = REF.createRef({
                                    onElementMount: element => {
                                        element.onclick = () => {
                                            self.expandedSDKs[key] = !self.expandedSDKs[key];
                                            self.rerender();
                                        }
                                    },
                                });
                                if (this.expandedSDKs[key] === undefined)
                                    return `<div class="series">${configLink}</div>`;
                                return `<div class="series text tiny">
                                        <a ref="${expander}" style="cursor: pointer;">
                                            ${function() {return self.expandedSDKs[key] ? '-sdk' : '+sdk'}()}
                                        </a>
                                         | ${configLink}
                                    </div>`;
                            }
                            let lastConfig = null;

                            this.results[configuration.toKey()].forEach(pair => {
                                if (pair.results.length <= 0 || pair.results[pair.results.length - 1].uuid < minDisplayedUuid)
                                    return '';

                                let config = new Configuration(pair.configuration);
                                const sdk = config.sdk;
                                delete config.sdk;  // Strip out sdk information.

                                const key = config.toKey();
                                const compareIndex = config.compare(lastConfig);
                                if (lastConfig !== null && compareIndex != 0 && !this.expandedSDKs[lastConfig.toKey()])
                                    result += seriesHeaderForSDKLessConfig(lastConfig);
                                else if (compareIndex === 0 && this.expandedSDKs[key] === undefined)
                                    this.expandedSDKs[key] = false;  // Populate this dictionary on the fly.
                                if (this.expandedSDKs[key]) {
                                    if (compareIndex)
                                        result += seriesHeaderForSDKLessConfig(config);
                                    result += `<div class="series">${sdk}</div>`;
                                }
                                lastConfig = config;
                            });
                            if (lastConfig !== null && !this.expandedSDKs[lastConfig.toKey()])
                                result += seriesHeaderForSDKLessConfig(lastConfig);
                        }
                        return result;
                    }).join('')}
                </div>
                <div class="content">
                    ${renderTimeline(commits, repositories, true)}
                    ${this.configurations.map(configuration => {
                        if (Object.keys(this.results[configuration.toKey()]).length === 0)
                            return '';

                        let currentArrayIndex = new Array(this.results[configuration.toKey()].length).fill(1);
                        let dots = {};
                        for (let i = 0; i < this.results[configuration.toKey()].length; ++i) {
                            const pairForIndex = this.results[configuration.toKey()][i];
                            if (pairForIndex.results.length <= 0 || pairForIndex.results[pairForIndex.results.length - 1].uuid < minDisplayedUuid)
                                continue;
                            dots[new Configuration(pairForIndex.configuration).toKey()] = [];
                        }
                        commits.slice(0, commits.length - Math.max(repositories.length - 1, 0)).forEach(commit => {
                            for (let i = 0; i < currentArrayIndex.length; ++i) {
                                const pairForIndex = this.results[configuration.toKey()][i];
                                if (pairForIndex.results.length <= 0 || pairForIndex.results[pairForIndex.results.length - 1].uuid < minDisplayedUuid)
                                    continue;

                                const config = new Configuration(pairForIndex.configuration);
                                const configurationKey = config.toKey();
                                let resultIndex = pairForIndex.results.length - currentArrayIndex[i];
                                if (resultIndex < 0) {
                                    dots[configurationKey].push(new Dot());
                                    continue;
                                }

                                if (commit.uuid() === pairForIndex.results[resultIndex].uuid) {
                                    let buildParams = config.toParams();
                                    buildParams['suite'] = [this.suite];
                                    buildParams['uuid'] = [commit.uuid()];
                                    const buildLink = `/urls/build?${paramsToQuery(buildParams)}`;

                                    if (pairForIndex.results[resultIndex].stats)
                                        dots[configurationKey].push(new Dot(
                                            pairForIndex.results[resultIndex].stats.tests_run,
                                            pairForIndex.results[resultIndex].stats.tests_unexpected_failed,
                                            pairForIndex.results[resultIndex].stats.tests_unexpected_timedout,
                                            pairForIndex.results[resultIndex].stats.tests_unexpected_crashed,
                                            false,
                                            buildLink,
                                        ));
                                    else {
                                        const resultId = Expectations.stringToStateId(Expectations.unexpectedResults(
                                            pairForIndex.results[resultIndex].actual,
                                            pairForIndex.results[resultIndex].expected,
                                        ));

                                        dots[configurationKey].push(new Dot(
                                            1,
                                            resultId <= Expectations.stringToStateId('ERROR') ? 1 : 0,
                                            resultId <= Expectations.stringToStateId('TIMEOUT') ? 1 : 0,
                                            resultId <= Expectations.stringToStateId('CRASH') ? 1 : 0,
                                            false,
                                            buildLink,
                                        ));
                                    }
                                } else
                                    dots[configurationKey].push(new Dot());

                                while (resultIndex >= 0 && commit.uuid() <= pairForIndex.results[resultIndex].uuid) {
                                    ++currentArrayIndex[i];
                                    resultIndex = pairForIndex.results.length - currentArrayIndex[i];
                                }
                            }
                        });

                        if (this.collapsed[configuration.toKey()])
                            return `<div class="series">${Dot.merge(dots).map(dot => {return dot.toString();}).join('')}</div>`;

                        let result = '';
                        let currentDots = {};
                        let lastConfig = null;

                        this.results[configuration.toKey()].forEach(pair => {
                            let config = new Configuration(pair.configuration);
                            const key = config.toKey();
                            if (!dots[key])
                                return;
                            delete config.sdk;  // Strip out sdk information

                            const compareIndex = config.compare(lastConfig);
                            const isSDKExpanded = this.expandedSDKs[config.toKey()];
                            if (compareIndex || isSDKExpanded) {
                                result += `<div class="series">${Dot.merge(currentDots).map(dot => {return dot.toString();}).join('')}</div>`;
                                currentDots = {};
                            }
                            if (compareIndex && isSDKExpanded)
                                result += '<div class="series"></div>';
                            currentDots[key] = dots[key];
                            lastConfig = config;
                        });
                        if (currentDots)
                            result += `<div class="series">${Dot.merge(currentDots).map(dot => {return dot.toString();}).join('')}</div>`;

                        return result;
                    }).join('')}
                    ${renderTimeline(commits, repositories)}
                </div>
            </div>`;
    }
}

function Legend() {
    return `<div class="content">
            <br>
            <div class="lengend timeline">
                <div class="item">
                    <div class="dot success"></div>
                    <div class="label">All Tests Passed</div>
                </div>
                <div class="item">
                    <div class="dot failed"></div>
                    <div class="label">Some Tests Failed</div>
                </div>
                <div class="item">
                    <div class="dot timeout"></div>
                    <div class="label">Some Tests Timed-Out</div>
                </div>
                <div class="item">
                    <div class="dot crash"></div>
                    <div class="label">Some Tests Crashed</div>
                </div>
            </div>
            <br>
        </div>`;
}

export {Legend, Timeline, Expectations};
