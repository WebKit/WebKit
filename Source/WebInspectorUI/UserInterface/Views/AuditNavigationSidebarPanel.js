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

        let contentPlaceholder = WI.createMessageTextView(WI.UIString("No audit selected"));
        contentView.element.appendChild(contentPlaceholder);

        let importNavigationItem = new WI.ButtonNavigationItem("import-audit", WI.UIString("Import"), "Images/Import.svg", 15, 15);
        importNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        importNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleImportButtonNavigationItemClicked, this);

        let importHelpElement = WI.createNavigationItemHelp(WI.UIString("Press %s to import a test or result file"), importNavigationItem);
        contentPlaceholder.appendChild(importHelpElement);

        this.contentBrowser.showContentView(contentView);
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this.contentTreeOutline.allowsRepeatSelection = false;

        this._resultsFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Results"));
        this.contentTreeOutline.appendChild(this._resultsFolderTreeElement);
        this._resultsFolderTreeElement.hidden = true;
        this._resultsFolderTreeElement.expand();

        let navigationBar = new WI.NavigationBar;

        this._startStopButtonNavigationItem = new WI.ToggleButtonNavigationItem("audit-start-stop", WI.UIString("Start"), WI.UIString("Stop"), "Images/AuditStart.svg", "Images/AuditStop.svg", 13, 13);
        this._startStopButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        this._updateStartStopButtonNavigationItemState();
        this._startStopButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleStartStopButtonNavigationItemClicked, this);
        navigationBar.addNavigationItem(this._startStopButtonNavigationItem);

        navigationBar.addNavigationItem(new WI.DividerNavigationItem);

        let importButtonNavigationItem = new WI.ButtonNavigationItem("audit-import", WI.UIString("Import"), "Images/Import.svg", 15, 15);
        importButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
        importButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        importButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleImportButtonNavigationItemClicked, this);
        navigationBar.addNavigationItem(importButtonNavigationItem);

        this.addSubview(navigationBar);

        for (let test of WI.auditManager.tests)
            this._addTest(test);

        for (let result of WI.auditManager.results)
            this._addResult(result);

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

    // Private

    _addTest(test)
    {
        this._updateStartStopButtonNavigationItemState();

        this.contentTreeOutline.insertChild(new WI.AuditTreeElement(test), this.contentTreeOutline.children.indexOf(this._resultsFolderTreeElement));

        this._resultsFolderTreeElement.hidden = !this._resultsFolderTreeElement.children.length;
    }

    _addResult(result, index)
    {
        this._updateStartStopButtonNavigationItemState();

        this._resultsFolderTreeElement.hidden = false;

        let resultFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Run %d").format(index + 1));
        if (result instanceof WI.AuditTestResultBase) {
            resultFolderTreeElement.subtitle = WI.UIString("Imported");
            result = [result];
        }
        this._resultsFolderTreeElement.appendChild(resultFolderTreeElement);

        for (let resultItem of result)
            resultFolderTreeElement.appendChild(new WI.AuditTreeElement(resultItem));
    }

    _updateStartStopButtonNavigationItemState()
    {
        this._startStopButtonNavigationItem.toggled = WI.auditManager.runningState !== WI.AuditManager.RunningState.Inactive;
        this._startStopButtonNavigationItem.enabled = WI.auditManager.tests.length && WI.auditManager.runningState !== WI.AuditManager.RunningState.Stopping;
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
    }

    _handleAuditTestScheduled(event)
    {
        this._updateStartStopButtonNavigationItemState();
    }

    _treeSelectionDidChange(event)
    {
        if (!this.selected)
            return;

        let treeElement = event.data.selectedElement;
        if (!treeElement || treeElement instanceof WI.FolderTreeElement) {
            this.showDefaultContentView();
            return;
        }

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
};
