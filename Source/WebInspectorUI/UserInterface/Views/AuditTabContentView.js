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

WI.AuditTabContentView = class AuditTabContentView extends WI.ContentBrowserTabContentView
{
    constructor()
    {
        super("audit", ["audit"], WI.GeneralTabBarItem.fromTabInfo(WI.AuditTabContentView.tabInfo()), WI.AuditNavigationSidebarPanel);

        this._startStopShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Space, this._handleSpace.bind(this));
        this._startStopShortcut.implicitlyPreventsDefault = false;
        this._startStopShortcut.disabled = true;
    }

    // Static

    static tabInfo()
    {
        return {
            image: "Images/Audit.svg",
            title: WI.UIString("Audit"),
        };
    }

    static isTabAllowed()
    {
        return !!window.RuntimeAgent && WI.settings.experimentalEnableAuditTab.value;
    }

    // Public

    get type()
    {
        return WI.AuditTabContentView.Type;
    }

    get supportsSplitContentBrowser()
    {
        return true;
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WI.AuditTestCase
            || representedObject instanceof WI.AuditTestGroup
            || representedObject instanceof WI.AuditTestCaseResult
            || representedObject instanceof WI.AuditTestGroupResult;
    }

    shown()
    {
        super.shown();

        this._startStopShortcut.disabled = false;
    }

    hidden()
    {
        this._startStopShortcut.disabled = true;

        super.hidden();
    }

    // Private

    _handleSpace(event)
    {
        if (WI.isEventTargetAnEditableField(event))
            return;

        if (WI.auditManager.runningState === WI.AuditManager.RunningState.Inactive)
            WI.auditManager.start(this.contentBrowser.currentRepresentedObjects);
        else if (WI.auditManager.runningState === WI.AuditManager.RunningState.Active)
            WI.auditManager.stop();
        else
            return;

        event.preventDefault();
    }
};

WI.AuditTabContentView.Type = "audit";
