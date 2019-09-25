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

import {DOM, REF} from '/library/js/Ref.js';
import {CommitBank} from '/assets/js/commit.js';
import {queryToParams, paramsToQuery, QueryModifier, } from '/assets/js/common.js';
import {Configuration} from '/assets/js/configuration.js'
import {Expectations} from '/assets/js/expectations.js';

function commitsForUuid(uuid) {
    return `Commits: ${CommitBank.commitsDuringUuid(uuid).map((commit) => {
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

function testRunLink(suite, data)
{
    if (!data.start_time)
        return '';
    const branch = queryToParams(document.URL.split('?')[1]).branch;
    return `<a href="/urls/build?${paramsToQuery(function () {
        const buildParams = data.configuration.toParams();
        buildParams['suite'] = [suite];
        buildParams['uuid'] = [data.uuid];
        buildParams['after_time'] = [data.start_time];
        buildParams['before_time'] = [data.start_time];
        if (branch)
            buildParams['branch'] = branch;
        return buildParams;
    } ())}" target="_blank">Test run</a> @ ${new Date(data.start_time * 1000).toLocaleString()}`;
}

function elapsed(data)
{
    if (data.time)
        return `Took ${data.time / 1000} seconds`;
    if (data.stats && data.stats.start_time && data.stats.end_time) {
        const time = new Date((data.stats.end_time - data.stats.start_time) * 1000);
        console.log(data.stats.end_time - data.stats.start_time);
        let result = 'Suite took ';
        if (time.getMinutes())
            result += `${time.getMinutes()}:`;
        result += `${time.getSeconds() <= 9 && time.getMinutes() ? '0' : ''}${time.getSeconds()}.${time.getMilliseconds() <= 99 ? '0' : ''}${time.getMilliseconds() <= 9 ? '0' : ''}${time.getMilliseconds()} `;
        if (time.getMinutes())
            result += 'minutes to run';
        else
            result += 'seconds to run';
        return result;
    }
    return '';
}

function percentage(value, max)
{
    if (value === max)
        return '100 %';
    if (!value)
        return '0 %';
    const result = Math.round(value / max * 100);
    if (!result)
        return '<1 %';
    if (result === 100)
        return '99 %';
    return `${result} %`
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
    result.push(`Ran ${testsRan.toLocaleString()} of ${totalTests.toLocaleString()} tests`);
    if (data.actual) {
        const type = Expectations.typeForId(data.actual);
        result.push(`<div>
                Actual: ${data.actual}
                <div class="dot ${type} small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
                </div>
            </div>`);
    }
    if (data.expected) {
        const type = Expectations.typeForId(data.expected);
        result.push(`<div>
                Expected: ${data.expected}
                <div class="dot ${type} small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
                </div>
            </div>`);
    }
    if (data.stats && data.stats.tests_run) {
        const succeeded = data.stats.tests_run - data.stats[`tests${willFilterExpected ? '_unexpected_' : '_'}failed`];
        if (succeeded)
            result.push(`<div>
                    <div class="dot success small">
                        <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap.success}</div>
                    </div>
                    ${data.start_time ? succeeded.toLocaleString() : percentage(succeeded, data.stats.tests_run)} passed
                </div>`);
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
        result.push(`<div>
                <div class="dot ${type} small">
                    <div class="tiny text" style="font-weight: normal;margin-top: 0px">${Expectations.symbolMap[type]}</div>
                </div>
                ${data.start_time ? value.toLocaleString() : percentage(value, data.stats.tests_run)} ${type}
            </div>`);
    }
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
        this.select(0);
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
