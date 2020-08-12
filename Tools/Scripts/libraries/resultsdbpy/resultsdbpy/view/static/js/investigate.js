// Copyright (C) 2020 Apple Inc. All rights reserved.
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
import {DOM, REF} from '/library/js/Ref.js';
import {CommitBank} from '/assets/js/commit.js';
import {queryToParams, paramsToQuery, QueryModifier, percentage, elapsedTime} from '/assets/js/common.js';
import {Configuration} from '/assets/js/configuration.js'
import {Expectations} from '/assets/js/expectations.js';
import {Failures} from '/assets/js/failures.js';

function commitsForUuid(uuid) {
    return `Commits: ${CommitBank.commitsDuring(uuid).map((commit) => {
            const params = {
                branch: commit.branch ? [commit.branch] : branch,
                uuid: [commit.uuid],
            }
            if (!params.branch)
                delete params.branch;
            const query = paramsToQuery(params);
            return `<a href="/commit/info?${query}" target="_blank">${commit.id.substring(0,12)}</a>`;
        }).join(', ')}`
}

function parametersForInstance(suite, data)
{
    const branch = queryToParams(document.URL.split('?')[1]).branch;
    const buildParams = data.configuration.toParams();
    buildParams['suite'] = [suite];
    buildParams['uuid'] = [data.uuid];
    buildParams['after_time'] = [data.start_time];
    buildParams['before_time'] = [data.start_time];
    if (branch)
        buildParams['branch'] = branch;
    return paramsToQuery(buildParams);
}

function testRunLink(suite, data)
{
    if (!data.start_time)
        return '';
    return `<a href="/urls/build?${parametersForInstance(suite, data)}" target="_blank">Test run</a> @ ${new Date(data.start_time * 1000).toLocaleString()}`;
}

function archiveLink(suite, data)
{
    if (!data.start_time || !ArchiveRouter.hasArchive(suite))
        return '';
    return `<a href="/archive/${ArchiveRouter.pathFor(suite)}?${parametersForInstance(suite, data)}" target="_blank">${ArchiveRouter.labelFor(suite)}</a>`;
}

function elapsed(data)
{
    if (data.time)
        return `Took ${data.time / 1000} seconds`;
    if (data.stats && data.stats.start_time && data.stats.end_time)
        return `Suite took ${elapsedTime(data.stats.start_time, data.stats.end_time)}`;
    return '';
}

function prioritizedFailures(failures, max = 0, willFilterExpected = false)
{
    if (failures === undefined)
        return '';

    if (failures === null)
        return `<div class="loader">
                <div class="spinner"></div>
            </div>`;
    let failuresToDisplay = [];
    Expectations.failureTypes.forEach(type => {
        for (let testName in failures[type]) {
            failuresToDisplay.push({
                failure: type,
                test: testName,
                count: failures.aggregating,
                failureCount: failures[type][testName],
            });
        }
    });
    failuresToDisplay.sort(function(a, b) {
        const typeCompare = Expectations.stringToStateId(Expectations.failureTypeMap[a.failure]) - Expectations.stringToStateId(Expectations.failureTypeMap[b.failure]);
        if (typeCompare)
            return typeCompare;
        if (a.failureCount - b.failureCount)
            return a.failureCount - b.failureCount;
        return (a.test).localeCompare(b.test);
    });

    if (max && failuresToDisplay.length > max) {
        failuresToDisplay = failuresToDisplay.splice(0, max);
        failuresToDisplay[max - 1] = null;
    }



    return `<div>${failuresToDisplay.map(failure => {
        if (failure === null) {
            const params = failures.toParams();
            params.unexpected = [willFilterExpected];
            return `<a style="margin-left: calc(var(--mediumSize) + 16px)" target="_blank" href="/investigate?${paramsToQuery(params)}">More...</a>`;
        }
        return `<div>
                <div class="dot ${failure.failure} small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[failure.failure]}</div>
                </div>
                <a class="text block" style="width: calc(100% - var(--mediumSize) - 16px); overflow: hidden; white-space: nowrap; text-overflow: ellipsis;" href="/?suite=${failures.suite}&test=${failure.test}">${failure.test}</a>
            </div>`;
    }).join('')}</div>`;
}

