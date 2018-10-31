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

WI.AuditTestCase = class AuditTestCase extends WI.AuditTestBase
{
    constructor(name, test, {description} = {})
    {
        console.assert(typeof test === "string");

        super(name, {description});

        this._test = test;
    }

    // Static

    static async fromPayload(payload)
    {
        if (typeof payload !== "object" || payload === null)
            return null;

        let {type, name, test, description} = payload;

        if (type !== WI.AuditTestCase.TypeIdentifier)
            return null;

        if (typeof name !== "string")
            return null;

        if (typeof test !== "string")
            return null;

        let options = {};
        if (typeof description === "string")
            options.description = description;

        return new WI.AuditTestCase(name, test, options);
    }

    // Public

    get test() { return this._test; }

    toJSON()
    {
        let json = super.toJSON();
        json.test = this._test;
        return json;
    }

    // Protected

    async run()
    {
        const levelStrings = Object.values(WI.AuditTestCaseResult.Level);
        let level = null;
        let data = {};
        let metadata = {
            url: WI.networkManager.mainFrame.url,
            startTimestamp: null,
            endTimestamp: null,
        };

        function setLevel(newLevel) {
            let newLevelIndex = levelStrings.indexOf(newLevel);
            if (newLevelIndex < 0) {
                addError(WI.UIString("Return string must be one of %s").format(JSON.stringify(levelStrings)));
                return;
            }

            if (newLevelIndex <= levelStrings.indexOf(level))
                return;

            level = newLevel;
        }

        function addError(value) {
            setLevel(WI.AuditTestCaseResult.Level.Error);

            if (!data.errors)
                data.errors = [];

            data.errors.push(value);
        }

        let evaluateArguments = {
            expression: `(function() { "use strict"; return eval(${this._test})(); })()`,
            objectGroup: "audit",
            doNotPauseOnExceptionsAndMuteConsole: true,
        };

        try {
            metadata.startTimestamp = new Date;
            let evaluateResponse = await RuntimeAgent.evaluate.invoke(evaluateArguments);
            metadata.endTimestamp = new Date;

            let remoteObject = WI.RemoteObject.fromPayload(evaluateResponse.result, WI.mainTarget);
            if (evaluateResponse.wasThrown || (remoteObject.type === "object" && remoteObject.subtype === "error"))
                addError(remoteObject.description);
            else if (remoteObject.type === "boolean")
                setLevel(remoteObject.value ? WI.AuditTestCaseResult.Level.Pass : WI.AuditTestCaseResult.Level.Fail);
            else if (remoteObject.type === "string")
                setLevel(remoteObject.value.trim().toLowerCase());
            else if (remoteObject.type === "object" && !remoteObject.subtype) {
                const options = {
                    ownProperties: true,
                };

                let properties = await new Promise((resolve, reject) => remoteObject.getPropertyDescriptorsAsObject(resolve, options));

                function checkResultProperty(key, type, subtype) {
                    if (!(key in properties))
                        return null;

                    let property = properties[key].value;
                    if (!property)
                        return null;

                    function addErrorForValueType(valueType) {
                        let value = null;
                        if (valueType === "object" || valueType === "array")
                            value = WI.UIString("“%s“ must be an %s");
                        else
                            value = WI.UIString("“%s“ must be a %s");
                        addError(value.format(key, valueType));
                    }

                    if (property.subtype !== subtype) {
                        addErrorForValueType(subtype);
                        return null;
                    }

                    if (property.type !== type) {
                        addErrorForValueType(type);
                        return null;
                    }

                    if (type === "boolean" || type === "string")
                        return property.value;

                    return property;
                }

                async function resultArrayForEach(key, callback) {
                    let array = checkResultProperty(key, "object", "array");
                    if (!array)
                        return;

                    // `getPropertyDescriptorsAsObject` returns an object, meaning that if we
                    // want to iterate over `array` by index, we have to count.
                    let asObject = await new Promise((resolve, reject) => array.getPropertyDescriptorsAsObject(resolve, options));
                    for (let i = 0; i < array.size; ++i) {
                        if (i in asObject)
                            await callback(asObject[i]);
                    }
                }

                let levelString = checkResultProperty("level", "string");
                if (levelString)
                    setLevel(levelString.trim().toLowerCase());

                if (checkResultProperty("pass", "boolean"))
                    setLevel(WI.AuditTestCaseResult.Level.Pass);
                if (checkResultProperty("warn", "boolean"))
                    setLevel(WI.AuditTestCaseResult.Level.Warn);
                if (checkResultProperty("fail", "boolean"))
                    setLevel(WI.AuditTestCaseResult.Level.Fail);
                if (checkResultProperty("error", "boolean"))
                    setLevel(WI.AuditTestCaseResult.Level.Error);
                if (checkResultProperty("unsupported", "boolean"))
                    setLevel(WI.AuditTestCaseResult.Level.Unsupported);

                await resultArrayForEach("domNodes", async (item) => {
                    if (!item || !item.value || item.value.type !== "object" || item.value.subtype !== "node") {
                        addError(WI.UIString("All items in “%s“ must be valid DOM nodes").format(WI.unlocalizedString("domNodes")));
                        return;
                    }

                    let domNodeId = await new Promise((resolve, reject) => item.value.pushNodeToFrontend(resolve));
                    let domNode = WI.domManager.nodeForId(domNodeId);
                    if (!domNode)
                        return;

                    if (!data.domNodes)
                        data.domNodes = [];
                    data.domNodes.push(domNode);
                });

                await resultArrayForEach("domAttributes", (item) => {
                    if (!item || !item.value || item.value.type !== "string" || !item.value.value.length) {
                        addError(WI.UIString("All items in “%s“ must be non-empty strings").format(WI.unlocalizedString("domAttributes")));
                        return;
                    }

                    if (!data.domAttributes)
                        data.domAttributes = [];
                    data.domAttributes.push(item.value.value);
                });

                await resultArrayForEach("errors", (item) => {
                    if (!item || !item.value || item.value.type !== "object" || item.value.subtype !== "error") {
                        addError(WI.UIString("All items in “%s“ must be error objects").format(WI.unlocalizedString("errors")));
                        return;
                    }

                    addError(item.value.description);
                });
            } else
                addError(WI.UIString("Return value is not an object, string, or boolean"));
        } catch (error) {
            metadata.endTimestamp = new Date;
            addError(error.message);
        }

        if (!level)
            addError(WI.UIString("Missing result level"));

        let options = {
            description: this.description,
            metadata,
        };
        if (!isEmptyObject(data))
            options.data = data;
        this._result = new WI.AuditTestCaseResult(this.name, level, options);
    }
};

WI.AuditTestCase.TypeIdentifier = "test-case";
