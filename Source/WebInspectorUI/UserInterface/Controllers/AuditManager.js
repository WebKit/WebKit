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

        this._disabledDefaultTestsSetting = new WI.Setting("audit-disabled-default-tests", []);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
    }

    // Static

    static synthesizeWarning(message)
    {
        message = WI.UIString("Audit Warning: %s").format(message);

        if (window.InspectorTest) {
            console.warn(message);
            return;
        }

        let consoleMessage = new WI.ConsoleMessage(WI.mainTarget, WI.ConsoleMessage.MessageSource.Other, WI.ConsoleMessage.MessageLevel.Warning, message);
        consoleMessage.shouldRevealConsole = true;

        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
    }

    static synthesizeError(message)
    {
        message = WI.UIString("Audit Error: %s").format(message);

        if (window.InspectorTest) {
            console.error(message);
            return;
        }

        let consoleMessage = new WI.ConsoleMessage(WI.mainTarget, WI.ConsoleMessage.MessageSource.Other, WI.ConsoleMessage.MessageLevel.Error, message);
        consoleMessage.shouldRevealConsole = true;

        WI.consoleLogViewController.appendConsoleMessage(consoleMessage);
    }

    // Public

    get tests() { return this._tests; }
    get results() { return this._results; }
    get runningState() { return this._runningState; }

    get editing()
    {
        return this._runningState === WI.AuditManager.RunningState.Disabled;
    }

    set editing(editing)
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Disabled || this._runningState === WI.AuditManager.RunningState.Inactive);
        if (this._runningState !== WI.AuditManager.RunningState.Disabled && this._runningState !== WI.AuditManager.RunningState.Inactive)
            return;

        let runningState = editing ? WI.AuditManager.RunningState.Disabled : WI.AuditManager.RunningState.Inactive;
        console.assert(runningState !== this._runningState);
        if (runningState === this._runningState)
            return;

        this._runningState = runningState;

        this.dispatchEventToListeners(WI.AuditManager.Event.EditingChanged);

        if (!this.editing) {
            WI.objectStores.audits.clear();

            let disabledDefaultTests = [];
            let saveDisabledDefaultTest = (test) => {
                if (test.supported && test.disabled)
                    disabledDefaultTests.push(test.name);

                if (test instanceof WI.AuditTestGroup) {
                    for (let child of test.tests)
                        saveDisabledDefaultTest(child);
                }
            };

            for (let test of this._tests) {
                if (test.default)
                    saveDisabledDefaultTest(test);
                else
                    WI.objectStores.audits.putObject(test);
            }

            this._disabledDefaultTestsSetting.value = disabledDefaultTests;
        }
    }

    async start(tests)
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Inactive);
        if (this._runningState !== WI.AuditManager.RunningState.Inactive)
            return null;

        if (tests && tests.length)
            tests = tests.filter((test) => typeof test === "object" && test instanceof WI.AuditTestBase);
        else
            tests = this._tests;

        console.assert(tests.length);
        if (!tests.length)
            return null;

        let mainResource = WI.networkManager.mainFrame.mainResource;

        this._runningState = WI.AuditManager.RunningState.Active;
        this.dispatchEventToListeners(WI.AuditManager.Event.RunningStateChanged);

        this._runningTests = tests;
        for (let test of this._runningTests)
            test.clearResult();

        this.dispatchEventToListeners(WI.AuditManager.Event.TestScheduled);

        let target = WI.assumingMainTarget();

        await Promise.chain(this._runningTests.map((test) => async () => {
            if (this._runningState !== WI.AuditManager.RunningState.Active)
                return;

            if (target.hasDomain("Audit"))
                await target.AuditAgent.setup();

            let topLevelTest = test.topLevelTest;
            console.assert(topLevelTest || window.InspectorTest, "No matching top-level test found", test);
            if (topLevelTest)
                await topLevelTest.runSetup();

            await test.start();

            if (target.hasDomain("Audit"))
                await target.AuditAgent.teardown();
        }));

        let result = this._runningTests.map((test) => test.result).filter((result) => !!result);

        this._runningState = WI.AuditManager.RunningState.Inactive;
        this.dispatchEventToListeners(WI.AuditManager.Event.RunningStateChanged);

        this._runningTests = [];

        this._addResult(result);

        if (mainResource !== WI.networkManager.mainFrame.mainResource) {
            // Navigated while tests were running.
            for (let test of this._tests)
                test.clearResult();
        }

        return this._results.lastValue === result ? result : null;
    }

    stop()
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Active);
        if (this._runningState !== WI.AuditManager.RunningState.Active)
            return;

        this._runningState = WI.AuditManager.RunningState.Stopping;
        this.dispatchEventToListeners(WI.AuditManager.Event.RunningStateChanged);

        for (let test of this._runningTests)
            test.stop();
    }

    async processJSON({json, error})
    {
        if (error) {
            WI.AuditManager.synthesizeError(error);
            return;
        }

        if (typeof json !== "object" || json === null) {
            WI.AuditManager.synthesizeError(WI.UIString("invalid JSON"));
            return;
        }

        if (json.type !== WI.AuditTestCase.TypeIdentifier && json.type !== WI.AuditTestGroup.TypeIdentifier
            && json.type !== WI.AuditTestCaseResult.TypeIdentifier && json.type !== WI.AuditTestGroupResult.TypeIdentifier) {
            WI.AuditManager.synthesizeError(WI.UIString("unknown %s \u0022%s\u0022").format(WI.unlocalizedString("type"), json.type));
            return;
        }

        let object = await WI.AuditTestGroup.fromPayload(json) || await WI.AuditTestCase.fromPayload(json) || await WI.AuditTestGroupResult.fromPayload(json) || await WI.AuditTestCaseResult.fromPayload(json);
        if (!object)
            return;

        if (object instanceof WI.AuditTestBase) {
            this.addTest(object);
            WI.objectStores.audits.putObject(object);
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

        WI.FileUtilities.save({
            content: JSON.stringify(object),
            suggestedName: filename + ".json",
            forceSaveAs: true,
        });
    }

    loadStoredTests()
    {
        if (this._tests.length)
            return;

        this._addDefaultTests();

        WI.objectStores.audits.getAll().then(async (tests) => {
            for (let payload of tests) {
                let test = await WI.AuditTestGroup.fromPayload(payload) || await WI.AuditTestCase.fromPayload(payload);
                if (!test)
                    continue;

                const key = null;
                WI.objectStores.audits.associateObject(test, key, payload);

                this.addTest(test);
            }
        });
    }

    addTest(test)
    {
        console.assert(test instanceof WI.AuditTestBase, test);
        console.assert(!this._tests.includes(test), test);

        this._tests.push(test);

        this.dispatchEventToListeners(WI.AuditManager.Event.TestAdded, {test});
    }

    removeTest(test)
    {
        console.assert(this.editing);
        console.assert(test instanceof WI.AuditTestBase, test);
        console.assert(this._tests.includes(test) || test.default, test);

        if (test.default) {
            test.clearResult();

            if (test.disabled) {
                InspectorFrontendHost.beep();
                return;
            }

            test.disabled = true;

            let disabledTests = this._disabledDefaultTestsSetting.value.slice();
            disabledTests.push(test.name);
            this._disabledDefaultTestsSetting.value = disabledTests;

            return;
        }

        console.assert(test.editable, test);

        this._tests.remove(test);

        this.dispatchEventToListeners(WI.AuditManager.Event.TestRemoved, {test});

        WI.objectStores.audits.deleteObject(test);
    }

    // Private

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

        if (this._runningState === WI.AuditManager.RunningState.Active)
            this.stop();
        else {
            for (let test of this._tests)
                test.clearResult();
        }
    }

    _addDefaultTests()
    {
        const levelPass = function() {
            return {level: "pass"};
        };

        const levelWarn = function() {
            return {level: "warn"};
        };

        const levelFail = function() {
            return {level: "fail"};
        };

        const levelError = function() {
            return {level: "error"};
        };

        const levelUnsupported = function() {
            return {level: "unsupported"};
        };

        const dataDOMNodes = function() {
            return {level: "pass", domNodes: [document.body]};
        };

        const dataDOMAttributes = function() {
            return {level: "pass", domNodes: Array.from(document.querySelectorAll("[id]")), domAttributes: ["id"]};
        };

        const dataErrors = function() {
            throw Error("this error was thrown from inside the audit test code.");
        };

        const dataCustom = function() {
            return {level: "pass", a: 1, b: [2], c: {key: 3}};
        };

        const getElementsByComputedRole = function() {
            return {level: "pass", domNodes: WebInspectorAudit.Accessibility.getElementsByComputedRole("link"), domAttributes: ["role"]};
        };

        const getActiveDescendant = function() {
            let result = {level: "pass"};
            let activeDescendant = WebInspectorAudit.Accessibility.getActiveDescendant(document.body);
            if (activeDescendant)
                result.domNodes = [activeDescendant];
            return result;
        };

        const getChildNodes = function() {
            let childNodes = WebInspectorAudit.Accessibility.getChildNodes(document.body);
            return {level: "pass", domNodes: childNodes || []};
        };

        const getComputedProperties = function() {
            let domAttributes = ["alt", "aria-atomic", "aria-busy", "aria-checked", "aria-current", "aria-disabled", "aria-expanded", "aria-haspopup", "aria-hidden", "aria-invalid", "aria-label", "aria-labelledby", "aria-level", "aria-live", "aria-pressed", "aria-readonly", "aria-relevant", "aria-required", "aria-selected", "role", "title"].filter((attribute) => document.body.hasAttribute(attribute));
            let computedProperties = WebInspectorAudit.Accessibility.getComputedProperties(document.body);
            return {level: "pass", domNodes: [document.body], domAttributes, ...(computedProperties || {})};
        };

        const getControlledNodes = function() {
            let controlledNodes = WebInspectorAudit.Accessibility.getControlledNodes(document.body);
            return {level: "pass", domNodes: controlledNodes || []};
        };

        const getFlowedNodes = function() {
            let flowedNodes = WebInspectorAudit.Accessibility.getFlowedNodes(document.body);
            return {level: "pass", domNodes: flowedNodes || []};
        };

        const getMouseEventNode = function() {
            let result = {level: "pass"};
            let mouseEventNode = WebInspectorAudit.Accessibility.getMouseEventNode(document.body);
            if (mouseEventNode)
                result.domNodes = [mouseEventNode];
            return result;
        };

        const getOwnedNodes = function() {
            let ownedNodes = WebInspectorAudit.Accessibility.getOwnedNodes(document.body);
            return {level: "pass", domNodes: ownedNodes || []};
        };

        const getParentNode = function() {
            let result = {level: "pass"};
            let parentNode = WebInspectorAudit.Accessibility.getParentNode(document.body);
            if (parentNode)
                result.domNodes = [parentNode];
            return result;
        };

        const getSelectedChildNodes = function() {
            let selectedChildNodes = WebInspectorAudit.Accessibility.getSelectedChildNodes(document.body);
            return {level: "pass", domNodes: selectedChildNodes || []};
        };

        const hasEventListeners = function() {
            let domAttributes = Array.from(document.body.attributes).filter((attribute) => attribute.name.startsWith("on"));
            return {level: "pass", domNodes: [document.body], domAttributes, hasEventListeners: WebInspectorAudit.DOM.hasEventListeners(document.body)};
        };

        const hasEventListenersClick = function() {
            let domAttributes = ["onclick"].filter((attribute) => document.body.hasAttribute(attribute));
            return {level: "pass", domNodes: [document.body], domAttributes, hasClickEventListener: WebInspectorAudit.DOM.hasEventListeners(document.body, "click")};
        };

        const getResources = function() {
            return {level: "pass", resources: WebInspectorAudit.Resources.getResources()};
        };

        const getResourceContent = function() {
            let resources = WebInspectorAudit.Resources.getResources();
            let mainResource = resources.find((resource) => resource.url === window.location.href);
            return {level: "pass", mainResource, resourceContent: WebInspectorAudit.Resources.getResourceContent(mainResource.id)};
        };

        const unsupported = function() {
            throw Error("this test should not be supported.");
        };

        const testMenuRoleForRequiredChildren = function() {
            const relationships = {
                menu: ["menuitem", "menuitemcheckbox", "menuitemradio"],
                menubar: ["menuitem", "menuitemcheckbox", "menuitemradio"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testGridRoleForRequiredChildren = function() {
            const relationships = {
                grid: ["row", "rowgroup"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testForAriaLabelledBySpelling = function() {
            let domNodes = Array.from(document.querySelectorAll("[aria-labeledby]"));
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-labeledby"]};
        };

        const testForMultipleBanners = function() {
            let domNodes = [];
            let banners = WebInspectorAudit.Accessibility.getElementsByComputedRole("banner");
            if (banners.length > 1)
                domNodes = banners;
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testForLinkLabels = function() {
            let links = WebInspectorAudit.Accessibility.getElementsByComputedRole("link");
            let domNodes = links.filter((link) => !WebInspectorAudit.Accessibility.getComputedProperties(link).label);
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-label", "aria-labelledby", "title"]};
        };

        const testRowGroupRoleForRequiredChildren = function() {
            const relationships = {
                rowgroup: ["row"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testTableRoleForRequiredChildren = function() {
            const relationships = {
                table: ["row", "rowgroup"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testForMultipleLiveRegions = function() {
            const liveRegionRoles = ["alert", "log", "status", "marquee", "timer"];
            let domNodes = [];
            let liveRegions = liveRegionRoles.reduce((a, b) => {
                return a.concat(WebInspectorAudit.Accessibility.getElementsByComputedRole(b));
            }, []);
            liveRegions = liveRegions.concat(Array.from(document.querySelectorAll(`[aria-live="polite"], [aria-live="assertive"]`)));
            if (liveRegions.length > 1)
                domNodes = liveRegions;
            return {level: domNodes.length ? "warn" : "pass", domNodes, domAttributes: ["aria-live"]};
        };

        const testListBoxRoleForRequiredChildren = function() {
            const relationships = {
                listbox: ["option"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testImageLabels = function() {
            let images = WebInspectorAudit.Accessibility.getElementsByComputedRole("img");
            let domNodes = images.filter((image) => !WebInspectorAudit.Accessibility.getComputedProperties(image).label);
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-label", "aria-labelledby", "title", "alt"]};
        };

        const testForAriaHiddenFalse = function() {
            let domNodes = Array.from(document.querySelectorAll(`[aria-hidden="false"]`));
            return {level: domNodes.length ? "warn" : "pass", domNodes, domAttributes: ["aria-hidden"]};
        };

        const testTreeRoleForRequiredChildren = function() {
            const relationships = {
                tree: ["treeitem", "group"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testRadioGroupRoleForRequiredChildren = function() {
            const relationships = {
                radiogroup: ["radio"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testFeedRoleForRequiredChildren = function() {
            const relationships = {
                feed: ["article"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testTabListRoleForRequiredChildren = function() {
            const relationships = {
                tablist: ["tab"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testButtonLabels = function() {
            let buttons = WebInspectorAudit.Accessibility.getElementsByComputedRole("button");
            let domNodes = buttons.filter((button) => !WebInspectorAudit.Accessibility.getComputedProperties(button).label);
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-label", "aria-labelledby", "title"]};
        };

        const testRowRoleForRequiredChildren = function() {
            const relationships = {
                row: ["cell", "gridcell", "columnheader", "rowheader"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testListRoleForRequiredChildren = function() {
            const relationships = {
                list: ["listitem", "group"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "warn" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testComboBoxRoleForRequiredChildren = function() {
            const relationships = {
                combobox: ["textbox", "listbox", "tree", "grid", "dialog"],
            };
            let domNodes = [];
            let visitedParents = new Set;
            function hasChildWithRole(node, expectedRoles) {
                let childNode = node;
                if (!childNode)
                    return false;

                if (childNode.parentNode)
                    visitedParents.add(childNode.parentNode);

                while (childNode) {
                    let properties = WebInspectorAudit.Accessibility.getComputedProperties(childNode);
                    if (childNode.nodeType === Node.ELEMENT_NODE && properties) {
                        if (expectedRoles.includes(properties.role))
                            return true;

                        if (childNode.hasChildNodes() && hasChildWithRole(childNode.firstChild, expectedRoles))
                            return true;
                    }
                    childNode = childNode.nextSibling;
                }
                return false;
            }
            for (let role in relationships) {
                for (let parentNode of WebInspectorAudit.Accessibility.getElementsByComputedRole(role)) {
                    if (visitedParents.has(parentNode))
                        continue;

                    if (!hasChildWithRole(parentNode.firstChild, relationships[role]))
                        domNodes.push(parentNode);
                }
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testForMultipleMainContentSections = function() {
            let domNodes = [];
            let mainContentElements = WebInspectorAudit.Accessibility.getElementsByComputedRole("main");
            if (mainContentElements.length > 1) {
                domNodes = mainContentElements;
            }
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["role"]};
        };

        const testDialogsForLabels = function() {
          let dialogs = WebInspectorAudit.Accessibility.getElementsByComputedRole("dialog");
          let domNodes = dialogs.filter((dialog) => !WebInspectorAudit.Accessibility.getComputedProperties(dialog).label);
          return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-label", "aria-labelledby", "title"]};
        };

        const testForInvalidAriaHiddenValue = function() {
            let domNodes = Array.from(document.querySelectorAll(`[aria-hidden]:not([aria-hidden="true"], [aria-hidden="false"])`));
            return {level: domNodes.length ? "fail" : "pass", domNodes, domAttributes: ["aria-hidden"]};
        };

        function removeWhitespace(func) {
            return WI.AuditTestCase.stringifyFunction(func, 8);
        }

        const defaultTests = [
            new WI.AuditTestGroup(WI.UIString("Demo Audit"), [
                new WI.AuditTestGroup(WI.UIString("Result Levels"), [
                    new WI.AuditTestCase("level-pass", removeWhitespace(levelPass), {description: WI.UIString("This is what the result of a passing test with no data looks like.")}),
                    new WI.AuditTestCase("level-warn", removeWhitespace(levelWarn), {description: WI.UIString("This is what the result of a warning test with no data looks like.")}),
                    new WI.AuditTestCase("level-fail", removeWhitespace(levelFail), {description: WI.UIString("This is what the result of a failing test with no data looks like.")}),
                    new WI.AuditTestCase("level-error", removeWhitespace(levelError), {description: WI.UIString("This is what the result of a test that threw an error with no data looks like.")}),
                    new WI.AuditTestCase("level-unsupported", removeWhitespace(levelUnsupported), {description: WI.UIString("This is what the result of an unsupported test with no data looks like.")}),
                ], {description: WI.UIString("These are all of the different test result levels.")}),
                new WI.AuditTestGroup(WI.UIString("Result Data"), [
                    new WI.AuditTestCase("data-domNodes", removeWhitespace(dataDOMNodes), {description: WI.UIString("This is an example of how result DOM nodes are shown. It will pass with the <body> element.")}),
                    new WI.AuditTestCase("data-domAttributes", removeWhitespace(dataDOMAttributes), {description: WI.UIString("This is an example of how result DOM attributes are highlighted on any returned DOM nodes. It will pass with all elements with an id attribute.")}),
                    new WI.AuditTestCase("data-errors", removeWhitespace(dataErrors), {description: WI.UIString("This is an example of how errors are shown. The error was thrown manually, but execution errors will appear in the same way.")}),
                    new WI.AuditTestCase("data-custom", removeWhitespace(dataCustom), {description: WI.UIString("This is an example of how custom result data is shown."), supports: 3}),
                ], {description: WI.UIString("These are all of the different types of data that can be returned with the test result.")}),
                new WI.AuditTestGroup(WI.UIString("Specially Exposed Data"), [
                    new WI.AuditTestGroup(WI.UIString("Accessibility"), [
                        new WI.AuditTestCase("getElementsByComputedRole", removeWhitespace(getElementsByComputedRole), {description: WI.UIString("This test will pass with all DOM nodes that have a computed role of \u0022link\u0022."), supports: 1}),
                        new WI.AuditTestCase("getActiveDescendant", removeWhitespace(getActiveDescendant), {description: WI.UIString("This test will pass with the active descendant (\u0022%s\u0022) of the <body> element, if it exists.").format(WI.unlocalizedString("aria-activedescendant")), supports: 1}),
                        new WI.AuditTestCase("getChildNodes", removeWhitespace(getChildNodes), {description: WI.UIString("This test will pass with all of the child nodes of the <body> element in the accessibility tree."), supports: 1}),
                        new WI.AuditTestCase("getComputedProperties", removeWhitespace(getComputedProperties), {description: WI.UIString("This test will pass with a variety of accessibility information about the <body> element."), supports: 3}),
                        new WI.AuditTestCase("getControlledNodes", removeWhitespace(getControlledNodes), {description: WI.UIString("This test will pass with all nodes controlled (\u0022%s\u0022) by the <body> element, if any exist.").format(WI.unlocalizedString("aria-controls")), supports: 1}),
                        new WI.AuditTestCase("getFlowedNodes", removeWhitespace(getFlowedNodes), {description: WI.UIString("This test will pass with all nodes flowed to (\u0022%s\u0022) from the <body> element, if any exist.").format(WI.unlocalizedString("aria-flowto")), supports: 1}),
                        new WI.AuditTestCase("getMouseEventNode", removeWhitespace(getMouseEventNode), {description: WI.UIString("This test will pass with the node that would handle mouse events for the <body> element, if applicable."), supports: 1}),
                        new WI.AuditTestCase("getOwnedNodes", removeWhitespace(getOwnedNodes), {description: WI.UIString("This test will pass with all nodes owned (\u0022%s\u0022) by the <body> element, if any exist.").format(WI.unlocalizedString("aria-owns")), supports: 1}),
                        new WI.AuditTestCase("getParentNode", removeWhitespace(getParentNode), {description: WI.UIString("This test will pass with the parent node of the <body> element in the accessibility tree."), supports: 1}),
                        new WI.AuditTestCase("getSelectedChildNodes", removeWhitespace(getSelectedChildNodes), {description: WI.UIString("This test will pass with all child nodes that are selected (\u0022%s\u0022) of the <body> element in the accessibility tree.").format(WI.unlocalizedString("aria-selected")), supports: 1}),
                    ], {description: WI.UIString("These tests demonstrate how to use %s to get information about the accessibility tree.").format(WI.unlocalizedString("WebInspectorAudit.Accessibility")), supports: 1}),
                    new WI.AuditTestGroup(WI.UIString("DOM"), [
                        new WI.AuditTestCase("hasEventListeners", removeWhitespace(hasEventListeners), {description: WI.UIString("This test will pass with data indicating whether the <body> element has any event listeners."), supports: 3}),
                        new WI.AuditTestCase("hasEventListeners-click", removeWhitespace(hasEventListenersClick), {description: WI.UIString("This test will pass with data indicating whether the <body> element has any click event listeners."), supports: 3}),
                    ], {description: WI.UIString("These tests demonstrate how to use %s to get information about DOM nodes.").format(WI.unlocalizedString("WebInspectorAudit.DOM")), supports: 1}),
                    new WI.AuditTestGroup(WI.UIString("Resources"), [
                        new WI.AuditTestCase("getResources", removeWhitespace(getResources), {description: WI.UIString("This test will pass with some basic information about each resource."), supports: 3}),
                        new WI.AuditTestCase("getResourceContent", removeWhitespace(getResourceContent), {description: WI.UIString("This test will pass with the contents of the main resource."), supports: 3}),
                    ], {description: WI.UIString("These tests demonstrate how to use %s to get information about loaded resources.").format(WI.unlocalizedString("WebInspectorAudit.Resources")), supports: 2}),
                ], {description: WI.UIString("These tests demonstrate how to use %s to access information not normally available to JavaScript.").format(WI.unlocalizedString("WebInspectorAudit")), supports: 1}),
                new WI.AuditTestCase("unsupported", removeWhitespace(unsupported), {description: WI.UIString("This test should not run because it should be unsupported."), supports: Infinity}),
            ], {description: WI.UIString("These tests serve as a demonstration of the functionality and structure of audits.")}),
            new WI.AuditTestGroup(WI.UIString("Accessibility"), [
                new WI.AuditTestCase("testMenuRoleForRequiredChildren", removeWhitespace(testMenuRoleForRequiredChildren), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 and \u0022%s\u0022 has required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("menu"), WI.unlocalizedString("menubar")), supports: 1}),
                new WI.AuditTestCase("testGridRoleForRequiredChildren", removeWhitespace(testGridRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("grid")), supports: 1}),
                new WI.AuditTestCase("testForAriaLabelledBySpelling", removeWhitespace(testForAriaLabelledBySpelling), {description: WI.UIString("Ensure that \u0022%s\u0022 is spelled correctly.").format(WI.unlocalizedString("aria-labelledby")), supports: 1}),
                new WI.AuditTestCase("testForMultipleBanners", removeWhitespace(testForMultipleBanners), {description: WI.UIString("Ensure that only one banner is used on the page."), supports: 1}),
                new WI.AuditTestCase("testForLinkLabels", removeWhitespace(testForLinkLabels), {description: WI.UIString("Ensure that links have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase("testRowGroupRoleForRequiredChildren", removeWhitespace(testRowGroupRoleForRequiredChildren), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 has required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("rowgroup")), supports: 1}),
                new WI.AuditTestCase("testTableRoleForRequiredChildren", removeWhitespace(testTableRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("table")), supports: 1}),
                new WI.AuditTestCase("testForMultipleLiveRegions", removeWhitespace(testForMultipleLiveRegions), {description: WI.UIString("Ensure that only one live region is used on the page."), supports: 1}),
                new WI.AuditTestCase("testListBoxRoleForRequiredChildren", removeWhitespace(testListBoxRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("listbox")), supports: 1}),
                new WI.AuditTestCase("testImageLabels", removeWhitespace(testImageLabels), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have accessible labels for assistive technology.").format(WI.unlocalizedString("img")), supports: 1}),
                new WI.AuditTestCase("testForAriaHiddenFalse", removeWhitespace(testForAriaHiddenFalse), {description: WI.UIString("Ensure aria-hidden=\u0022%s\u0022 is not used.").format(WI.unlocalizedString("false")), supports: 1}),
                new WI.AuditTestCase("testTreeRoleForRequiredChildren", removeWhitespace(testTreeRoleForRequiredChildren), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 has required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("tree")), supports: 1}),
                new WI.AuditTestCase("testRadioGroupRoleForRequiredChildren", removeWhitespace(testRadioGroupRoleForRequiredChildren), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 has required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("radiogroup")), supports: 1}),
                new WI.AuditTestCase("testFeedRoleForRequiredChildren", removeWhitespace(testFeedRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("feed")), supports: 1}),
                new WI.AuditTestCase("testTabListRoleForRequiredChildren", removeWhitespace(testTabListRoleForRequiredChildren), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 has required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("tablist")), supports: 1}),
                new WI.AuditTestCase("testButtonLabels", removeWhitespace(testButtonLabels), {description: WI.UIString("Ensure that buttons have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase("testRowRoleForRequiredChildren", removeWhitespace(testRowRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("row")), supports: 1}),
                new WI.AuditTestCase("testListRoleForRequiredChildren", removeWhitespace(testListRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("list")), supports: 1}),
                new WI.AuditTestCase("testComboBoxRoleForRequiredChildren", removeWhitespace(testComboBoxRoleForRequiredChildren), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("combobox")), supports: 1}),
                new WI.AuditTestCase("testForMultipleMainContentSections", removeWhitespace(testForMultipleMainContentSections), {description: WI.UIString("Ensure that only one main content section is used on the page."), supports: 1}),
                new WI.AuditTestCase("testDialogsForLabels", removeWhitespace(testDialogsForLabels), {description: WI.UIString("Ensure that dialogs have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase("testForInvalidAriaHiddenValue", removeWhitespace(testForInvalidAriaHiddenValue), {description: WI.UIString("Ensure that values for \u0022%s\u0022 are valid.").format(WI.unlocalizedString("aria-hidden")), supports: 1})
            ], {description: WI.UIString("Diagnoses common accessibility problems affecting screen readers and other assistive technology.")}),
        ];

        let checkDisabledDefaultTest = (test) => {
            test.markAsDefault();

            if (this._disabledDefaultTestsSetting.value.includes(test.name))
                test.disabled = true;

            if (test instanceof WI.AuditTestGroup) {
                for (let child of test.tests)
                    checkDisabledDefaultTest(child);
            }
        };

        for (let test of defaultTests) {
            checkDisabledDefaultTest(test);

            this.addTest(test);
        }
    }
};

WI.AuditManager.RunningState = {
    Disabled: "disabled",
    Inactive: "inactive",
    Active: "active",
    Stopping: "stopping",
};

WI.AuditManager.Event = {
    EditingChanged: "audit-manager-editing-changed",
    RunningStateChanged: "audit-manager-running-state-changed",
    TestAdded: "audit-manager-test-added",
    TestCompleted: "audit-manager-test-completed",
    TestRemoved: "audit-manager-test-removed",
    TestScheduled: "audit-manager-test-scheduled",
};
