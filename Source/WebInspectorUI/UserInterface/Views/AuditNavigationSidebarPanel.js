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

WI.AuditNavigationSidebarPanel = class AuditNavigationSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor()
    {
        super("audit", WI.UIString("Audits"));
    }

    // Public

    showDefaultContentView()
    {
        let contentView = new WI.ContentView;

        if (WI.auditManager.editing) {
            let contentPlaceholder = WI.createMessageTextView(WI.UIString("Editing audits"));
            contentPlaceholder.classList.add("finish-editing-audits-placeholder");
            contentView.element.appendChild(contentPlaceholder);

            let finishEditingNavigationItem = new WI.ButtonNavigationItem("finish-editing-audits", WI.UIString("Done"));
            finishEditingNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, (event) => {
                WI.auditManager.editing = false;
            });

            let importHelpElement = WI.createNavigationItemHelp(WI.UIString("Press %s to stop editing"), finishEditingNavigationItem);
            contentPlaceholder.appendChild(importHelpElement);
        } else {
            let contentPlaceholder = WI.createMessageTextView(WI.UIString("No audit selected"));
            contentView.element.appendChild(contentPlaceholder);

            let importNavigationItem = new WI.ButtonNavigationItem("import-audit", WI.UIString("Import"), "Images/Import.svg", 15, 15);
            importNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
            importNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleImportButtonNavigationItemClicked, this);

            let importHelpElement = WI.createNavigationItemHelp(WI.UIString("Press %s to import a test or result file"), importNavigationItem);
            contentPlaceholder.appendChild(importHelpElement);
        }

        let versionContainer = contentView.element.appendChild(document.createElement("div"));
        versionContainer.classList.add("audit-version");

        let version = WI.AuditTestBase.Version;
        if (InspectorBackend.domains.Audit)
            version = Math.min(version, InspectorBackend.domains.Audit.VERSION);
        versionContainer.textContent = WI.UIString("Audit version: %s").format(version);

        this.contentBrowser.showContentView(contentView);
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this.contentTreeOutline.allowsRepeatSelection = false;

        let controlsNavigationBar = new WI.NavigationBar;

        this._startStopButtonNavigationItem = new WI.ToggleButtonNavigationItem("audit-start-stop", WI.UIString("Start"), WI.UIString("Stop"), "Images/AuditStart.svg", "Images/AuditStop.svg", 13, 13);
        this._startStopButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        this._updateStartStopButtonNavigationItemState();
        this._startStopButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleStartStopButtonNavigationItemClicked, this);
        controlsNavigationBar.addNavigationItem(this._startStopButtonNavigationItem);

        controlsNavigationBar.addNavigationItem(new WI.DividerNavigationItem);

        let importButtonNavigationItem = new WI.ButtonNavigationItem("audit-import", WI.UIString("Import"), "Images/Import.svg", 15, 15);
        importButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        importButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        importButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleImportButtonNavigationItemClicked, this);
        controlsNavigationBar.addNavigationItem(importButtonNavigationItem);

        this.addSubview(controlsNavigationBar);

        let editNavigationbar = new WI.NavigationBar;

        this._editButtonNavigationItem = new WI.ActivateButtonNavigationItem("edit-audits", WI.UIString("Edit"), WI.UIString("Done"));
        this._editButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleEditButtonNavigationItemClicked, this);
        editNavigationbar.addNavigationItem(this._editButtonNavigationItem);

        this.contentView.addSubview(editNavigationbar);

        for (let test of WI.auditManager.tests)
            this._addTest(test);

        WI.auditManager.results.forEach((result, i) => {
            this._addResult(result, i);
        });

        WI.auditManager.addEventListener(WI.AuditManager.Event.EditingChanged, this._handleAuditManagerEditingChanged, this);
        WI.auditManager.addEventListener(WI.AuditManager.Event.TestAdded, this._handleAuditTestAdded, this);
        WI.auditManager.addEventListener(WI.AuditManager.Event.TestCompleted, this._handleAuditTestCompleted, this);
        WI.auditManager.addEventListener(WI.AuditManager.Event.TestRemoved, this._handleAuditTestRemoved, this);
        WI.auditManager.addEventListener(WI.AuditManager.Event.TestScheduled, this._handleAuditTestScheduled, this);

        this.contentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
    }

    closed()
    {
        super.closed();

        WI.auditManager.removeEventListener(null, null, this);
    }

    updateFilter()
    {
        super.updateFilter();

        if (!this.hasActiveFilters)
            this._updateNoAuditsPlaceholder();
    }

    hasCustomFilters()
    {
        return true;
    }

    matchTreeElementAgainstCustomFilters(treeElement, flags)
    {
        if (WI.auditManager.editing) {
            if (treeElement.representedObject instanceof WI.AuditTestResultBase || treeElement.hasAncestor(this._resultsFolderTreeElement) || treeElement === this._resultsFolderTreeElement)
                return false;
        } else {
            if (treeElement.representedObject instanceof WI.AuditTestBase && treeElement.representedObject.disabled)
                return false;
        }

        return super.matchTreeElementAgainstCustomFilters(treeElement, flags);
    }

    // Private

    _addTest(test)
    {
        let treeElement = new WI.AuditTreeElement(test);

        if (this._resultsFolderTreeElement) {
            this.contentTreeOutline.insertChild(treeElement, this.contentTreeOutline.children.indexOf(this._resultsFolderTreeElement));
            this._resultsFolderTreeElement.hidden = !this._resultsFolderTreeElement.children.length;
        } else
            this.contentTreeOutline.appendChild(treeElement);

        this._updateStartStopButtonNavigationItemState();
        this._updateEditButtonNavigationItemState();
        this._updateNoAuditsPlaceholder();
    }

    _addResult(result, index)
    {
        this.element.classList.add("has-results");

        if (!this._resultsFolderTreeElement) {
            this._resultsFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Results"));
            this.contentTreeOutline.appendChild(this._resultsFolderTreeElement);
        }

        this._resultsFolderTreeElement.expand();

        let resultFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Run %d").format(index + 1));
        if (result instanceof WI.AuditTestResultBase) {
            resultFolderTreeElement.subtitle = WI.UIString("Imported");
            result = [result];
        }
        this._resultsFolderTreeElement.appendChild(resultFolderTreeElement);

        console.assert(this._resultsFolderTreeElement.children.length === WI.auditManager.results.length);

        for (let resultItem of result)
            resultFolderTreeElement.appendChild(new WI.AuditTreeElement(resultItem));

        this._updateStartStopButtonNavigationItemState();
        this._updateEditButtonNavigationItemState();
    }

    _updateStartStopButtonNavigationItemState()
    {
        this._startStopButtonNavigationItem.toggled = WI.auditManager.runningState === WI.AuditManager.RunningState.Active || WI.auditManager.runningState === WI.AuditManager.RunningState.Stopping;
        this._startStopButtonNavigationItem.enabled = WI.auditManager.tests.some((test) => !test.disabled) && (WI.auditManager.runningState === WI.AuditManager.RunningState.Inactive || WI.auditManager.runningState === WI.AuditManager.RunningState.Active);
    }

     _updateEditButtonNavigationItemState()
    {
        this._editButtonNavigationItem.label = WI.auditManager.editing ? this._editButtonNavigationItem.activatedToolTip : this._editButtonNavigationItem.defaultToolTip;
        this._editButtonNavigationItem.activated = WI.auditManager.editing;
        this._editButtonNavigationItem.enabled = WI.auditManager.tests.length && (WI.auditManager.editing || WI.auditManager.runningState === WI.AuditManager.RunningState.Inactive);
    }

    _updateNoAuditsPlaceholder()
    {
        if (WI.auditManager.editing || WI.auditManager.tests.some((test) => !test.disabled)) {
            if (!this.hasActiveFilters)
                this.hideEmptyContentPlaceholder();
            return;
        }

        let contentPlaceholder = this.showEmptyContentPlaceholder(WI.UIString("No Enabled Audits"));
        contentPlaceholder.classList.add("no-enabled-audits");

        if (WI.auditManager.results.length) {
            // Move the placeholder to be the first element in the content area, where it will
            // be styled so that it doesn't obstruct the results elements.
            this.contentView.element.insertBefore(contentPlaceholder, this.contentView.element.firstChild);
        }
    }

    _handleAuditManagerEditingChanged(event)
    {
        if (WI.auditManager.editing) {
            console.assert(!this._selectedTreeElementBeforeEditing);
            this._selectedTreeElementBeforeEditing = this.contentTreeOutline.selectedTreeElement;
            if (this._selectedTreeElementBeforeEditing)
                this._selectedTreeElementBeforeEditing.deselect();
        } else if (this._selectedTreeElementBeforeEditing) {
            if (!(this._selectedTreeElementBeforeEditing.representedObject instanceof WI.AuditTestBase) || !this._selectedTreeElementBeforeEditing.representedObject.disabled)
                this._selectedTreeElementBeforeEditing.select();
            this._selectedTreeElementBeforeEditing = null;
        }

        if (!this.contentTreeOutline.selectedTreeElement)
            this.showDefaultContentView();

        this._updateStartStopButtonNavigationItemState();
        this._updateEditButtonNavigationItemState();

        this.updateFilter();
    }

    _handleAuditTestAdded(event)
    {
        this._addTest(event.data.test);
    }

    _handleAuditTestCompleted(event)
    {
        let {result, index} = event.data;
        this._addResult(result, index);
    }

    _handleAuditTestRemoved(event)
    {
        let {test} = event.data;
        let treeElement = this.treeElementForRepresentedObject(test);
        this.contentTreeOutline.removeChild(treeElement);

        this._updateStartStopButtonNavigationItemState();
        this._updateEditButtonNavigationItemState();
        this._updateNoAuditsPlaceholder();
    }

    _handleAuditTestScheduled(event)
    {
        this._updateStartStopButtonNavigationItemState();
        this._updateEditButtonNavigationItemState();
    }

    _treeSelectionDidChange(event)
    {
        if (!this.selected)
            return;

        let treeElement = this.contentTreeOutline.selectedTreeElement;
        if (!treeElement || treeElement instanceof WI.FolderTreeElement) {
            this.showDefaultContentView();
            return;
        }

        if (WI.auditManager.editing)
            return;

        let representedObject = treeElement.representedObject;
        if (representedObject instanceof WI.AuditTestCase || representedObject instanceof WI.AuditTestGroup
            || representedObject instanceof WI.AuditTestCaseResult || representedObject instanceof WI.AuditTestGroupResult) {
            WI.showRepresentedObject(representedObject);
            return;
        }

        console.error("Unknown tree element", treeElement);
    }

    _handleStartStopButtonNavigationItemClicked(event)
    {
        if (WI.auditManager.runningState === WI.AuditManager.RunningState.Inactive)
            WI.auditManager.start();
        else if (WI.auditManager.runningState === WI.AuditManager.RunningState.Active)
            WI.auditManager.stop();

        this._updateStartStopButtonNavigationItemState();
    }

    _handleImportButtonNavigationItemClicked(event)
    {
        WI.FileUtilities.importJSON((result) => WI.auditManager.processJSON(result));
    }

    _handleEditButtonNavigationItemClicked(event)
    {
        WI.auditManager.editing = !WI.auditManager.editing;
    }
};
