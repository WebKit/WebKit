/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.ScriptProfileTimelineView = class ScriptProfileTimelineView extends WebInspector.TimelineView
{
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.Script);

        this.element.classList.add("script");

        this._recording = extraArguments.recording;

        this._forceNextLayout = false;
        this._lastLayoutStartTime = undefined;
        this._lastLayoutEndTime = undefined;

        if (!WebInspector.ScriptProfileTimelineView.profileOrientationSetting)
            WebInspector.ScriptProfileTimelineView.profileOrientationSetting = new WebInspector.Setting("script-profile-timeline-view-profile-orientation-setting", WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown);

        let callingContextTree = this._callingContextTreeForOrientation(WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value);
        this._profileView = new WebInspector.ProfileView(callingContextTree);
        this._profileView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);
        this.addSubview(this._profileView);

        let scopeBarItems = [
            new WebInspector.ScopeBarItem(WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown, WebInspector.UIString("Top Down"), true),
            new WebInspector.ScopeBarItem(WebInspector.ScriptProfileTimelineView.ProfileOrientation.BottomUp, WebInspector.UIString("Bottom Up"), true),
        ];
        let defaultScopeBarItem = WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value === WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown ? scopeBarItems[0] : scopeBarItems[1];
        this._profileOrientationScopeBar = new WebInspector.ScopeBar("profile-orientation", scopeBarItems, defaultScopeBarItem);
        this._profileOrientationScopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        let clearTooltip = WebInspector.UIString("Clear focus");
        this._clearFocusNodesButtonItem = new WebInspector.ButtonNavigationItem("clear-profile-focus", clearTooltip, "Images/Close.svg", 16, 16);
        this._clearFocusNodesButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearFocusNodes, this);
        this._updateClearFocusNodesButtonItem();

        timeline.addEventListener(WebInspector.Timeline.Event.Refreshed, this._scriptTimelineRecordRefreshed, this);

        // FIXME: Support filtering the ProfileView.
    }

    // Protected

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);
    }

    get navigationItems()
    {
        return [this._clearFocusNodesButtonItem, this._profileOrientationScopeBar];
    }

    get selectionPathComponents()
    {
        return this._profileView.selectionPathComponents;
    }

    layout()
    {
        if (!this._forceNextLayout && (this._lastLayoutStartTime === this.startTime && this._lastLayoutEndTime === this.endTime))
            return;

        this._forceNextLayout = false;
        this._lastLayoutStartTime = this.startTime;
        this._lastLayoutEndTime = this.endTime;

        this._profileView.setStartAndEndTime(this.startTime, this.endTime);
    }

    // Private

    _callingContextTreeForOrientation(orientation)
    {
        switch (orientation) {
        case WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown:
            return this._recording.topDownCallingContextTree;
        case WebInspector.ScriptProfileTimelineView.ProfileOrientation.BottomUp:
            return this._recording.bottomUpCallingContextTree;
        }
    }

    _profileViewSelectionPathComponentsDidChange(event)
    {
        this._updateClearFocusNodesButtonItem();
        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _scriptTimelineRecordRefreshed(event)
    {
        this._forceNextLayout = true;
        this.needsLayout();
    }

    _scopeBarSelectionDidChange()
    {
        let currentOrientation = WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value;
        let newOrientation = this._profileOrientationScopeBar.selectedItems[0].id;
        let callingContextTree = this._callingContextTreeForOrientation(newOrientation);

        WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value = newOrientation;

        this.removeSubview(this._profileView);
        this._profileView.removeEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);

        this._profileView = new WebInspector.ProfileView(callingContextTree);

        this._profileView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);
        this.addSubview(this._profileView);

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        this._forceNextLayout = true;
        this.needsLayout();
    }

    _updateClearFocusNodesButtonItem()
    {
        this._clearFocusNodesButtonItem.enabled = this._profileView.hasFocusNodes();
    }

    _clearFocusNodes()
    {
        this._profileView.clearFocusNodes();
    }
};

WebInspector.ScriptProfileTimelineView.ProfileOrientation = {
    BottomUp: "bottom-up",
    TopDown: "top-down",
};
