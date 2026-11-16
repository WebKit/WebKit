"use strict";

/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
*/

const defaultEmptyMap = Object.freeze({});

class Params {
    // Enable a detailed developer menu to change the current Params.
    developerMode = false;
    startAutomatically = false;
    report = false;
    startDelay = undefined;

    testList = [];
    testIterationCount = undefined;
    testWorstCaseCount = undefined;
    prefetchResources = true;

    // Display group details.
    groupDetails = false

    RAMification = false;
    forceGC = false;
    dumpJSONResults = false;
    dumpTestList = false;
    // Override iteration and worst-case counts per workload.
    // Example:
    //   testIterationCountMap = { "acorn-wtb": 5 };
    testIterationCountMap = defaultEmptyMap;
    testWorstCaseCountMap = defaultEmptyMap;

    customPreIterationCode = undefined;
    customPostIterationCode = undefined;

    constructor(sourceParams = undefined) {
        if (sourceParams)
            this._copyFromSearchParams(sourceParams);
        if (!this.developerMode)
            Object.freeze(this);
    }

    _copyFromSearchParams(sourceParams) {
        this.startAutomatically = this._parseBooleanParam(sourceParams, "startAutomatically");
        this.developerMode = this._parseBooleanParam(sourceParams, "developerMode");
        this.report = this._parseBooleanParam(sourceParams, "report");
        this.prefetchResources = this._parseBooleanParam(sourceParams, "prefetchResources");
        this.RAMification = this._parseBooleanParam(sourceParams, "RAMification");
        this.dumpJSONResults = this._parseBooleanParam(sourceParams, "dumpJSONResults");
        this.groupDetails = this._parseBooleanParam(sourceParams, "groupDetails");
        this.dumpTestList = this._parseBooleanParam(sourceParams, "dumpTestList");
        this.forceGC = this._parseBooleanParam(sourceParams, "forceGC");

        this.customPreIterationCode = this._parseStringParam(sourceParams, "customPreIterationCode");
        this.customPostIterationCode = this._parseStringParam(sourceParams, "customPostIterationCode");

        this.startDelay = this._parseIntParam(sourceParams, "startDelay", 0);
        if (!this.startDelay) {
            if (this.report)
                 this.startDelay = 4000;
            if (this.startAutomatically)
                 this.startDelay = 100;
        }

        this.testList = this._parseOneOf(sourceParams, ["testList", "tag", "tags", "test", "tests"], this._parseTestListParam);
        this.testIterationCount = this._parseOneOf(sourceParams, ["testIterationCount", "iterationCount", "iterations" ], this._parseIntParam, 1);
        this.testWorstCaseCount = this._parseOneOf(sourceParams, ["testWorstCaseCount", "worstCaseCount", "worst"], this._parseIntParam, 1);

        const unused = Array.from(sourceParams.keys());
        if (unused.length > 0)
            console.error("Got unused source params", unused);
    }

    _parseOneOf(sourceParams, paramKeys, parseFunction, ...args) {
        const defaultParamKey = paramKeys[0]
        let result = undefined;
        let parsedParamKey = undefined;
        for (const paramKey of paramKeys) {
            if (!sourceParams.has(paramKey)) {
                continue;
            }
            const parseResult = parseFunction.call(this, sourceParams, paramKey, ...args);
            if (parsedParamKey) {
                throw new Error(`Cannot parse ${paramKey}, overriding previous "${parsedParamKey}" value ${JSON.stringify(result)} with ${JSON.stringify(parseResult)}`)
            }
            parsedParamKey = paramKey;
            result = parseResult;
        }
        if (!parsedParamKey) {
            return DefaultJetStreamParams[defaultParamKey];
        }
        return result;
    }

    _parseTestListParam(sourceParams, key) {
        if (!sourceParams.has(key))
            return this.testList;
        let testList = [];
        if (sourceParams?.getAll) {
            for (const param of sourceParams?.getAll(key)) {
                testList.push(...param.split(","));
            }
        } else {
            // fallback for cli sourceParams which is just a Map;
            testList = sourceParams.get(key).split(",");
        }
        testList = testList.map(each => each.trim());
        sourceParams.delete(key);
        return testList;
    }

    _parseStringParam(sourceParams, paramKey) {
        if (!sourceParams.has(paramKey))
            return DefaultJetStreamParams[paramKey];
        const value = sourceParams.get(paramKey);
        sourceParams.delete(paramKey);
        return value;
    }

    _parseBooleanParam(sourceParams, paramKey) {
        if (!sourceParams.has(paramKey))
            return DefaultJetStreamParams[paramKey];
        const rawValue = sourceParams.get(paramKey);;
        sourceParams.delete(paramKey);
        const value = rawValue.toLowerCase();
        return !(value === "false" || value === "0");
    }

    _parseIntParam(sourceParams, paramKey, minValue) {
        if (!sourceParams.has(paramKey))
            return DefaultJetStreamParams[paramKey];

        const parsedValue = this._parseInt(sourceParams.get(paramKey), paramKey);
        if (parsedValue < minValue)
            throw new Error(`Invalid ${paramKey} param: '${parsedValue}', value must be >= ${minValue}.`);
        sourceParams.delete(paramKey);
        return parsedValue;
    }

    _parseInt(value, errorMessage) {
        const number = Number(value);
        if (!Number.isInteger(number) && errorMessage)
            throw new Error(`Invalid ${errorMessage} param: '${value}', expected int.`);
        return parseInt(number);
    }

    get isDefault() {
      return this === DefaultJetStreamParams;
    }

    get nonDefaultParams() {
        const diff = Object.create(null);
        for (const [key, value] of Object.entries(this)) {
            const defaultValue = DefaultJetStreamParams[key]
            if (value == defaultValue) continue;
            if (value?.length == 0 && defaultValue?.length == 0) continue;
            diff[key] = value;
        }
        return diff;
    }
}

const DefaultJetStreamParams = new Params();
let maybeCustomParams = DefaultJetStreamParams;
if (globalThis?.JetStreamParamsSource) {
    try {
        maybeCustomParams = new Params(globalThis?.JetStreamParamsSource);
        // We might have parsed the same values again, do a poor-mans deep-equals:
        if (JSON.stringify(maybeCustomParams) === JSON.stringify(DefaultJetStreamParams)) {
           maybeCustomParams = DefaultJetStreamParams 
        }
    } catch (e) {
        console.error("Invalid Params", e, "\nUsing defaults as fallback:", maybeCustomParams);
    }
}
const JetStreamParams = maybeCustomParams;
