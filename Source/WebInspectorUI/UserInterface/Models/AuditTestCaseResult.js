/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.AuditTestCaseResult = class AuditTestCaseResult extends WI.AuditTestResultBase
{
    constructor(name, level, {description, data, metadata} = {})
    {
        console.assert(Object.values(WI.AuditTestCaseResult.Level).includes(level));
        console.assert(!data || typeof data === "object");
        console.assert(!metadata || typeof metadata === "object");

        super(name, {description});

        this._level = level;
        this._data = data || {};
        this._metadata = metadata || {};
    }

    // Static

    static async fromPayload(payload)
    {
        if (typeof payload !== "object" || payload === null)
            return null;

        let {type, name, description, level, data, metadata} = payload;

        if (type !== WI.AuditTestCaseResult.TypeIdentifier)
            return null;

        if (typeof name !== "string")
            return null;

        if (!Object.values(WI.AuditTestCaseResult.Level).includes(level))
            return null;

        if (typeof data !== "object" || data === null)
            data = {};
        else {
            function checkArray(key) {
                if (!data[key])
                    return;

                if (!Array.isArray(data[key]))
                    data[key] = [];

                data[key] = data[key].filter((item) => typeof item === "string");
            }
            checkArray("domNodes");
            checkArray("domAttributes");
            checkArray("errors");
        }

        if (typeof metadata !== "object" || metadata === null)
            metadata = {};
        else {
            metadata.startTimestamp = typeof metadata.startTimestamp === "string" ? new Date(metadata.startTimestamp) : null;
            metadata.endTimestamp = typeof metadata.endTimestamp === "string" ? new Date(metadata.endTimestamp) : null;
            metadata.url = typeof metadata.url === "string" ? metadata.url : null;
        }

        let options = {};
        if (typeof description === "string")
            options.description = description;
        if (!isEmptyObject(data)) {
            options.data = {};
            if (data.domNodes && data.domNodes.length) {
                if (window.DOMAgent) {
                    let documentNode = await new Promise((resolve) => WI.domManager.requestDocument(resolve));
                    data.domNodes = await Promise.all(data.domNodes.map(async (domNodeString) => {
                        let nodeId = 0;
                        try {
                            nodeId = await WI.domManager.querySelector(documentNode, domNodeString);
                        } catch { }
                        return WI.domManager.nodeForId(nodeId) || domNodeString;
                    }));
                }

                options.data.domNodes = data.domNodes;
            }
            if (data.domAttributes && data.domAttributes.length)
                options.data.domAttributes = data.domAttributes;
            if (data.errors && data.errors.length)
                options.data.errors = data.errors;
        }
        if (!isEmptyObject(metadata)) {
            options.metadata = {};
            if (metadata.startTimestamp && !isNaN(metadata.startTimestamp))
                options.metadata.startTimestamp = metadata.startTimestamp;
            if (metadata.endTimestamp && !isNaN(metadata.endTimestamp))
                options.metadata.endTimestamp = metadata.endTimestamp;
            if (metadata.url)
                options.metadata.url = metadata.url;
        }

        return new WI.AuditTestCaseResult(name, level, options);
    }

    // Public

    get level() { return this._level; }
    get data() { return this._data; }
    get metadata() { return this._metadata; }

    get result()
    {
        return this;
    }

    get didPass()
    {
        return this._level === WI.AuditTestCaseResult.Level.Pass;
    }

    get didWarn()
    {
        return this._level === WI.AuditTestCaseResult.Level.Warn;
    }

    get didFail()
    {
        return this._level === WI.AuditTestCaseResult.Level.Fail;
    }

    get didError()
    {
        return this._level === WI.AuditTestCaseResult.Level.Error;
    }

    get unsupported()
    {
        return this._level === WI.AuditTestCaseResult.Level.Unsupported;
    }

    toJSON()
    {
        let json = super.toJSON();
        json.level = this._level;

        let data = {};
        if (this._data.domNodes && this._data.domNodes.length) {
            data.domNodes = this._data.domNodes.map((domNode) => domNode instanceof WI.DOMNode ? WI.cssPath(domNode, {full: true}) : domNode);
            if (this._data.domAttributes && this._data.domAttributes.length)
                data.domAttributes = this._data.domAttributes;
        }
        if (this._data.errors && this._data.errors.length)
            data.errors = this._data.errors;
        if (!isEmptyObject(data))
            json.data = data;

        let metadata = {};
        if (this._metadata.startTimestamp && !isNaN(this._metadata.startTimestamp))
            metadata.startTimestamp = this._metadata.startTimestamp;
        if (this._metadata.endTimestamp && !isNaN(this._metadata.endTimestamp))
            metadata.endTimestamp = this._metadata.endTimestamp;
        if (this._metadata.url)
            metadata.url = this._metadata.url;
        if (!isEmptyObject(metadata))
            json.metadata = metadata;

        return json;
    }
};

WI.AuditTestCaseResult.TypeIdentifier = "test-case-result";

// Keep this ordered by precedence.
WI.AuditTestCaseResult.Level = {
    Pass: "pass",
    Warn: "warn",
    Fail: "fail",
    Error: "error",
    Unsupported: "unsupported",
};
