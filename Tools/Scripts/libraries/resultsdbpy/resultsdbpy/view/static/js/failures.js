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

import {TIMESTAMP_TO_UUID_MULTIPLIER} from '/assets/js/commit.js';
import {queryToParams, paramsToQuery, QueryModifier} from '/assets/js/common.js';
import {Expectations} from '/assets/js/expectations.js';
import {Configuration} from '/assets/js/configuration.js';

class Failures {
    static fromEndpoint(suite, configuration=null, params={})
    {
        return new Promise(function(resolve, reject) {
            const paramsToDispatch = Object.assign(configuration ? configuration.toParams() : {}, params);
            paramsToDispatch.collapsed = [false];
            if (paramsToDispatch.unexpected === undefined)
                paramsToDispatch.unexpected = [false];

            fetch(`api/failures/${suite}?${paramsToQuery(paramsToDispatch)}`).then(response => {
                let result = [];
                response.json().then(json => {
                    if (!Array.isArray(json))
                        return [];
                    json.forEach((pairs) => {
                        pairs.results.forEach((run) => {
                            result.push(new Failures(suite, new Configuration(pairs.configuration), run));
                        });
                    });
                    resolve(result);
                });
            }).catch(error => {
                // If the load fails, log the error and continue
                console.error(JSON.stringify(error, null, 4));
                reject(error);
            });
        });
    }

    static combine()
    {
        if (!arguments.length)
            return null;

        const result = new Failures(arguments[0].suite);
        const configurations = [];

        for (let i = 0; i < arguments.length; ++i) {
            configurations.push(arguments[i].configuration);
            result.aggregating += arguments[i].aggregating;
            result.start_time_range = [
                Math.min(result.start_time_range[0], arguments[i].start_time_range[0]),
                Math.max(result.start_time_range[1], arguments[i].start_time_range[1]),
            ];
            result.uuid_range = [
                Math.min(result.uuid_range[0], arguments[i].uuid_range[0]),
                Math.max(result.uuid_range[1], arguments[i].uuid_range[1]),
            ];

            // Combine
            Expectations.failureTypes.forEach(type => {
                for (let test in arguments[i][type]) {
                    if (result[type][test] === undefined)
                        result[type][test] = arguments[i][type][test];
                    else
                        result[type][test] += arguments[i][type][test];
                }
            });

            // Prioritize failures
            const remainingTypes = [...Expectations.failureTypes];
            while (remainingTypes.length > 1) {
                const toRemove = [];
                const currentType = remainingTypes[0];
                remainingTypes.shift();

                for (let test in result[currentType]) {
                    let worstType = currentType;
                    remainingTypes.forEach(type => {
                        if (result[type][test] !== undefined)
                            worstType = type;
                    });
                    if (worstType == currentType)
                        continue;
                    result[worstType][test] += result[currentType][test];
                    toRemove.push(test);
                }
                toRemove.forEach(test => {
                    delete result[currentType][test];
                });
            }
        }
        result.configuration = Configuration.combine(...configurations);
        return result;
    }

    constructor(suite, configuration = null, json = null) {
        this.suite = suite;
        this.configuration = configuration ? new Configuration(configuration) : new Configuration();

        Expectations.failureTypes.forEach(type => {
            this[type] = {};
        });
        if (json === null) {
            this.aggregating = 0;
            const currentTime = Math.floor(Date.now() / 1000);
            this.start_time_range = [currentTime, 0];
            this.uuid_range = [currentTime * TIMESTAMP_TO_UUID_MULTIPLIER, 0];
            return;
        }

        this.aggregating = 1;
        this.start_time_range = [json.start_time, json.start_time];
        this.uuid_range = [json.uuid, json.uuid];
        for (var parameter in json) {
            if (parameter === 'uuid' || parameter === 'start_time')
                continue;
            const type = Expectations.typeForId(json[parameter]);
            this[type][parameter] = 1;
        }
    }
    toParams() {
        const params = this.configuration.toParams();
        params.suite = [this.suite];
        if (this.uuid_range[0] <= this.uuid_range[1]) {
            if (this.uuid_range[0] === this.uuid_range[1])
                params.uuid = [this.uuid_range[0]];
            else {
                params.after_uuid = [this.uuid_range[0]];
                params.before_uuid = [this.uuid_range[1]];
            }
        }
        if (this.start_time_range[0] <= this.start_time_range[1]) {
            params.after_time = [this.start_time_range[0]];
            params.before_time = [this.start_time_range[1]];
        }
        return params;
    }
};

export {Failures};
