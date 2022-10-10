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

import {ArchiveRouter} from '/assets/js/archiveRouter.js';
import {CommitBank} from '/assets/js/commit.js';
import {Configuration} from '/assets/js/configuration.js';
import {deepCompare, ErrorDisplay, escapeHTML, paramsToQuery, queryToParams, linkify, escapeEndpoint} from '/assets/js/common.js';
import {Expectations} from '/assets/js/expectations.js';
import {InvestigateDrawer} from '/assets/js/investigate.js';
import {ToolTip} from '/assets/js/tooltip.js';
import {Timeline} from '/library/js/components/TimelineComponents.js';
import {DOM, EventStream, REF, FP} from '/library/js/Ref.js';


const DEFAULT_LIMIT = 100;

let willFilterExpected = false;
let showTestTimes = false;
let showTestFlakiness = false;
let showNumberOfFlakyTests = false;

const BUG_TRACKER_COLORS = {
    radar: 'var(--purple)',
    bugzilla: 'var(--blue)'
} 

function minimumUuidForResults(results, limit) {
    const now = Math.floor(Date.now() / 10);
    let minDisplayedUuid = now;
    let maxLimitedUuid = 0;

    Object.keys(results).forEach((key) => {
        results[key].forEach(pair => {
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
        return maxLimitedUuid;
    return Math.max(minDisplayedUuid, maxLimitedUuid);
}

function commitsForResults(results, limit, allCommits = true) {
    const minDisplayedUuid = minimumUuidForResults(limit);
    let commits = [];
    let repositories = new Set();
    let currentCommitIndex = CommitBank.commits.length - 1;
    Object.keys(results).forEach((key) => {
        results[key].forEach(pair => {
            pair.results.forEach(result => {
                if (result.uuid < minDisplayedUuid)
                    return;
                let candidateCommits = [];

                if (!allCommits)
                    currentCommitIndex = CommitBank.commits.length - 1;
                while (currentCommitIndex >= 0) {
                    if (CommitBank.commits[currentCommitIndex].uuid < result.uuid)
                        break;
                    if (allCommits || CommitBank.commits[currentCommitIndex].uuid === result.uuid)
                        candidateCommits.push(CommitBank.commits[currentCommitIndex]);
                    --currentCommitIndex;
                }
                if (candidateCommits.length === 0 || candidateCommits[candidateCommits.length - 1].uuid !== result.uuid)
                    candidateCommits.push({
                        id: '?',
                        uuid: result.uuid,
                    });

                let index = 0;
                candidateCommits.forEach(commit => {
                    if (commit.repository_id)
                        repositories.add(commit.repository_id);
                    while (index < commits.length) {
                        if (commit.uuid === commits[index].uuid)
                            return;
                        if (commit.uuid > commits[index].uuid) {
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
    return commits;
}

function scaleForCommits(commits) {
    let scale = [];
    for (let i = commits.length - 1; i >= 0; --i) {
        const repository_id = commits[i].repository_id ? commits[i].repository_id : '?';
        scale.unshift({});
        scale[0][repository_id] = commits[i];
        if (scale.length < 2)
            continue;
        Object.keys(scale[1]).forEach((key) => {
            if (key === repository_id || key === '?' || key === 'uuid')
                return;
            scale[0][key] = scale[1][key];
        });
        scale[0].uuid = Math.max(...Object.keys(scale[0]).map((key) => {
            return scale[0][key].uuid;
        }));
    }
    return scale;
}

function repositoriesForCommits(commits) {
    let repositories = new Set();
    commits.forEach((commit) => {
        if (commit.repository_id)
            repositories.add(commit.repository_id);
    });
    repositories = [...repositories];
    if (!repositories.length)
        repositories = ['?'];
    repositories.sort();
    return repositories;
}

function xAxisFromScale(scale, repository, updatesArray, isTop=false, viewport=null)
{
    function scaleForRepository(scale) {
        return scale.map(node => {
            let commit = node[repository];
            if (!commit)
                commit = node['?'];
            if (!commit)
                return {id: '', uuid: null};
            return commit;
        });
    }

    function onScaleClick(node) {
        if (!node.label.label())
            return;
        let params = {
            branch: node.label.branch ? [node.label.branch] : queryToParams(document.URL.split('?')[1]).branch,
            uuid: [node.label.uuid],
        }
        if (!params.branch)
            delete params.branch;
        const query = paramsToQuery(params);
        window.open(`/commit?${query}`, '_blank');
    }

    return Timeline.CanvasXAxisComponent(scaleForRepository(scale), {
        isTop: isTop,
        height: 130,
        onScaleClick: onScaleClick,
        onScaleEnter: (node, event, canvas) => {
            const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;
            ToolTip.set(
                `<div class="content">
                    Time: ${new Date(node.label.timestamp * 1000).toLocaleString()}
                    ${node.label.author ? `<br>Author: ${escapeHTML(node.label.author.name)}
                            &#60<a href="mailto:${escapeHTML(node.label.author.emails[0])}">${escapeHTML(node.label.author.emails[0])}</a>&#62` : ''}
                    ${node.label.message ? `<br><div>${linkify(escapeHTML(node.label.message.split('\n')[0]))}</div>` : ''}
                    ${node.label.bugUrls || node.label.radarUrls ? '<hr>' : ''}
                    ${node.label.bugUrls ? `<div>${linkify(escapeHTML(node.label.bugUrls.join('\n').split('\n')[0]))}</div>` : ''}
                    ${node.label.radarUrls ? `<div>${linkify(escapeHTML(node.label.radarUrls.join('\n').split('\n')[0]))}</div>` : ''}
                </div>`,
                node.tipPoints.map((point) => {
                    return {x: canvas.x + point.x, y: canvas.y + scrollDelta + point.y};
                }),
                (event) => {return onScaleClick(node);},
                viewport,
            );
        },
        onScaleLeave: (event, canvas) => {
            const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;
            if (!ToolTip.isIn({x: event.pageX, y: event.pageY - scrollDelta}))
                ToolTip.unset();
        },
        getLabelFunc: (commit) => {return commit && commit.label ? commit.label() : '?';},
        getScaleFunc: (commit) => commit.uuid,
        exporter: (updateFunction) => {
            updatesArray.push((scale) => {updateFunction(scaleForRepository(scale));});
        },
    });
}

const testsRegex = /tests_([a-z])+/;
const worstRegex = /worst_([a-z])+/;

function inPlaceCombine(out, obj)
{
    if (!obj)
        return out;

    if (!out) {
        out = {};
        Object.keys(obj).forEach(key => {
            if (key[0] === '_')
                return;
            if (obj[key] instanceof Object)
                out[key] = inPlaceCombine(out[key], obj[key]);
            else
                out[key] = obj[key];
        });
    } else {
        Object.keys(out).forEach(key => {
            if (key[0] === '_')
                return;

            if (out[key] instanceof Object) {
                out[key] = inPlaceCombine(out[key], obj[key]);
                return;
            }

            // Set of special case keys which need to be added together
            if (key.match(worstRegex))
                return;
            if (key.match(testsRegex)) {
                const worstKey = `worst_${key}`;
                out[worstKey] = Math.max(
                    out[worstKey] ? out[worstKey] : out[key],
                    obj[worstKey] ? obj[worstKey] : obj[key],
                );
                out[key] += obj[key];
                return;
            }

            // Some special combination logic
            if (key === 'time') {
                out[key] = Math.max(
                    out[key] ? out[key] : 0,
                    obj[key] ? obj[key] : 0,
                );
                return;
            }

            // If the key exists, but doesn't match, delete it
            if (!(key in obj) || out[key] !== obj[key]) {
                delete out[key];
                return;
            }
        });
        Object.keys(obj).forEach(key => {
            if (!key.match(testsRegex))
                return;
            const worstKey = `worst_${key}`;
            if (!(key in out)) {
                out[key] = obj[key];
                out[worstKey] = obj[key];
            }
            out[worstKey] = Math.max(out[worstKey], obj[worstKey] ? obj[worstKey] : obj[key]);
        });
    }
    return out;
}

function statsForSingleResult(result) {
    const actualId = Expectations.stringToStateId(result.actual);
    const unexpectedId = Expectations.stringToStateId(Expectations.unexpectedResults(result.actual, result.expected));
    let stats = {
        tests_run: 1,
        tests_skipped: 0,
    }
    Expectations.failureTypes.forEach(type => {
        const idForType = Expectations.stringToStateId(Expectations.failureTypeMap[type]);
        stats[`tests_${type}`] = actualId > idForType  ? 0 : 1;
        stats[`tests_unexpected_${type}`] = unexpectedId > idForType  ? 0 : 1;
    });
    return stats;
}

function combineResults() {
    let counts = new Array(arguments.length).fill(0);
    let data = [];

    while (true) {
        // Find candidate uuid
        let uuid = 0;
        for (let i = 0; i < counts.length; ++i) {
            let candidateUuid = null;
            while (arguments[i] && arguments[i].length > counts[i]) {
                candidateUuid = arguments[i][counts[i]].uuid;
                if (candidateUuid)
                    break;
                ++counts[i];
            }
            if (candidateUuid)
                uuid = Math.max(uuid, candidateUuid);
        }

        if (!uuid)
            return data;

        // Combine relevant results
        let dataNode = null;
        for (let i = 0; i < counts.length; ++i) {
            while (counts[i] < arguments[i].length && arguments[i][counts[i]] && arguments[i][counts[i]].uuid === uuid) {
                if (dataNode && !dataNode.stats)
                    dataNode.stats = statsForSingleResult(dataNode);

                dataNode = inPlaceCombine(dataNode, arguments[i][counts[i]]);

                if (dataNode.stats && !arguments[i][counts[i]].stats)
                    dataNode.stats = inPlaceCombine(dataNode.stats, statsForSingleResult(arguments[i][counts[i]]));

                ++counts[i];
            }
        }
        if (dataNode)
            data.push(dataNode);
    }
    return data;
}

class TimelineFromEndpoint {
    constructor(endpoint, suite = null, test = null, viewport = null) {
        this.endpoint = endpoint;
        this.displayAllCommits = true;

        this.configurations = Configuration.fromQuery();
        this.results = {};
        this.selectedDots = new Map();

        // Suite and test can often be implied by the endpoint, but doing so is more confusing then helpful
        this.suite = suite;
        this.test = test;

        this.updates = [];
        this.xaxisUpdates = [];
        this.timelineUpdate = null;
        this.notifyRerender = () => {};
        this.repositories = [];
        this.viewport = viewport;

        const self = this;

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
        this.commit_callback = () => {
            self.update();
        };
        CommitBank.callbacks.push(this.commit_callback);

        this.reload();
    }
    teardown() {
        CommitBank.callbacks = CommitBank.callbacks.filter((value, index, arr) => {
            return this.commit_callback === value;
        });
    }
    update() {
        if (this.selectedDotsButtonGroupRef)
            this.selectedDotsButtonGroupRef.setState({show: false});
        const params = queryToParams(document.URL.split('?')[1]);
        const commits = commitsForResults(this.results, params.limit ? parseInt(params.limit[params.limit.length - 1]) : DEFAULT_LIMIT, this.allCommits);
        const scale = scaleForCommits(commits);

        const newRepositories = repositoriesForCommits(commits);
        let haveNewRepos = this.repositories.length !== newRepositories.length;
        for (let i = 0; !haveNewRepos && i < this.repositories.length && i < newRepositories.length; ++i)
            haveNewRepos = this.repositories[i] !== newRepositories[i];
        if (haveNewRepos && this.timelineUpdate) {
            this.xaxisUpdates = [];
            let top = true;
            let components = [];

            newRepositories.forEach(repository => {
                components.push(xAxisFromScale(scale, repository, this.xaxisUpdates, top, this.viewport));
                top = false;
            });

            this.timelineUpdate(components);
            this.repositories = newRepositories;
        }

        this.updates.forEach(func => {func(scale);})
        this.xaxisUpdates.forEach(func => {func(scale);});
    }
    rerender() {
        const params = queryToParams(document.URL.split('?')[1]);
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
            this.configurations.forEach(configuration => {
                this.results[configuration.toKey()] = [];
            });
        }

        this.configurations.forEach(configuration => {
            let params = configuration.toParams();
            for (let key in sharedParams)
                params[key] = sharedParams[key];
            const query = paramsToQuery(params);

            fetch(query ? escapeEndpoint(this.endpoint) + '?' + query : escapeEndpoint(this.endpoint)).then(response => {
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
                    DOM.inject(element, ErrorDisplay(state));
                else if (state > 0)
                    DOM.inject(element, this.render(state));
                else
                    DOM.inject(element, this.placeholder());
            }
        });

        return `
        <div style="position:relative">
            <div class="content" ref="${this.ref}"></div>
        </div>`;
    }

    getTestResultStatus(data, willFilterExpected=false) {
        let failureType = null;
        let failureNumber = null;
        if (data.stats) {
            if (data.start_time)
                failureNumber = data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}failed`];
            else
                failureNumber = data.stats[`worst_tests${willFilterExpected ? '_unexpected_' : '_'}failed`];
            if (data.stats.worst_tests_run <= 1)
                failureNumber = null;

            Expectations.failureTypes.forEach(type => {
                if (data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}${type}`] > 0) {
                    failureType = type;
                }
            });
        } else {
            let resultId = Expectations.stringToStateId(data.actual);
            if (willFilterExpected)
                resultId = Expectations.stringToStateId(Expectations.unexpectedResults(data.actual, data.expected));
            Expectations.failureTypes.forEach(type => {
                if (Expectations.stringToStateId(Expectations.failureTypeMap[type]) >= resultId) {
                    failureType = type;
                }
            });
        }
        return {failureType, failureNumber};
    }

    getTestResultUrl(config, data) {
        const buildParams = config.toParams();
        buildParams['suite'] = [this.suite];
        buildParams['uuid'] = [data.uuid];
        buildParams['after_time'] = [data.start_time];
        buildParams['before_time'] = [data.start_time];
        return `${window.location.protocol}//${window.location.host}/urls/build?${paramsToQuery(buildParams)}`;
    }

    _renderSelectedDotsButtonGroup(element) {
        DOM.inject(element, 
            `<div class="row">
                ${this.bugTrackers.map(bugTracker => {
                    const buttonText = `${bugTracker[0].toUpperCase()}${bugTracker.substring(1)}`;
                    const buttonRef = REF.createRef({
                        state: {
                            loading: false
                        },
                        onStateUpdate: (element, stateDiff) => {
                            if (stateDiff.loading)
                                element.innerText = 'Waiting...';
                            else
                                element.innerText = buttonText;
                        }
                    });
        
                    buttonRef.fromEvent('click').action(e => {
                        const requestPayload = {
                            selectedRows: [],
                            willFilterExpected: InvestigateDrawer.willFilterExpected,
                            repositories: this.repositories,
                            suite: this.suite,
                            test: this.test
                        };
                        Array.from(this.selectedDots.keys()).forEach(config => {
                            const dots = this.selectedDots.get(config);
                            requestPayload.selectedRows.push({
                                config,
                                results: dots
                            });
                        });
                        buttonRef.setState({loading: true});
                        fetch(`api/bug-trackers/${bugTracker}/create-bug`, {
                            method: 'PUT',
                            headers: {
                                'Content-Type': 'application/json'
                            },
                            body: JSON.stringify(requestPayload)
                        }).then(res => {
                            if (res.ok)
                                return res.json()
                            return res.json().then((data) => {
                                throw new Error(data.description);
                            });
                        }).then(data => {
                            const bugLinkElement = document.createElement('a');
                            if (data['newWindow'])
                                bugLinkElement.setAttribute('target', '_blank');
                            bugLinkElement.setAttribute('href', data['url']);
                            bugLinkElement.click();
                        }).catch(e => {
                            alert(e);
                        }).finally(() => {
                            buttonRef.setState({loading: false});
                        });
                        
                    });
                    return `
                        <button class="button tiny" style="background: ${BUG_TRACKER_COLORS[bugTracker]}; color: var(--white)" ref="${buttonRef}">
                            ${buttonText}
                        </button>`;
                }).join('')}
            </div>`);
    }

    render(limit) {
        const branch = queryToParams(document.URL.split('?')[1]).branch;
        const self = this;
        const commits = commitsForResults(this.results, limit, this.allCommits);
        const scale = scaleForCommits(commits);

        this.selectedDotsButtonGroupRef = REF.createRef({
            state: {
                show: false,
                top: 0,
                left: 0,
            },
            onElementMount: (element) => {
                if (this.bugTrackers)
                    this._renderSelectedDotsButtonGroup(element);
                fetch('api/bug-trackers').then(res => res.json()).then(bugTrackers => {
                    this.bugTrackers = bugTrackers;
                    this._renderSelectedDotsButtonGroup(element);
                })
            },
            onStateUpdate: (element, stateDiff) => {
                if ('show' in stateDiff) {
                    if (stateDiff.show)
                        element.style.display = 'block';
                    else
                        element.style.display = 'none';
                }
                if ('top' in stateDiff)
                    element.style.top = `${stateDiff.top}px`;
                if ('left' in stateDiff) {
                    const rect = element.getBoundingClientRect();
                    element.style.left = `${stateDiff.left - rect.width}px`;
                }
            }
        });

        this.radarButtonRef = REF.createRef({
            state: {
                show: false,
                top: 0,
                left: 0,
                loading: false,
            },
            onStateUpdate: (element, stateDiff) => {
                if ('show' in stateDiff) {
                    if (stateDiff.show)
                        element.style.display = 'block';
                    else
                        element.style.display = 'none';
                }
                if ('top' in stateDiff)
                    element.style.top = `${stateDiff.top}px`;
                if ('left' in stateDiff) {
                    const rect = element.getBoundingClientRect();
                    element.style.left = `${stateDiff.left - rect.width}px`;
                }
            }
        });

        const colorMap = Expectations.colorMap();
        this.updates = [];
        const options = {
            getScaleFunc: (value) => {
                if (value && value.uuid)
                    return {uuid: value.uuid};
                return {};
            },
            compareFunc: (a, b) => {return b.uuid - a.uuid;},
            renderFactory: (drawDot) => (data, context, x, y) => {
                if (!data)
                    return drawDot(context, x, y, true);

                let tag = [];
                let pushTag = (value, prefix='', suffix='') => {
                    if (value)
                        tag.push(prefix + value + suffix);
                };
                let color = colorMap.success;
                let symbol = Expectations.symbolMap.success;
                if (data.stats) {
                    if (data.start_time)
                        pushTag(data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}failed`]);
                    else
                        pushTag(data.stats[`worst_tests${willFilterExpected ? '_unexpected_' : '_'}failed`]);
                    if (data.stats.worst_tests_run <= 1)
                        tag = [];

                    Expectations.failureTypes.forEach(type => {
                        if (data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}${type}`] > 0) {
                            color = colorMap[type];
                            symbol = Expectations.symbolMap[type];
                        }
                    });
                } else {
                    let resultId = Expectations.stringToStateId(data.actual);
                    if (willFilterExpected)
                        resultId = Expectations.stringToStateId(Expectations.unexpectedResults(data.actual, data.expected));
                    Expectations.failureTypes.forEach(type => {
                        if (Expectations.stringToStateId(Expectations.failureTypeMap[type]) >= resultId) {
                            color = colorMap[type];
                            symbol = Expectations.symbolMap[type];
                        }
                    });
                }
                const time = data.time ? Math.round(data.time / 1000) : 0;
                if (time && showTestTimes)
                    pushTag(time);
                if (showTestFlakiness && data.flakiness_num_passes && data.flakiness_num_tries) {
                    let flakiness = 1 - (data.flakiness_num_passes / data.flakiness_num_tries);
                    pushTag((100 * flakiness).toFixed(), '', '%');
                }
                if (showNumberOfFlakyTests && data.details && data.details.number_of_flaky_tests){
                    pushTag(data.details.number_of_flaky_tests, 'fl');
                }

                return drawDot(context, x, y, false, tag.length ? tag.join('/') : null, symbol, false, color);
            },
        };

        function onDotClickFactory(configuration) {
            return (data) => {
                let allData = [];
                let partialConfiguration = {};
                self.configurations.forEach(configurationKey => {
                    if (configurationKey.compare(configuration) || configurationKey.compareSDKs(configuration))
                        return;
                    self.results[configurationKey.toKey()].forEach(pair => {
                        const computedConfiguration = new Configuration(pair.configuration);
                        if (computedConfiguration.compare(configuration) || computedConfiguration.compareSDKs(configuration))
                            return;
                        let doesMatch = false;
                        pair.results.forEach(node => {
                            if (node.uuid !== data.uuid)
                                return;
                            doesMatch = true;
                            let dataNode = {};
                            Object.keys(node).forEach(key => {
                                dataNode[key] = node[key];
                            });
                            dataNode['configuration'] = computedConfiguration;
                            allData.push(dataNode);
                        });
                        if (doesMatch) {
                            Configuration.members().forEach(member => {
                                if (member in partialConfiguration) {
                                    if (partialConfiguration[member] !== null && partialConfiguration[member] !== computedConfiguration[member])
                                        partialConfiguration[member] = null;
                                } else if (computedConfiguration[member] !== null)
                                    partialConfiguration[member] = computedConfiguration[member];
                            });
                        }
                    });
                });
                let agregateData = {};
                Object.keys(data).forEach(key => {
                    agregateData[key] = data[key];
                });
                agregateData['configuration'] = new Configuration(partialConfiguration);
                ToolTip.unset();
                InvestigateDrawer.expand(self.suite, agregateData, allData);
            }
        }

        function onDotEnterFactory(configuration) {
            return (data, event, canvas) => {
                let partialConfiguration = {};
                self.configurations.forEach(configurationKey => {
                    if (configurationKey.compare(configuration) || configurationKey.compareSDKs(configuration))
                        return;
                    self.results[configurationKey.toKey()].forEach(pair => {
                        const computedConfiguration = new Configuration(pair.configuration);
                        if (computedConfiguration.compare(configuration) || computedConfiguration.compareSDKs(configuration))
                            return;
                        let doesMatch = false;
                        pair.results.forEach(node => {
                            if (doesMatch)
                                return;
                            if (node.uuid == data.uuid)
                                doesMatch = true;
                        });
                        if (doesMatch) {
                            Configuration.members().forEach(member => {
                                if (member in partialConfiguration) {
                                    if (partialConfiguration[member] !== null && partialConfiguration[member] !== computedConfiguration[member])
                                        partialConfiguration[member] = null;
                                } else if (computedConfiguration[member] !== null)
                                    partialConfiguration[member] = computedConfiguration[member];
                            });
                        }
                    });
                });
                partialConfiguration = new Configuration(partialConfiguration);
                const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;

                const buildParams = configuration.toParams();
                buildParams['suite'] = [self.suite];
                buildParams['uuid'] = [data.uuid];
                buildParams['after_time'] = [data.start_time];
                buildParams['before_time'] = [data.start_time];
                if (branch)
                    buildParams['branch'] = branch;

                ToolTip.set(
                    `<div class="content">
                        ${data.start_time ? `<a href="/urls/build?${paramsToQuery(buildParams)}" target="_blank">Test run</a> @ ${new Date(data.start_time * 1000).toLocaleString()}<br>` : ''}
                        ${data.start_time && ArchiveRouter.hasArchive(self.suite, data.actual) ? `<a href="/archive/${ArchiveRouter.pathFor(self.suite, data.actual, self.test)}?${paramsToQuery(buildParams)}" target="_blank">${ArchiveRouter.labelFor(self.suite, data.actual)}</a><br>` : ''}
                        Commits: ${CommitBank.commitsDuring(data.uuid).map((commit) => {
                            let params = {
                                branch: commit.branch ? [commit.branch] : branch,
                                uuid: [commit.uuid],
                            }
                            if (!params.branch)
                                delete params.branch;
                            const query = paramsToQuery(params);
                            return `<a href="/commit/info?${query}" target="_blank">${commit.label()}</a>`;
                        }).join(', ')}
                        <br>
                        ${partialConfiguration}
                        <br>
                        ${data.stats ? `<a href="/investigate?${paramsToQuery(buildParams)}" target="_blank">Investigate failures</a><br>` : ''}
                        ${data.expected ? `Expected: ${data.expected}<br>` : ''}
                        ${data.actual ? `Actual: ${data.actual}<br>` : ''}
                    </div>`,
                    data.tipPoints.map((point) => {
                        return {x: canvas.x + point.x, y: canvas.y + scrollDelta + point.y};
                    }),
                    (event) => {onDotClickFactory(configuration)(data);},
                    self.viewport,
                );
            }
        }

        function onDotLeave(event, canvas) {
            const scrollDelta = document.documentElement.scrollTop || document.body.scrollTop;
            if (!ToolTip.isIn({x: event.pageX, y: event.pageY - scrollDelta}))
                ToolTip.unset();
        }

        function exporterFactory(data) {
            return (updateFunction) => {
                self.updates.push((scale) => {updateFunction(data, scale);});
            }
        }

        let children = [];
        this.configurations.forEach(configuration => {
            if (!this.results[configuration.toKey()] || Object.keys(this.results[configuration.toKey()]).length === 0)
                return;

            // Create a list of configurations to display with SDKs stripped
            let mappedChildrenConfigs = {};
            let childrenConfigsBySDK = {}
            let resultsByKey = {};
            this.results[configuration.toKey()].forEach(pair => {
                const strippedConfig = new Configuration(pair.configuration);
                resultsByKey[strippedConfig.toKey()] = combineResults([], [...pair.results].sort(function(a, b) {return b.uuid - a.uuid;}));
                strippedConfig.sdk = null;
                mappedChildrenConfigs[strippedConfig.toKey()] = strippedConfig;
                if (!childrenConfigsBySDK[strippedConfig.toKey()])
                    childrenConfigsBySDK[strippedConfig.toKey()] = [];
                childrenConfigsBySDK[strippedConfig.toKey()].push(new Configuration(pair.configuration));
            });
            let childrenConfigs = [];
            Object.keys(mappedChildrenConfigs).forEach(key => {
                childrenConfigs.push(mappedChildrenConfigs[key]);
            });
            childrenConfigs.sort(function(a, b) {return a.compare(b);});

            // Create the collapsed timelines, cobine results
            let allResults = [];
            let collapsedTimelines = [];
            childrenConfigs.forEach(config => {
                childrenConfigsBySDK[config.toKey()].sort(function(a, b) {return a.compareSDKs(b);});

                let resultsForConfig = [];
                childrenConfigsBySDK[config.toKey()].forEach(sdkConfig => {
                    resultsForConfig = combineResults(resultsForConfig, resultsByKey[sdkConfig.toKey()]);
                });
                allResults = combineResults(allResults, resultsForConfig);

                let queueParams = config.toParams();
                queueParams['suite'] = [this.suite];
                if (branch)
                    queueParams['branch'];
                let myTimeline = Timeline.SeriesWithHeaderComponent(
                    `${childrenConfigsBySDK[config.toKey()].length > 1 ? ' | ' : ''}<a href="/urls/queue?${paramsToQuery(queueParams)}" target="_blank">${config}</a>`,
                    Timeline.CanvasSeriesComponent(resultsForConfig, scale, {
                        getScaleFunc: options.getScaleFunc,
                        compareFunc: options.compareFunc,
                        renderFactory: options.renderFactory,
                        exporter: options.exporter,
                        onDotClick: onDotClickFactory(config),
                        onDotEnter: onDotEnterFactory(config),
                        onDotLeave: onDotLeave,
                        onDotsSelected: dots => {
                            if (dots.length)
                                this.selectedDots.set(config, dots);
                            else
                                this.selectedDots.delete(config);
                        },
                        exporter: exporterFactory(resultsForConfig),
                    }));

                if (childrenConfigsBySDK[config.toKey()].length > 1) {
                    let timelinesBySDK = [];
                    childrenConfigsBySDK[config.toKey()].forEach(sdkConfig => {
                        timelinesBySDK.push(
                            Timeline.SeriesWithHeaderComponent(`${Configuration.integerToVersion(sdkConfig.version)} (${sdkConfig.sdk})`,
                                Timeline.CanvasSeriesComponent(resultsByKey[sdkConfig.toKey()], scale, {
                                    getScaleFunc: options.getScaleFunc,
                                    compareFunc: options.compareFunc,
                                    renderFactory: options.renderFactory,
                                    exporter: options.exporter,
                                    onDotClick: onDotClickFactory(sdkConfig),
                                    onDotEnter: onDotEnterFactory(sdkConfig),
                                    onDotLeave: onDotLeave,
                                    onDotsSelected: dots => {
                                        if (dots.length)
                                            this.selectedDots.set(sdkConfig, dots);
                                        else
                                            this.selectedDots.delete(sdkConfig);
                                    },
                                    exporter: exporterFactory(resultsByKey[sdkConfig.toKey()]),
                                })));
                    });
                    myTimeline = Timeline.ExpandableSeriesWithHeaderExpanderComponent(myTimeline, {}, ...timelinesBySDK);
                }
                collapsedTimelines.push(myTimeline);
            });

            if (collapsedTimelines.length === 0)
                return;
            if (collapsedTimelines.length === 1) {
                if (!collapsedTimelines[0].header.includes('class="series"'))
                    collapsedTimelines[0].header = Timeline.HeaderComponent(collapsedTimelines[0].header);
                children.push(collapsedTimelines[0]);
                return;
            }

            children.push(
                Timeline.ExpandableSeriesWithHeaderExpanderComponent(
                Timeline.SeriesWithHeaderComponent(` ${configuration}`,
                    Timeline.CanvasSeriesComponent(allResults, scale, {
                        getScaleFunc: options.getScaleFunc,
                        compareFunc: options.compareFunc,
                        renderFactory: options.renderFactory,
                        onDotClick: onDotClickFactory(configuration),
                        onDotEnter: onDotEnterFactory(configuration),
                        onDotLeave: onDotLeave,
                        onDotsSelected: dots => {
                            if (dots.length)
                                this.selectedDots.set(configuration, dots);
                            else
                                this.selectedDots.delete(configuration);
                        },
                        exporter: exporterFactory(allResults),
                    })),
                {expanded: this.configurations.length <= 1},
                ...collapsedTimelines
            ));
        });

        let top = true;
        self.xaxisUpdates = [];
        this.repositories = repositoriesForCommits(commits);
        this.repositories.forEach(repository => {
            const xAxisComponent = xAxisFromScale(scale, repository, self.xaxisUpdates, top, self.viewport);
            if (top)
                children.unshift(xAxisComponent);
            else
                children.push(xAxisComponent);
            top = false;
        });

        const composer = FP.composer(FP.currying((updateTimeline, notifyRerender) => {
            self.timelineUpdate = (xAxises) => {
                children.splice(0, 1);
                if (self.repositories.length > 1)
                    children.splice(children.length - self.repositories.length, self.repositories.length);

                let top = true;
                xAxises.forEach(component => {
                    if (top)
                        children.unshift(component);
                    else
                        children.push(component);
                    top = false;
                });
                updateTimeline(children);
            };
            self.notifyRerender = notifyRerender;
        }));
        return Timeline.CanvasContainer({
            customizedLayer: `<div style="position:absolute" ref="${this.selectedDotsButtonGroupRef}"></div>`,
            onSelecting: (e) => {
                this.selectedDotsButtonGroupRef.setState({show: false});
            },
            onSelect: (dots, selectedDotRect, seriesRect, e) => {
                // this api will called with selected dots for each series once, and compose the selectedDotRect during the call
                this.selectedDotsButtonGroupRef.setState({show: true, top: selectedDotRect.bottom, left: selectedDotRect.right});
            },
            onSelectionScroll: (dots, selectedDotRect) => {
                // this api will called with selected dots for each series once, and compose the selectedDotRect during the call
                this.selectedDotsButtonGroupRef.setState({top: selectedDotRect.bottom, left: selectedDotRect.right});
            },
        }, composer, ...children);
    }
}


function LegendLabel(eventStream, filterExpectedText, filterUnexpectedText) {
    let ref = REF.createRef({
        state: willFilterExpected,
        onStateUpdate: (element, state) => {
            if (state) element.innerText = filterExpectedText;
            else element.innerText = filterUnexpectedText;
        }
    });
    eventStream.action((willFilterExpected) => ref.setState(willFilterExpected));
    return `<div class="label" style="font-size: var(--smallSize)" ref="${ref}"></div>`;
} 

function Legend(callback=null, plural=false, defaultWillFilterExpected=false, flakinessSwitch=false, numberOfFlakySwitch=false) {
    willFilterExpected = defaultWillFilterExpected;
    InvestigateDrawer.willFilterExpected = willFilterExpected;
    let updateLabelEvents = new EventStream();
    const legendDetails = {
        success: {
            expected: plural ? 'No unexpected results' : 'Result expected',
            unexpected: plural ? 'All tests passed' : 'Test passed',
        },
        warning: {
            expected: plural ? 'Some tests unexpectedly reported warnings' : 'Unexpected warning',
            unexpected: plural ? 'Some tests reported warnings' : 'Test warning',
        },
        failed: {
            expected: plural ? 'Some tests unexpectedly failed' : 'Unexpectedly failed',
            unexpected: plural ? 'Some tests failed' : 'Test failed',
        },
        timedout: {
            expected: plural ? 'Some tests unexpectedly timed out' : 'Unexpectedly timed out',
            unexpected: plural ? 'Some tests timed out' : 'Test timed out',
        },
        crashed: {
            expected: plural ? 'Some tests unexpectedly crashed' : 'Unexpectedly crashed',
            unexpected: plural ? 'Some tests crashed' : 'Test crashed',
        },
    };
    let result = `<div class="legend horizontal">
            ${Object.keys(legendDetails).map((key) => {
                const dot = REF.createRef({
                    onElementMount: (element) => {
                        element.addEventListener('mouseleave', (event) => {
                            if (!ToolTip.isIn({x: event.x, y: event.y}))
                                ToolTip.unset();
                        });
                        element.onmouseover = (event) => {
                            if (!element.classList.contains('disabled'))
                                return;
                            ToolTip.setByElement(
                                `<div class="content">
                                    ${willFilterExpected ? legendDetails[key].expected : legendDetails[key].unexpected}
                                </div>`,
                                element,
                                {orientation: ToolTip.HORIZONTAL},
                            );
                        };
                    }
                });
                return `<div class="item">
                        <div class="dot ${key}" ref="${dot}"><div class="text">${Expectations.symbolMap[key]}</div></div>
                        ${LegendLabel(updateLabelEvents, legendDetails[key].expected, legendDetails[key].unexpected)}
                    </div>`
            }).join('')}
        </div>`;

    if (callback) {
        const filterSwitch = REF.createRef({
            onElementMount: (element) => {
                element.onchange = () => {
                    if (element.checked)
                        willFilterExpected = true;
                    else
                        willFilterExpected = false;
                    updateLabelEvents.add(willFilterExpected);
                    InvestigateDrawer.willFilterExpected = willFilterExpected;
                    InvestigateDrawer.dispatch();
                    InvestigateDrawer.select(InvestigateDrawer.selected);
                    callback(willFilterExpected);
                };
            },
        });
        const showTimesSwitch = REF.createRef({
            onElementMount: (element) => {
                element.onchange = () => {
                    if (element.checked)
                        showTestTimes = true;
                    else
                        showTestTimes = false;
                    callback();
                };
            },
        });
        const showTestFlakinessSwitch = REF.createRef({
            onElementMount: (element) => {
                element.onchange = () => {
                    if (element.checked)
                        showTestFlakiness = true;
                    else
                        showTestFlakiness = false;
                    callback();
                };
            },
        });
        const showNumberOfFlakyTestsSwitch = REF.createRef({
            onElementMount: (element) => {
                element.onchange = () => {
                    if (element.checked)
                        showNumberOfFlakyTests = true;
                    else
                        showNumberOfFlakyTests = false;
                    callback();
                };
            },
        });
        result += `<div class="input">
            <label>Filter expected results</label>
            <label class="switch">
                <input type="checkbox"${willFilterExpected ? ' checked': ''} ref="${filterSwitch}">
                <span class="slider"></span>
            </label>
        </div>`
        if (!plural)
            result += `<div class="input">
                <label>Show test times</label>
                <label class="switch">
                    <input type="checkbox"${showTestTimes ? ' checked': ''} ref="${showTimesSwitch}">
                    <span class="slider"></span>
                </label>
            </div>`;
        if (flakinessSwitch)
            result += `<div class="input">
                <label>Show test flakiness</label>
                <label class="switch">
                    <input type="checkbox"${showTestFlakiness ? ' checked': ''} ref="${showTestFlakinessSwitch}">
                    <span class="slider"></span>
                </label>
            </div>`;
        if (numberOfFlakySwitch)
            result += `<div class="input">
                <label>Number of flakes</label>
                <label class="switch">
                    <input type="checkbox"${showNumberOfFlakyTests ? ' checked': ''} ref="${showNumberOfFlakyTestsSwitch}">
                    <span class="slider"></span>
                </label>
            </div>`;
    }

    return `${result}`;
}

export {Legend, TimelineFromEndpoint, Expectations};

