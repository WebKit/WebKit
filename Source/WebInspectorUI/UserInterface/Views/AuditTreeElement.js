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

WI.AuditTreeElement = class AuditTreeElement extends WI.GeneralTreeElement
{
    constructor(representedObject)
    {
        let isTestCase = representedObject instanceof WI.AuditTestCase;
        let isTestGroup = representedObject instanceof WI.AuditTestGroup;
        let isTestCaseResult = representedObject instanceof WI.AuditTestCaseResult;
        let isTestGroupResult = representedObject instanceof WI.AuditTestGroupResult;
        console.assert(isTestCase || isTestGroup || isTestCaseResult || isTestGroupResult);

        let classNames = ["audit"];
        if (isTestCase)
            classNames.push("test-case");
        else if (isTestGroup)
            classNames.push("test-group");
        else if (isTestCaseResult)
            classNames.push("test-case-result");
        else if (isTestGroupResult)
            classNames.push("test-group-result");

        let options = {
            hasChildren: isTestGroup || isTestGroupResult,
        };

        const subtitle = null;
        super(classNames, representedObject.name, subtitle, representedObject, options);

        if (isTestGroup)
            this._expandedSetting = new WI.Setting(`audit-tree-element-${this.representedObject.name}-expanded`, false);
    }

    // Protected

    onattach()
    {
        super.onattach();

        if (this.representedObject instanceof WI.AuditTestBase) {
            this.representedObject.addEventListener(WI.AuditTestBase.Event.ResultCleared, this._handleTestResultCleared, this);

            if (this.representedObject instanceof WI.AuditTestCase)
                this.representedObject.addEventListener(WI.AuditTestBase.Event.Scheduled, this._handleTestCaseScheduled, this);
            else if (this.representedObject instanceof WI.AuditTestGroup)
                this.representedObject.addEventListener(WI.AuditTestBase.Event.Scheduled, this._handleTestGroupScheduled, this);
        }

        if (this._expandedSetting && this._expandedSetting.value)
            this.expand();

        this._updateLevel();
    }

    ondetach()
    {
        this.representedObject.removeEventListener(null, null, this);

        super.ondetach();
    }

    onpopulate()
    {
        super.onpopulate();

        if (this.children.length && !this.shouldRefreshChildren)
            return;

        this.shouldRefreshChildren = false;

        this.removeChildren();

        if (this.representedObject instanceof WI.AuditTestGroup) {
            for (let test of this.representedObject.tests)
                this.appendChild(new WI.AuditTreeElement(test));
        } else if (this.representedObject instanceof WI.AuditTestGroupResult) {
            for (let result of this.representedObject.results)
                this.appendChild(new WI.AuditTreeElement(result));
        }
    }

    onexpand()
    {
        console.assert(this.expanded);

        if (this._expandedSetting)
            this._expandedSetting.value = this.expanded;
    }

    oncollapse()
    {
        console.assert(!this.expanded);

        if (this._expandedSetting)
            this._expandedSetting.value = this.expanded;
    }

    ondelete()
    {
        if (!(this.representedObject instanceof WI.AuditTestBase))
            return false;

        if (!(this.parent instanceof WI.TreeOutline))
            return false;

        WI.auditManager.removeTest(this.representedObject);

        return true;
    }

    populateContextMenu(contextMenu, event)
    {
        if (WI.auditManager.runningState === WI.AuditManager.RunningState.Inactive) {
            contextMenu.appendItem(WI.UIString("Start"), (event) => {
                this._start();
            });
        }

        contextMenu.appendSeparator();

        if (this.representedObject instanceof WI.AuditTestCase || this.representedObject instanceof WI.AuditTestGroup) {
            contextMenu.appendItem(WI.UIString("Export Test"), (event) => {
                WI.auditManager.export(this.representedObject);
            });
        }

        if (this.representedObject.result) {
            contextMenu.appendItem(WI.UIString("Export Result"), (event) => {
                WI.auditManager.export(this.representedObject.result);
            });
        }

        contextMenu.appendSeparator();

        super.populateContextMenu(contextMenu, event);
    }

    // Private

    _start()
    {
        if (WI.auditManager.runningState !== WI.AuditManager.RunningState.Inactive)
            return;

        WI.auditManager.start([this.representedObject]);
    }

    _updateLevel()
    {
        let className = "show-on-hover";

        let result = this.representedObject.result;
        if (result) {
            if (result.didError)
                className = WI.AuditTestCaseResult.Level.Error;
            else if (result.didFail)
                className = WI.AuditTestCaseResult.Level.Fail;
            else if (result.didWarn)
                className = WI.AuditTestCaseResult.Level.Warn;
            else if (result.didPass)
                className = WI.AuditTestCaseResult.Level.Pass;
            else if (result.unsupported)
                className = WI.AuditTestCaseResult.Level.Unsupported;
        }

        this.status = document.createElement("img");
        this.status.classList.add(className);

        if (this.representedObject instanceof WI.AuditTestCase || this.representedObject instanceof WI.AuditTestGroup) {
            this.status.title = WI.UIString("Start");
            this.status.addEventListener("click", this._handleStatusClick.bind(this));
        }
    }

    _showRunningSpinner()
    {
        if (this.representedObject.runningState === WI.AuditManager.RunningState.Inactive) {
            this._updateLevel();
            return;
        }

        if (!this.status || !this.status.__spinner) {
            let spinner = new WI.IndeterminateProgressSpinner;
            this.status = spinner.element;
            this.status.__spinner = true;
        }
    }

    _showRunningProgress(progress)
    {
        if (!this.representedObject.runningState === WI.AuditManager.RunningState.Inactive) {
            this._updateLevel();
            return;
        }

        if (!this.status || !this.status.__progress) {
            this.status = document.createElement("progress");
            this.status.__progress = true;
        }

        this.status.value = progress || 0;
    }

    _handleTestCaseCompleted(event)
    {
        this.representedObject.removeEventListener(WI.AuditTestBase.Event.Completed, this._handleTestCaseCompleted, this);

        this._updateLevel();
    }

    _handleTestResultCleared(event)
    {
        this._updateLevel();
    }

    _handleTestCaseScheduled(event)
    {
        this.representedObject.addEventListener(WI.AuditTestBase.Event.Completed, this._handleTestCaseCompleted, this);

        this._showRunningSpinner();
    }

    _handleTestGroupCompleted(event)
    {
        this.representedObject.removeEventListener(WI.AuditTestBase.Event.Completed, this._handleTestGroupCompleted, this);
        this.representedObject.removeEventListener(WI.AuditTestBase.Event.Progress, this._handleTestGroupProgress, this);

        this._updateLevel();
    }

    _handleTestGroupProgress(event)
    {
        let {index, count} = event.data;
        this._showRunningProgress((index + 1) / count);
    }

    _handleTestGroupScheduled(event)
    {
        this.representedObject.addEventListener(WI.AuditTestBase.Event.Completed, this._handleTestGroupCompleted, this);
        this.representedObject.addEventListener(WI.AuditTestBase.Event.Progress, this._handleTestGroupProgress, this);

        this._showRunningProgress();
    }

    _handleStatusClick(event)
    {
        this._start();
    }
};