function resultsForData(data, willFilterExpected = false)
{
    const result = [];
    let testsRan = 1;
    let totalTests = 1;
    if (data.stats && data.stats.tests_run) {
        testsRan = data.stats.tests_run;
        totalTests = data.stats.tests_run + (data.stats.tests_skipped ? data.stats.tests_skipped : 0);
    }
    let dotsDisplayed = 0;
    result.push(`<div>Ran ${testsRan.toLocaleString()} of ${totalTests.toLocaleString()} tests`);
    if (data.actual) {
        const type = Expectations.typeForId(data.actual);
        ++dotsDisplayed;
        result.push(`Actual: ${data.actual}
            <div class="dot ${type} small">
                <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
            </div>`);
    }
    if (data.expected) {
        const type = Expectations.typeForId(data.expected);
        ++dotsDisplayed;
        result.push(`Expected: ${data.expected}
            <div class="dot ${type} small">
                <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
            </div>`);
    }
    if (data.stats && data.stats.tests_run) {
        const succeeded = data.stats.tests_run - data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}failed`];
        if (succeeded) {
            ++dotsDisplayed;
            result.push(`<div class="dot success small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap.success}</div>
                </div>
                ${data.start_time ? succeeded.toLocaleString() : percentage(succeeded, data.stats.tests_run)} passed`);
        }
    }
    for (let i = 0; i < Expectations.failureTypes.length; i++) {
        if (!data.stats)
            continue;
        const type = Expectations.failureTypes[i];
        let value = data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}${type}`];
        if (i < Expectations.failureTypes.length - 1)
            value -= data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}${Expectations.failureTypes[i + 1]}`];
        if (!value)
            continue;
        ++dotsDisplayed;
        result.push(`<div class="dot ${type} small">
                <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
            </div>
            ${data.start_time ? value.toLocaleString() : percentage(value, data.stats.tests_run)} ${type}`);
    }
    result.push('</div>');
    result.push(prioritizedFailures(data.failures, Math.max(6 - dotsDisplayed, 1), willFilterExpected));
    return result;
}

function renderInvestigateDrawer(arrays)
{
    return `<div class="row">
            ${arrays.map(array => {
                return `<div class="col-s-6 list">
                        ${array.map(element => {
                            return `<div class="item">${element}</div>`;
                        }).join('')}
                    </div>`;
            }).join('<div class="divider mobile-control"></div>')}
        </div>`
}

function contentForAgregateData(suite, agregateData, data, willFilterExpected = false)
{
    const metaData = [
        `${data.length} reports for ${agregateData.configuration}`,
        commitsForUuid(agregateData.uuid),
    ];
    let count = 0;
    data.forEach(node => {
        const myCount = count;
        let dotType = 'success';
        if (node.stats) {
            Expectations.failureTypes.forEach(type => {
                if (node.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}${type}`] > 0)
                    dotType = type;
            });
        } else {
            let resultId = Expectations.stringToStateId(node.actual);
            if (willFilterExpected)
                resultId = Expectations.stringToStateId(Expectations.unexpectedResults(node.actual, node.expected));
            Expectations.failureTypes.forEach(type => {
                if (Expectations.stringToStateId(Expectations.failureTypeMap[type]) >= resultId)
                    dotType = type;
            });
        }
        metaData.push(`<div>
                <div class="dot ${dotType} small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[dotType]}</div>
                </div>
                <a class="link-button" ref="${REF.createRef({
                    onElementMount: (element) => {
                        element.onclick = () => InvestigateDrawer.select(myCount + 1);
                    },
                })}">
                    ${node.configuration}
                </a>
            </div>`);
        ++count;
    });
    return renderInvestigateDrawer([metaData, resultsForData(agregateData, willFilterExpected)]);
}

function contentForData(suite, data, willFilterExpected = false)
{
    const metaData = [
        data.configuration,
        commitsForUuid(data.uuid),
        testRunLink(suite, data),
        archiveLink(suite, data),
        elapsed(data),
    ];

    return renderInvestigateDrawer([metaData, resultsForData(data, willFilterExpected)]);
}

