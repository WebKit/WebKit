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

WI.AuditManager = class AuditManager extends WI.Object
{
    constructor()
    {
        super();

        this._tests = [];
        this._results = [];

        this._runningState = WI.AuditManager.RunningState.Inactive;
        this._runningTests = [];

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
    }

    static synthesizeError(message)
    {
        let consoleMessage = new WI.ConsoleMessage(WI.mainTarget, WI.ConsoleMessage.MessageSource.Other, WI.ConsoleMessage.MessageLevel.Error, WI.UIString("Audit error: %s").format(message));
        consoleMessage.shouldRevealConsole = true;

        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
    }

    // Public

    get tests() { return this._tests; }
    get results() { return this._results; }
    get runningState() { return this._runningState; }

    async start(tests)
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Inactive);
        if (this._runningState !== WI.AuditManager.RunningState.Inactive)
            return;

        if (tests && tests.length)
            tests = tests.filter((test) => typeof test === "object" && test instanceof WI.AuditTestBase);
        else
            tests = this._tests;

        if (!tests.length)
            return;

        this._runningState = WI.AuditManager.RunningState.Active;
        this._runningTests = tests;
        for (let test of this._runningTests)
            test.clearResult();

        this.dispatchEventToListeners(WI.AuditManager.Event.TestScheduled);

        await Promise.chain(this._runningTests.map((test) => () => this._runningState === WI.AuditManager.RunningState.Active ? test.start() : null));

        let result = this._runningTests.map((test) => test.result).filter((result) => !!result);

        this._runningState = WI.AuditManager.RunningState.Inactive;
        this._runningTests = [];

        this._addResult(result);
    }

    stop()
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Active);
        if (this._runningState !== WI.AuditManager.RunningState.Active)
            return;

        for (let test of this._runningTests)
            test.stop();

        this._runningState = WI.AuditManager.RunningState.Stopping;
    }

    async processJSON({json, error})
    {
        if (error) {
            WI.AuditManager.synthesizeError(error);
            return;
        }

        let object = await WI.AuditTestGroup.fromPayload(json) || await WI.AuditTestCase.fromPayload(json);
        if (!object) {
            object = await WI.AuditTestGroupResult.fromPayload(json) || await WI.AuditTestCaseResult.fromPayload(json);
            if (!object) {
                WI.AuditManager.synthesizeError(WI.UIString("invalid JSON."));
                return;
            }
        }

        if (object instanceof WI.AuditTestBase) {
            this._addTest(object);
            WI.objectStores.audits.addObject(object);
        } else if (object instanceof WI.AuditTestResultBase)
            this._addResult(object);

        WI.showRepresentedObject(object);
    }

    export(object)
    {
        console.assert(object instanceof WI.AuditTestCase || object instanceof WI.AuditTestGroup || object instanceof WI.AuditTestCaseResult || object instanceof WI.AuditTestGroupResult, object);

        let filename = object.name;
        if (object instanceof WI.AuditTestResultBase)
            filename = WI.UIString("%s Result").format(filename);

        let url = "web-inspector:///" + encodeURI(filename) + ".json";

        WI.FileUtilities.save({
            url,
            content: JSON.stringify(object),
            forceSaveAs: true,
        });
    }

    loadStoredTests()
    {
        if (this._tests.length)
            return;

        WI.objectStores.audits.getAll().then(async (tests) => {
            for (let payload of tests) {
                let test = await WI.AuditTestGroup.fromPayload(payload) || await WI.AuditTestCase.fromPayload(payload);
                if (!test)
                    continue;

                const key = null;
                WI.objectStores.audits.associateObject(test, key, payload);

                this._addTest(test);
            }

            this.addDefaultTestsIfNeeded();
        });
    }

    removeTest(test)
    {
        this._tests.remove(test);

        this.dispatchEventToListeners(WI.AuditManager.Event.TestRemoved, {test});

        WI.objectStores.audits.deleteObject(test);
    }

    // Private

    _addTest(test)
    {
        this._tests.push(test);

        this.dispatchEventToListeners(WI.AuditManager.Event.TestAdded, {test});
    }

    _addResult(result)
    {
        if (!result || (Array.isArray(result) && !result.length))
            return;

        this._results.push(result);

        this.dispatchEventToListeners(WI.AuditManager.Event.TestCompleted, {
            result,
            index: this._results.length - 1,
        });
    }

    _handleFrameMainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        for (let test of this._tests)
            test.clearResult();
    }

    addDefaultTestsIfNeeded()
    {
        if (this._tests.length)
            return;

        const defaultTests = [
            new WI.AuditTestGroup(WI.UIString("Demo Audit"), [
                new WI.AuditTestGroup(WI.UIString("Result Levels"), [
                    new WI.AuditTestCase(`level-pass`, `function() { return {level: "pass"}; }`, {description: WI.UIString("This is what the result of a passing test with no data looks like.")}),
                    new WI.AuditTestCase(`level-warn`, `function() { return {level: "warn"}; }`, {description: WI.UIString("This is what the result of a warning test with no data looks like.")}),
                    new WI.AuditTestCase(`level-fail`, `function() { return {level: "fail"}; }`, {description: WI.UIString("This is what the result of a failing test with no data looks like.")}),
                    new WI.AuditTestCase(`level-error`, `function() { return {level: "error"}; }`, {description: WI.UIString("This is what the result of a test that threw an error with no data looks like.")}),
                    new WI.AuditTestCase(`level-unsupported`, `function() { return {level: "unsupported"}; }`, {description: WI.UIString("This is what the result of a unsupported test with no data looks like.")}),
                ], {description: WI.UIString("These are all of the different test result levels.")}),
                new WI.AuditTestGroup(WI.UIString("Result Data"), [
                    new WI.AuditTestCase(`data-domNodes`, `function() { return {domNodes: [document.body], level: "pass"}; }`, {description: WI.UIString("This is an example of how result DOM nodes are shown. It will pass with the <body> element.")}),
                    new WI.AuditTestCase(`data-domAttributes`, `function() { return {domNodes: Array.from(document.querySelectorAll("[id]")), domAttributes: ["id"], level: "pass"}; }`, {description: WI.UIString("This is an example of how result DOM nodes are shown. It will pass with all elements with an id attribute.")}),
                    new WI.AuditTestCase(`data-errors`, `function() { throw Error("this error was thrown from inside the audit test code."); }`, {description: WI.UIString("This is an example of how errors are shown. The error was thrown manually, but execution errors will appear in the same way.")}),
                ], {description: WI.UIString("These are all of the different types of data that can be returned with the test result.")}),
            ], {description: WI.UIString("These tests serve as a demonstration of the functionality and structure of audits.")}),
            new WI.AuditTestGroup(WI.UIString("Accessibility"), [
                new WI.AuditTestGroup(WI.UIString("Attributes"), [
                    new WI.AuditTestCase(`img-alt`, `function() { let domNodes = Array.from(document.getElementsByTagName("img")).filter((img) => !img.alt || !img.alt.length); return { level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["alt"] }; }`, {description: WI.UIString("Ensure <img> elements have alternate text.")}),
                    new WI.AuditTestCase(`area-alt`, `function() { let domNodes = Array.from(document.getElementsByTagName("area")).filter((area) => !area.alt || !area.alt.length); return { level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["alt"] }; }`, {description: WI.UIString("Ensure <area> elements of image maps have alternate text.")}),
                    new WI.AuditTestCase(`valid-tabindex`, `function() { let domNodes = Array.from(document.querySelectorAll("*[tabindex]")) .filter((node) => { let tabindex = node.getAttribute("tabindex"); if (!tabindex) return false; tabindex = parseInt(tabindex); return isNaN(tabindex) || (tabindex !== 0 && tabindex !== -1); }); return { level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["tabindex"] }; }`, {description: WI.UIString("Ensure tabindex is a number.")}),
                    new WI.AuditTestCase(`frame-title`, `function() { let domNodes = Array.from(document.querySelectorAll("iframe, frame")) .filter((node) => { let title = node.getAttribute("title"); return !title || !title.trim().length; }); return { level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["title"] }; }`, {description: WI.UIString("Ensure <area> elements of image maps have alternate text.")}),
                    new WI.AuditTestCase(`hidden-body`, `function() { let domNodes = Array.from(document.querySelectorAll("body[hidden]")).filter((body) => body.hidden); return { level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["hidden"] }; }`, {description: WI.UIString("Ensure hidden=true is not present on the document body.")}),
                    new WI.AuditTestCase(`meta-refresh`, `function() { let domNodes = Array.from(document.querySelectorAll("meta[http-equiv=refresh]")); return { level: domNodes.length ? "warn" : "pass", domNodes, domAttributes: ["http-equiv"] }; }`, {description: WI.UIString("Ensure <meta http-equiv=refresh> is not used.")}),
                ], {description: WI.UIString("Tests for element attribute accessibility issues.")}),
                new WI.AuditTestGroup(WI.UIString("Elements"), [
                    new WI.AuditTestCase(`blink`, `function() { let domNodes = Array.from(document.getElementsByTagName("blink")); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure hidden=true is not present on the document body.")}),
                    new WI.AuditTestCase(`marquee`, `function() { let domNodes = Array.from(document.getElementsByTagName("marquee")); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure hidden=true is not present on the document body.")}),
                    new WI.AuditTestCase(`dlitem`, `function() { function check(node) { if (!node) { return false; } if (node.nodeName === "DD") { return true; } return check(node.parentNode); } let domNodes = Array.from(document.querySelectorAll("dt, dd")).filter(check); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure <dt> and <dd> elements are contained by a <dl>.")}),
                ], {description: WI.UIString("Tests for element accessibility issues.")}),
                new WI.AuditTestGroup(WI.UIString("Forms"), [
                    new WI.AuditTestCase(`one-legend`, `function() { let formLegendsMap = Array.from(document.querySelectorAll("form legend")).reduce((accumulator, node) => { let existing = accumulator.get(node.form); if (!existing) { existing = []; accumulator.set(node.form, existing); } existing.push(node); return accumulator; }, new Map); let domNodes = Array.from(formLegendsMap.values()).reduce((accumulator, legends) => accumulator.concat(legends), []); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure exactly one <legend> exists per form.")}),
                    new WI.AuditTestCase(`legend-first-child`, `function() { let domNodes = Array.from(document.querySelectorAll("form > legend:not(:first-child)")); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure legend is first child in form.")}),
                    new WI.AuditTestCase(`form-input`, `function() { let domNodes = Array.from(document.getElementsByTagName("form")) .filter(node => !node.elements.length); return { level: domNodes.length ? "warn" : "pass", domNodes }; }`, {description: WI.UIString("Ensure forms have at least one input.")}),
                ], {description: WI.UIString("Tests the accessibility of form elements.")}),
            ], {description: WI.UIString("Tests for ways to improve accessibility.")}),
        ];

        for (let test of defaultTests) {
            this._addTest(test);
            WI.objectStores.audits.addObject(test);
        }
    }
};

WI.AuditManager.RunningState = {
    Inactive: "inactive",
    Active: "active",
    Stopping: "stopping",
};

WI.AuditManager.Event = {
    TestAdded: "audit-manager-test-added",
    TestCompleted: "audit-manager-test-completed",
    TestRemoved: "audit-manager-test-removed",
    TestScheduled: "audit-manager-test-scheduled",
};
