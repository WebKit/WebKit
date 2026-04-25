// Copyright (C) 2024 Apple Inc. All rights reserved.
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

import {intersection, paramsToQuery, queryToParams, mergeQueries, ErrorDisplay} from '/assets/js/common.js';
import {CommitBank} from '/assets/js/commit.js'
import {Configuration} from '/assets/js/configuration.js';
import {Expectations} from '/assets/js/expectations.js';
import {DOM, REF} from '/library/js/Ref.js';
import {TypeForSuite} from '/assets/js/suites.js';

let dashboards = new Set();

class Dashboard {
    static setWillFilterExpected(value) {
        dashboards.forEach(dashboard => {
            if (dashboard.willFilterExpected != value) {
                dashboard.willFilterExpected = value;
                dashboard.setTilesFromData();
            }
        });
    }

    static refreshCommits() {
        dashboards.forEach(dashboard => {
            dashboard.setTilesFromData();
        });
    }

    constructor(title, suite, parameters, warnAfter=null, expireAfter=null) {
        this.title = title;
        this.suite = suite;
        this.parameters = parameters;

        this.warnAfter = warnAfter ? warnAfter : 12;
        this.expireAfter = expireAfter ? expireAfter : 7*24;

        this.willFilterExpected = false;

        this.requestData = null;
        this.redundentParameters = null;

        this.commit_bank = null;

        this.ref = REF.createRef({
            state: {displayed: this.canEnable()},
            onStateUpdate: (element, state) => {
                element.style.display = state.displayed ? null : 'none';
            },
        });
        this.title_ref = REF.createRef({
            state: {status: null},
            onStateUpdate: (element, state) => {
                if (state.status)
                    element.innerHTML = `<div class="dot ${state.status} small">
                        <div class="tiny text" style="font-weight: normal;margin-top: 0px">
                            ${Expectations.symbolMap[state.status]}
                        </div>
                    </div>
                    ${this.title}`;
                else
                    element.innerHTML = `<div class="spinner tiny"></div> ${this.title}`;
            },
        });

        this.showPassing = false;
        this.showPassingSwitch = REF.createRef({
            onElementMount: (element) => {
                element.onchange = () => {
                    this.showPassing = element.checked;
                    this.setTilesFromData();
                };
            },
        });

        const typ = TypeForSuite(this.suite);
        this.content = REF.createRef({
            state: {tiles: null},
            onStateUpdate: (element, state) => {
                if (state.error)
                    DOM.inject(element, ErrorDisplay(state));
                else if (state.tiles) {
                    let parameters = {...this.parameters};
                    parameters.suite = [this.suite];
                    DOM.inject(element, `<div style="display: grid; gap: 10px; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); grid-template-rows: masonry;">
                        ${state.tiles.map(tile => {
                            let queueParameters = {...parameters};
                            const configParameters = tile.configuration.toParams();
                            for (const key in configParameters)
                                queueParameters[key] = configParameters[key];

                            return `<div class="badge">
                                <table class="table">
                                    <thead>
                                        <tr>
                                            <th>
                                                <div class="dot ${tile.status} small">
                                                    <div class="small text" style="font-weight: normal;margin-top: 0px">
                                                        ${Expectations.symbolMap[tile.status]}
                                                    </div>
                                                </div>
                                            </th>
                                            <th>
                                                <a class="text small clickable" href="suites?recent=False&${paramsToQuery(queueParameters)}" target="_blank">
                                                    ${tile.configuration.toString()}
                                                </a>
                                            </th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        <tr>
                                            <td></td>
                                            <td>
                                                <a class="text tiny clickable" href="urls/build?${paramsToQuery(mergeQueries(queueParameters, {uuid: [tile.builds[0].uuid], after_time: [tile.builds[0].start_time], before_time: [tile.builds[0].start_time]}))}" target="_blank">
                                                    ${typ.runDescription} @ ${new Date(tile.builds[0].start_time * 1000).toLocaleString()}
                                                </a>
                                            </td>
                                        </tr>
                                        ${tile.builds.map(build => {
                                            if (build.sdks)
                                                return `<tr>
                                                        <td></td>
                                                        <td>
                                                            <a class="text tiny clickable" href="suites?recent=False&${paramsToQuery(mergeQueries(queueParameters, {sdk: build.sdks}))}" target="_blank">
                                                                ${build.sdks[0]} -> ${build.sdks[1]}
                                                            </a>
                                                        </td>
                                                    </tr>`;

                                            if (build.range)
                                                return `<tr>
                                                        <td></td>
                                                        <td>
                                                            <a class="text tiny clickable" href="suites?recent=False&${paramsToQuery(mergeQueries(queueParameters, {after_uuid: [build.range[0]], before_uuid: [build.range[1]]}))}" target="_blank">
                                                                ${build.count} ${typ.runDescription}${build.count > 1 ? 's' : ''}...
                                                            </a>
                                                        </td>
                                                    </tr>`;

                                            if (!build.status)
                                                return `<tr>
                                                        <td></td>
                                                        <td>${Object.keys(build).sort().map(key => {
                                                            // FIXME: We should link these commit ranges directly to the SCM providers
                                                            return `<a class="text tiny clickable" href="commits?${paramsToQuery({repository_id: [build[key][0].repository_id], branch: this.parameters.branch ? this.parameters.branch : [], after_ref: [build[key][0].label()], before_ref: [build[key][build[key].length - 1].label()]})}" target="_blank">
                                                                    ${build[key].length} ${key} change${build[key].length > 1 ? 's' : ''}
                                                                </a>`;
                                                        }).join(', ')}</td>
                                                    </tr>`;

                                            return `<tr>
                                                    <td>
                                                        <a class="clickable dot ${build.status} tiny" style="color: var(--white)" href="urls/build?${paramsToQuery(mergeQueries(queueParameters, {uuid: [build.uuid], after_time: [build.start_time], before_time: [build.start_time]}))}" target="_blank">
                                                            ${Expectations.symbolMap[build.status]}
                                                        </a>
                                                    </td>
                                                    <td>
                                                        ${(build.commits.length ? build.commits : ['?']).map(commit => {
                                                            if (typeof commit === 'string')
                                                                return `<a class="clickable text tiny" href="commit?${paramsToQuery({branch: this.parameters.branch ? this.parameters.branch : [], uuid: [build.uuid]})}" target="_blank">${commit}</a>`;
                                                            return `<a class="clickable text tiny" href="commit/info?${paramsToQuery({branch: this.parameters.branch ? this.parameters.branch : [], repository_id: [commit.repository_id], ref: [commit.label()]})}" target="_blank">${commit.label()}</a>`;
                                                        }).join(', ')}
                                                    </td>
                                                </tr>`;
                                        }).join('')}
                                    </tbody>
                                </table>
                            </div>`;
                        }).join('')}
                    </div>`);
                } else
                    DOM.inject(element, '<div class="loader"><div class="spinner"></div></div>');
            },
        });

        dashboards.add(this);
        this.reload();
    }
    documentParams() {
        let result = queryToParams(document.URL.split('?')[1]);
        delete result.suite;
        delete result.test;
        return result;
    }
    canEnable() {
        const globalParams = this.documentParams();

        // Specific check for suite
        if (Object.hasOwn(globalParams, 'suite') && !globalParams.suite.includes(this.suite))
            return false;

        // Specific check for branch
        if (Object.hasOwn(globalParams, 'branch')) {
            if (!Object.hasOwn(this.parameters, 'branch'))
                return false;
        }
        for (const key in globalParams) {
            if (!Object.hasOwn(this.parameters, key))
                continue;
            if (!intersection(this.parameters[key], globalParams[key]).length)
                return false;
        }
        return true;
    }
    setEnabled(enabled) {
        const wasEnabled = this.ref.state.displayed;
        enabled = enabled && this.canEnable();

        this.ref.setState({displayed: enabled})
        if (!wasEnabled && enabled)
            this.reload();
    }
    reload() {
        this.setEnabled(this.ref.state.displayed);
        if (!this.ref.state.displayed)
            return;

        this.title_ref.setState({status: null});

        let parameters = mergeQueries(this.parameters, this.documentParams());
        if (!Object.hasOwn(parameters, 'limit'))
            parameters.limit = ['150'];

        if (Object.hasOwn(this.parameters, 'branch'))
            this.commit_bank = CommitBank.forBranch(this.parameters.branch);
        else
            this.commit_bank = CommitBank;
        if (this.commit_bank.callbacks.indexOf(Dashboard.refreshCommits) < 0)
            this.commit_bank.callbacks.push(Dashboard.refreshCommits)

        fetch(`/api/results/${this.suite}?${paramsToQuery(parameters)}`).then(response => {
            response.json().then(json => {
                let results = {};
                let uniqueParameters = {};
                Configuration.members().forEach(member => {
                    uniqueParameters[member] = new Set();
                });

                json.forEach(node => {
                    let strippedConfig = new Configuration(node.configuration);
                    strippedConfig.sdk = null;

                    Configuration.members().forEach(member => {
                        if (strippedConfig[member])
                            uniqueParameters[member].add(strippedConfig[member]);
                    });

                    if (!results.hasOwnProperty(strippedConfig.toKey())) {
                        results[strippedConfig.toKey()] = [];
                    }
                    const config = new Configuration(node.configuration);
                    let index = Math.max(results[strippedConfig.toKey()].length - 1, 0);
                    node.results.forEach(result => {
                        let direction = 0;
                        while (index < results[strippedConfig.toKey()].length) {
                            let aKey = results[strippedConfig.toKey()][index][1].uuid;
                            let bKey = result.uuid;
                            if (aKey == bKey) {
                                aKey = results[strippedConfig.toKey()][index][1].start_time;
                                bKey = result.start_time;
                            }

                            if (aKey < bKey) {
                                if (direction < 0)
                                    break;
                                direction = 1;
                                index += 1;
                            }
                            else if (index && aKey > bKey) {
                                if (direction > 0)
                                    break;
                                direction = -1;
                                index -= 1;
                            }
                            else
                                break;
                        }
                        results[strippedConfig.toKey()].splice(index, 0, [config, result]);
                    });
                });

                this.redundentParameters = new Set();
                for (const member in uniqueParameters) {
                    // Never declare version or version_name redundent
                    if (member == 'version' || member == 'version_name')
                        continue
                    if (!uniqueParameters[member].size || (uniqueParameters[member].size == 1 && [true, false].indexOf(uniqueParameters[member].values().next().value) < 0))
                        this.redundentParameters.add(member);
                }

                this.requestData = results;
                this.setTilesFromData();
            }).catch(error => {
                this.title_ref.setState({status: 'crashed'});
                this.content.setState({
                    error: 'Failed to Decode',
                    description: `Failed to decode history of '${this.title}', and cannot construct dashboard`,
                });
            });
        }).catch(error => {
            this.title_ref.setState({status: 'crashed'});
            this.content.setState({
                error: 'Failed to Fetch',
                description: `Failed to fetch history of '${this.title}', and cannot construct dashboard`,
            });
        });
    }
    failureForBuild(build) {
        let result = Expectations.stringToStateId('PASS');
        Expectations.failureTypes.forEach(type => {
            const idForType = Expectations.stringToStateId(Expectations.failureTypeMap[type]);
            const key = this.willFilterExpected ? `tests_unexpected_${type}` : `tests_${type}`;
            if (build.stats[key])
                result = Math.min(result, idForType);
        });
        return result;
    }
    tileLineForBuild(build) {
        return {
            status: Expectations.typeForId(this.failureForBuild(build)),
            start_time: build.start_time,
            uuid: build.uuid,
            commits: this.commit_bank.commitsDuring(build.uuid).sort((a, b) => {
                return a.repository_id.localeCompare(b.repository_id);
            }),
        };
    }
    setTilesFromData() {
        if (!this.ref.state.displayed || !this.requestData)
            return;

        const globalParams = this.documentParams()
        let now = Math.floor(Date.now() / 1000);
        if (Object.hasOwn(globalParams, 'before_time'))
            now = Math.min(now, globalParams.before_time[0]);
        if (Object.hasOwn(globalParams, 'before_timestamp'))
            now = Math.min(now, globalPaarams.before_timestamp[0]);
        if (Object.hasOwn(globalParams, 'before_uuid'))
            now = Math.min(now, Math.floor(globalParams.before_uuid[0] / 100));
        if (Object.hasOwn(globalParams, 'before_ref')) {
            let commit_bank = CommitBank;
            if (Object.hasOwn(globalParams, 'branch'))
                commit_bank = CommitBank.forBranch(globalParams.branch);
            let doesContain = false;
            for (const commit of commit_bank.commits) {
                if (commit.identifier == globalParams.before_ref[0] || commit.revision == globalParams.before_ref[0] || commit.hash.startsWith(globalParams.before_ref[0])) {
                    now = Math.min(now, commit.timestamp);
                    doesContain = true;
                    break;
                }
            }
            if (!doesContain && commit_bank.commits)
                commit_bank.addCommit(globalParams.before_ref[0])
        }

        let oldestUuid = now * 100;
        let totalStatus = Expectations.stringToStateId('PASS');
        let tiles = [];
        for (const key in this.requestData) {
            if (!this.requestData[key] || !this.requestData[key].length)
                continue;

            const latestResult = this.requestData[key][this.requestData[key].length - 1][1];
            if (latestResult.uuid / 100 < now - this.expireAfter * 60 * 60)
                continue;

            // For now, do the easy thing and only consider the latest run
            // In the future, we can consider more sophisticated algorithms
            const latestStatus = this.failureForBuild(latestResult);
            let configStatus = latestStatus;
            
            if (latestResult.uuid / 100 < now - this.warnAfter * 60 * 60)
                configStatus = Math.min(configStatus, Expectations.stringToStateId('WARNING'));;
            if (!this.showPassing && configStatus >= Expectations.stringToStateId('PASS'))
                continue

            totalStatus = Math.min(totalStatus, configStatus);
            let strippedConfig = new Configuration(this.requestData[key][this.requestData[key].length - 1][0]);
            this.redundentParameters.forEach(member => {
                strippedConfig[member] = null;
            });
            let tile = {
                configuration: strippedConfig,
                status: Expectations.typeForId(configStatus),
                status_id: configStatus,
                builds: [this.tileLineForBuild(latestResult)],
            };

            let count = this.requestData[key].length - 2;
            let previous = null;
            let previousSdk = this.requestData[key][this.requestData[key].length - 1][0];
            while (count >= 0) {
                const sdk = this.requestData[key][count][0];
                const build = this.requestData[key][count][1];
                if (this.failureForBuild(build) >= Expectations.stringToStateId('WARNING')) {
                    if (previous) {
                        if (count < this.requestData[key].length - 3)
                            tile.builds.push({
                                count: this.requestData[key].length - 3 - count,
                                range: [build.uuid, latestResult.uuid],
                            });
                        tile.builds.push(this.tileLineForBuild(previous));
                    }

                    if (previousSdk && sdk.sdk != previousSdk.sdk)
                        tile.builds.push({sdks: [sdk.sdk, previousSdk.sdk]});

                    let candidateCommits = this.commit_bank.commitsDuring(build.uuid, (previous ? previous : latestResult).uuid);
                    let regressionCommits = {};
                    let maxFromRepo = 0;
                    candidateCommits.forEach(commit => {
                        if (commit.uuid <= build.uuid)
                            return;
                        if (!regressionCommits[commit.repository_id])
                            regressionCommits[commit.repository_id] = [];
                        regressionCommits[commit.repository_id].push(commit)
                        maxFromRepo = Math.max(maxFromRepo, regressionCommits[commit.repository_id].length)
                    });
                    if (maxFromRepo > 1)
                        tile.builds.push(regressionCommits);
                    tile.builds.push(this.tileLineForBuild(build));
                    break;
                }
                previous = build;
                previousSdk = sdk;
                count -= 1;
            }
            oldestUuid = Math.min(oldestUuid, this.requestData[key][count < 0 ? 0 : count][1].uuid);

            tiles.push(tile);
        }

        this.commit_bank.add(oldestUuid, now * 100);

        tiles.sort((a, b) => {
            if (a.status_id != b.status_id)
                return a.status_id - b.status_id;
            return a.configuration.compare(b.configuration);
        });

        if (Object.keys(this.requestData).length) {
            this.title_ref.setState({status: Expectations.typeForId(totalStatus)});
            this.content.setState({tiles: tiles});
        } else {
            this.title_ref.setState({status: 'warning'});
            this.content.setState({
                error: 'No History',
                description: `Provided parameters for '${this.title}' yield no history`,
            });
        }
    }
    toString() {
        let parameters = mergeQueries(this.parameters, this.documentParams());
        parameters.suite = [this.suite];

        return `<div class="section" ref="${this.ref}">
            <div class="header row">
                <a class="title" ref="${this.title_ref}" href="suites?${paramsToQuery(parameters)}">
                    ${this.title}
                </a>
            </div>
            <div class="content">
                <div class="input" style="width: 180px">
                    <label>Show passing</label>
                    <label class="switch">
                        <input type="checkbox" ref="${this.showPassingSwitch}">
                        <span class="slider"></span>
                    </label>
                </div>
                <div ref="${this.content}"> </div>
            </div>
        </div>`;
    }
}

export {Dashboard};