class _InvestigateDrawer {
    constructor() {
        this.ref = null;
        this.content = null;
        this.close = null;
        this.previous = null;
        this.next = null;

        this.selected = 0;
        this.suite = null;
        this.agregate = null;
        this.data = [];

        this.willFilterExpected = false;
    }
    isRendered() {
        let result = true;
        ['ref', 'content', 'close', 'previous', 'next'].forEach(element => {
            if (!result)
                return;
            result = this[element] && this[element].element;
        });
        if (!result)
            console.error('Investigation drawer not rendered');
        return result;
    }
    toString() {
        const self = this;
        this.ref = REF.createRef();
        this.content = REF.createRef({
            state: '',
            onStateUpdate: (element, state) => {
                DOM.inject(element, state);
            },
        });
        this.close = REF.createRef({
            onElementMount: (element) => {
                element.onclick = () => self.collapse();
            },
        });

        this.previous = REF.createRef({
            onElementMount: (element) => {
                element.onclick = () => self.select(self.selected - 1);
            },
        });
        this.next = REF.createRef({
            onElementMount: (element) => {
                element.onclick = () => self.select(self.selected + 1);
            },
        });

        return `<div class="drawer main right-sidebar" ref="${this.ref}" style="z-index: 20">
                <div class="content unselectable" style="display: flex; justify-content: space-between; flex-direction: row; padding: 10px;">
                    <div style="width: 150px; text-align: left">
                        <a class="button" style="cursor: pointer" ref="${this.previous}">◀ Previous</a>
                    </div>
                    <div>
                        <a class="button" style="cursor: pointer" ref="${this.close}">Close</a>
                    </div>
                    <div style="width: 150px; text-align: right">
                        <a class="button" style="cursor: pointer" ref="${this.next}">Next ▶</a>
                    </div>
                </div>
                <div class="content" ref="${this.content}">
                </div>
            </div>`;
    }
    expand(suite, agregateData, allData) {
        if (!this.isRendered())
            return;
        this.ref.element.classList.add('display');
        this.suite = suite;
        this.agregate = agregateData;
        this.data = allData;
        this.dispatch();
        this.select(0);
    }
    dispatch() {
        if (!this.data.length)
            return;
        for (let i = 0; i < this.data.length; i++) {
            if (!this.data[i].stats)
                return;
        }

        this.data.forEach(datum => {
            datum.failures = null;
        });
        this.agregate.failures = null;

        const branch = queryToParams(document.URL.split('?')[1]).branch;
        const params = {
            unexpected: [this.willFilterExpected],
            uuid: [this.agregate.uuid],
        }
        if (this.agregate.start_time) {
            params.before_time = [this.agregate.start_time];
            params.after_time = [this.agregate.start_time];
        }
        if (branch)
            params.branch = [branch];

        Failures.fromEndpoint(
            this.suite,
            this.agregate.configuration,
            params,
        ).then(failures => {
            this.agregate.failures = Failures.combine(...failures);
            this.data.forEach(datum => {
                datum.failures = new Failures(this.suite, datum.configuration);
                failures.forEach(failure => {
                    if (datum.configuration.compare(failure.configuration) === 0)
                        datum.failures = Failures.combine(datum.failures, failure);
                });
            });
            this.select(this.selected);
        });
    }
    collapse() {
        if (!this.isRendered())
            return;
        this.ref.element.classList.remove('display');

        this.agregate = null;
        this.data = [];
        this.select(0);
    }
    select(index) {
        if (!this.ref)
            return;

        let candidates = this.data.length;
        if (this.agregate && this.data.length > 1)
            candidates += 1;
        if (!candidates) {
            this.ref.element.classList.remove('display');
            return;
        }

        // Force selection in bounds
        if (index < 0)
            index = 0;
        if (index >= candidates)
            index = candidates - 1;
        this.selected = index;

        // Display next/previous buttons
        if (index)
            this.previous.element.style.display = null;
        else
            this.previous.element.style.display = 'none';
        if (index === candidates - 1)
            this.next.element.style.display = 'none';
        else
            this.next.element.style.display = null;

        if (this.agregate && this.data.length > 1 && !this.selected)
            this.content.setState(contentForAgregateData(this.suite, this.agregate, this.data, this.willFilterExpected));
        else
            this.content.setState(contentForData(
                this.suite,
                this.data[this.agregate && this.data.length > 1 ? this.selected - 1 : this.selected],
                this.willFilterExpected,
            ));
    }
}

const InvestigateDrawer = new _InvestigateDrawer();

export {InvestigateDrawer};
