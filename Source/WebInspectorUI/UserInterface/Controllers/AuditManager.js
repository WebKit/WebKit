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
                if (test.disabled)
                    disabledDefaultTests.push(test.name);

                if (test instanceof WI.AuditTestGroup) {
                    for (let child of test.tests)
                        saveDisabledDefaultTest(child);
                }
            };

            for (let test of this._tests) {
                if (test.__default)
                    saveDisabledDefaultTest(test);
                else
                    WI.objectStores.audits.addObject(test);
            }

            this._disabledDefaultTestsSetting.value = disabledDefaultTests;
        }
    }

    async start(tests)
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Inactive);
        if (this._runningState !== WI.AuditManager.RunningState.Inactive)
            return;

        if (tests && tests.length)
            tests = tests.filter((test) => typeof test === "object" && test instanceof WI.AuditTestBase);
        else
            tests = this._tests;

        console.assert(tests.length);
        if (!tests.length)
            return;

        let mainResource = WI.networkManager.mainFrame.mainResource;

        this._runningState = WI.AuditManager.RunningState.Active;
        this._runningTests = tests;
        for (let test of this._runningTests)
            test.clearResult();

        this.dispatchEventToListeners(WI.AuditManager.Event.TestScheduled);

        await Promise.chain(this._runningTests.map((test) => async () => {
            if (this._runningState !== WI.AuditManager.RunningState.Active)
                return;

            if (InspectorBackend.domains.Audit)
                await AuditAgent.setup();

            await test.start();

            if (InspectorBackend.domains.Audit)
                await AuditAgent.teardown();
        }));

        let result = this._runningTests.map((test) => test.result).filter((result) => !!result);

        this._runningState = WI.AuditManager.RunningState.Inactive;
        this._runningTests = [];

        this._addResult(result);

        if (mainResource !== WI.networkManager.mainFrame.mainResource) {
            // Navigated while tests were running.
            for (let test of this._tests)
                test.clearResult();
        }
    }

    stop()
    {
        console.assert(this._runningState === WI.AuditManager.RunningState.Active);
        if (this._runningState !== WI.AuditManager.RunningState.Active)
            return;

        this._runningState = WI.AuditManager.RunningState.Stopping;

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

        if (!test.__default)
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

        if (this._runningState === WI.AuditManager.RunningState.Active)
            this.stop();
        else {
            for (let test of this._tests)
                test.clearResult();
        }
    }

    addDefaultTestsIfNeeded()
    {
        if (this._tests.length)
            return;

        const testMenuRoleForRequiredChidren = function() {
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

        const testGridRoleForRequiredChidren = function() {
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

        const testRowGroupRoleForRequiredChidren = function() {
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

        const testTableRoleForRequiredChidren = function() {
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

        const testListBoxRoleForRequiredChidren = function() {
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

        const testTreeRoleForRequiredChidren = function() {
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

        const testRadioGroupRoleForRequiredChidren = function() {
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

        const testFeedRoleForRequiredChidren = function() {
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

        const testTabListRoleForRequiredChidren = function() {
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

        const testCellRoleForRequiredChidren = function() {
            const relationships = {
                cell: ["row"],
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

        const testListRoleForRequiredChidren = function() {
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

        const testComboBoxRoleForRequiredChidren = function() {
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

        const defaultTests = [
            new WI.AuditTestGroup(WI.UIString("Demo Audit"), [
                new WI.AuditTestGroup(WI.UIString("Result Levels"), [
                    new WI.AuditTestCase(`level-pass`, `function() { return {level: "pass"}; }`, {description: WI.UIString("This is what the result of a passing test with no data looks like.")}),
                    new WI.AuditTestCase(`level-warn`, `function() { return {level: "warn"}; }`, {description: WI.UIString("This is what the result of a warning test with no data looks like.")}),
                    new WI.AuditTestCase(`level-fail`, `function() { return {level: "fail"}; }`, {description: WI.UIString("This is what the result of a failing test with no data looks like.")}),
                    new WI.AuditTestCase(`level-error`, `function() { return {level: "error"}; }`, {description: WI.UIString("This is what the result of a test that threw an error with no data looks like.")}),
                    new WI.AuditTestCase(`level-unsupported`, `function() { return {level: "unsupported"}; }`, {description: WI.UIString("This is what the result of an unsupported test with no data looks like.")}),
                ], {description: WI.UIString("These are all of the different test result levels.")}),
                new WI.AuditTestGroup(WI.UIString("Result Data"), [
                    new WI.AuditTestCase(`data-domNodes`, `function() { return {domNodes: [document.body], level: "pass"}; }`, {description: WI.UIString("This is an example of how result DOM nodes are shown. It will pass with the <body> element.")}),
                    new WI.AuditTestCase(`data-domAttributes`, `function() { return {domNodes: Array.from(document.querySelectorAll("[id]")), domAttributes: ["id"], level: "pass"}; }`, {description: WI.UIString("This is an example of how result DOM nodes are shown. It will pass with all elements with an id attribute.")}),
                    new WI.AuditTestCase(`data-errors`, `function() { throw Error("this error was thrown from inside the audit test code."); }`, {description: WI.UIString("This is an example of how errors are shown. The error was thrown manually, but execution errors will appear in the same way.")}),
                ], {description: WI.UIString("These are all of the different types of data that can be returned with the test result.")}),
            ], {description: WI.UIString("These tests serve as a demonstration of the functionality and structure of audits.")}),
            new WI.AuditTestGroup(WI.UIString("Accessibility"), [
                new WI.AuditTestCase(`testMenuRoleForRequiredChidren`, testMenuRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 and \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("menu"), WI.unlocalizedString("menubar")), supports: 1}),
                new WI.AuditTestCase(`testGridRoleForRequiredChidren`, testGridRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("grid")), supports: 1}),
                new WI.AuditTestCase(`testForAriaLabelledBySpelling`, testForAriaLabelledBySpelling.toString(), {description: WI.UIString("Ensure that \u0022%s\u0022 is spelled correctly.").format(WI.unlocalizedString("aria-labelledby")), supports: 1}),
                new WI.AuditTestCase(`testForMultipleBanners`, testForMultipleBanners.toString(), {description: WI.UIString("Ensure that only one banner is used on the page."), supports: 1}),
                new WI.AuditTestCase(`testForLinkLabels`, testForLinkLabels.toString(), {description: WI.UIString("Ensure that links have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase(`testRowGroupRoleForRequiredChidren`, testRowGroupRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("rowgroup")), supports: 1}),
                new WI.AuditTestCase(`testTableRoleForRequiredChidren`, testTableRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("table")), supports: 1}),
                new WI.AuditTestCase(`testForMultipleLiveRegions`, testForMultipleLiveRegions.toString(), {description: WI.UIString("Ensure that only one live region is used on the page."), supports: 1}),
                new WI.AuditTestCase(`testListBoxRoleForRequiredChidren`, testListBoxRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("listbox")), supports: 1}),
                new WI.AuditTestCase(`testImageLabels`, testImageLabels.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have accessible labels for assistive technology.").format(WI.unlocalizedString("img")), supports: 1}),
                new WI.AuditTestCase(`testForAriaHiddenFalse`, testForAriaHiddenFalse.toString(), {description: WI.UIString("Ensure aria-hidden=\u0022%s\u0022 is not used.").format(WI.unlocalizedString("false")), supports: 1}),
                new WI.AuditTestCase(`testTreeRoleForRequiredChidren`, testTreeRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("tree")), supports: 1}),
                new WI.AuditTestCase(`testRadioGroupRoleForRequiredChidren`, testRadioGroupRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("radiogroup")), supports: 1}),
                new WI.AuditTestCase(`testFeedRoleForRequiredChidren`, testFeedRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("feed")), supports: 1}),
                new WI.AuditTestCase(`testTabListRoleForRequiredChidren`, testTabListRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that element of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("tablist")), supports: 1}),
                new WI.AuditTestCase(`testButtonLabels`, testButtonLabels.toString(), {description: WI.UIString("Ensure that buttons have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase(`testCellRoleForRequiredChidren`, testCellRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("cell")), supports: 1}),
                new WI.AuditTestCase(`testListRoleForRequiredChidren`, testListRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("list")), supports: 1}),
                new WI.AuditTestCase(`testComboBoxRoleForRequiredChidren`, testComboBoxRoleForRequiredChidren.toString(), {description: WI.UIString("Ensure that elements of role \u0022%s\u0022 have required owned elements in accordance with WAI-ARIA.").format(WI.unlocalizedString("combobox")), supports: 1}),
                new WI.AuditTestCase(`testForMultipleMainContentSections`, testForMultipleMainContentSections.toString(), {description: WI.UIString("Ensure that only one main content section is used on the page."), supports: 1}),
                new WI.AuditTestCase(`testDialogsForLabels`, testDialogsForLabels.toString(), {description: WI.UIString("Ensure that dialogs have accessible labels for assistive technology."), supports: 1}),
                new WI.AuditTestCase(`testForInvalidAriaHiddenValue`, testForInvalidAriaHiddenValue.toString(), {description: WI.UIString("Ensure that values for \u0022%s\u0022 are valid.").format(WI.unlocalizedString("aria-hidden")), supports: 1})
            ], {description: WI.UIString("Diagnoses common accessibility problems affecting screen readers and other assistive technology.")}),
        ];

        let checkDisabledDefaultTest = (test) => {
            if (this._disabledDefaultTestsSetting.value.includes(test.name))
                test.disabled = true;

            if (test instanceof WI.AuditTestGroup) {
                for (let child of test.tests)
                    checkDisabledDefaultTest(child);
            }
        };

        for (let test of defaultTests) {
            checkDisabledDefaultTest(test);

            test.__default = true;
            this._addTest(test);
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
    TestAdded: "audit-manager-test-added",
    TestCompleted: "audit-manager-test-completed",
    TestRemoved: "audit-manager-test-removed",
    TestScheduled: "audit-manager-test-scheduled",
};
