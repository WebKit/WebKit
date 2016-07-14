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

        this._sharedProfileViewData = {
            selectedNodeHash: null,
        };

        if (!WebInspector.ScriptProfileTimelineView.profileOrientationSetting)
            WebInspector.ScriptProfileTimelineView.profileOrientationSetting = new WebInspector.Setting("script-profile-timeline-view-profile-orientation-setting", WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown);
        if (!WebInspector.ScriptProfileTimelineView.profileTypeSetting)
            WebInspector.ScriptProfileTimelineView.profileTypeSetting = new WebInspector.Setting("script-profile-timeline-view-profile-type-setting", WebInspector.ScriptProfileTimelineView.ProfileViewType.Hierarchy);

        this._showProfileViewForOrientation(WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value, WebInspector.ScriptProfileTimelineView.profileTypeSetting.value);

        let clearTooltip = WebInspector.UIString("Clear focus");
        this._clearFocusNodesButtonItem = new WebInspector.ButtonNavigationItem("clear-profile-focus", clearTooltip, "Images/Close.svg", 16, 16);
        this._clearFocusNodesButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearFocusNodes, this);
        this._updateClearFocusNodesButtonItem();

        this._profileOrientationButton = new WebInspector.TextToggleButtonNavigationItem("profile-orientation", WebInspector.UIString("Inverted"));
        this._profileOrientationButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._profileOrientationButtonClicked, this);
        if (WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value == WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown)
            this._profileOrientationButton.activated = false;
        else
            this._profileOrientationButton.activated = true;

        this._topFunctionsButton = new WebInspector.TextToggleButtonNavigationItem("top-functions", WebInspector.UIString("Top Functions"));
        this._topFunctionsButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._topFunctionsButtonClicked, this);
        if (WebInspector.ScriptProfileTimelineView.profileTypeSetting.value == WebInspector.ScriptProfileTimelineView.ProfileViewType.Hierarchy)
            this._topFunctionsButton.activated = false;
        else
            this._topFunctionsButton.activated = true;

        timeline.addEventListener(WebInspector.Timeline.Event.Refreshed, this._scriptTimelineRecordRefreshed, this);
    }

    // Public

    get scrollableElements() { return this._profileView.scrollableElements; }

    get showsLiveRecordingData() { return false; }

    // Protected

    closed()
    {
        console.assert(this.representedObject instanceof WebInspector.Timeline);
        this.representedObject.removeEventListener(null, null, this);
    }

    get navigationItems()
    {
        return [this._clearFocusNodesButtonItem, this._profileOrientationButton, this._topFunctionsButton];
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

    _callingContextTreeForOrientation(profileOrientation, profileViewType)
    {
        switch (profileOrientation) {
        case WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown:
            return profileViewType === WebInspector.ScriptProfileTimelineView.ProfileViewType.Hierarchy ? this._recording.topDownCallingContextTree : this._recording.topFunctionsTopDownCallingContextTree;
        case WebInspector.ScriptProfileTimelineView.ProfileOrientation.BottomUp:
            return profileViewType === WebInspector.ScriptProfileTimelineView.ProfileViewType.Hierarchy ? this._recording.bottomUpCallingContextTree : this._recording.topFunctionsBottomUpCallingContextTree;
        }

        console.assert(false, "Should not be reached.");
        return this._recording.topDownCallingContextTree;
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

    _profileOrientationButtonClicked()
    {
        this._profileOrientationButton.activated = !this._profileOrientationButton.activated;
        let isInverted = this._profileOrientationButton.activated;
        let newOrientation;
        if (isInverted)
            newOrientation = WebInspector.ScriptProfileTimelineView.ProfileOrientation.BottomUp;
        else
            newOrientation = WebInspector.ScriptProfileTimelineView.ProfileOrientation.TopDown;

        WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value = newOrientation;
        this._showProfileViewForOrientation(newOrientation, WebInspector.ScriptProfileTimelineView.profileTypeSetting.value);

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        this._forceNextLayout = true;
        this.needsLayout();
    }

    _topFunctionsButtonClicked()
    {
        this._topFunctionsButton.activated = !this._topFunctionsButton.activated;
        let isTopFunctionsEnabled = this._topFunctionsButton.activated;
        let newOrientation;
        if (isTopFunctionsEnabled)
            newOrientation = WebInspector.ScriptProfileTimelineView.ProfileViewType.TopFunctions;
        else
            newOrientation = WebInspector.ScriptProfileTimelineView.ProfileViewType.Hierarchy;

        WebInspector.ScriptProfileTimelineView.profileTypeSetting.value = newOrientation;
        this._showProfileViewForOrientation(WebInspector.ScriptProfileTimelineView.profileOrientationSetting.value, newOrientation);

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);

        this._forceNextLayout = true;
        this.needsLayout();
    }

    _showProfileViewForOrientation(profileOrientation, profileViewType)
    {
        let filterText;
        if (this._profileView) {
            this._profileView.removeEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);
            this.removeSubview(this._profileView);
            filterText = this._profileView.dataGrid.filterText;
        }

        let callingContextTree = this._callingContextTreeForOrientation(profileOrientation, profileViewType);
        this._profileView = new WebInspector.ProfileView(callingContextTree, this._sharedProfileViewData);
        this._profileView.addEventListener(WebInspector.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);

        this.addSubview(this._profileView);
        this.setupDataGrid(this._profileView.dataGrid);

        if (filterText)
            this._profileView.dataGrid.filterText = filterText;
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

WebInspector.ScriptProfileTimelineView.ProfileViewType = {
    Hierarchy: "hierarchy",
    TopFunctions: "top-functions",
};
