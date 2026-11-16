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

WI.ScriptProfileTimelineView = class ScriptProfileTimelineView extends WI.TimelineView
{
    constructor(target, timeline)
    {
        console.assert(timeline.type === WI.TimelineRecord.Type.Script, timeline);

        super(timeline);

        this._target = target;

        this.element.classList.add("script");

        this._forceNextLayout = false;
        this._missingCallingContextTree = false;
        this._lastLayoutStartTime = undefined;
        this._lastLayoutEndTime = undefined;

        this._sharedProfileViewData = {
            selectedNodeHash: null,
        };

        if (!WI.ScriptProfileTimelineView.profileOrientationSetting)
            WI.ScriptProfileTimelineView.profileOrientationSetting = new WI.Setting("script-profile-timeline-view-profile-orientation-setting", WI.ScriptProfileTimelineView.ProfileOrientation.TopDown);
        if (!WI.ScriptProfileTimelineView.profileTypeSetting)
            WI.ScriptProfileTimelineView.profileTypeSetting = new WI.Setting("script-profile-timeline-view-profile-type-setting", WI.ScriptProfileTimelineView.ProfileViewType.Hierarchy);

        this._createProfileView();

        let clearTooltip = WI.UIString("Clear focus");
        this._clearFocusNodesButtonItem = new WI.ButtonNavigationItem("clear-profile-focus", clearTooltip, "Images/Close.svg", 16, 16);
        this._clearFocusNodesButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._clearFocusNodes, this);
        this._updateClearFocusNodesButtonItem();

        this._profileOrientationButton = new WI.TextToggleButtonNavigationItem("profile-orientation", WI.UIString("Inverted"));
        this._profileOrientationButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._profileOrientationButtonClicked, this);
        if (WI.ScriptProfileTimelineView.profileOrientationSetting.value === WI.ScriptProfileTimelineView.ProfileOrientation.TopDown)
            this._profileOrientationButton.activated = false;
        else
            this._profileOrientationButton.activated = true;

        this._topFunctionsButton = new WI.TextToggleButtonNavigationItem("top-functions", WI.UIString("Top Functions"));
        this._topFunctionsButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._topFunctionsButtonClicked, this);
        if (WI.ScriptProfileTimelineView.profileTypeSetting.value === WI.ScriptProfileTimelineView.ProfileViewType.Hierarchy)
            this._topFunctionsButton.activated = false;
        else
            this._topFunctionsButton.activated = true;

        this.representedObject.addEventListener(WI.ScriptTimeline.Event.Refreshed, this._handleScriptTimelineRefreshed, this);
    }

    // Public

    get scrollableElements() { return this._profileView.scrollableElements; }
    get showsLiveRecordingData() { return false; }

    closed()
    {
        this.representedObject.removeEventListener(WI.ScriptTimeline.Event.Refreshed, this._handleScriptTimelineRefreshed, this);

        this._profileView.removeEventListener(WI.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);

        super.closed();
    }

    get navigationItems()
    {
        return [this._clearFocusNodesButtonItem, this._profileOrientationButton, this._topFunctionsButton];
    }

    get selectionPathComponents()
    {
        return this._profileView.selectionPathComponents;
    }

    reset()
    {
        super.reset();

        this._createProfileView();

        this._updateClearFocusNodesButtonItem();
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
        let type = null;
        switch (profileOrientation) {
        case WI.ScriptProfileTimelineView.ProfileOrientation.TopDown:
            switch (profileViewType) {
            case WI.ScriptProfileTimelineView.ProfileViewType.Hierarchy:
                type = WI.CallingContextTree.Type.TopDown;
                break;
            case WI.ScriptProfileTimelineView.ProfileViewType.TopFunctions:
                type = WI.CallingContextTree.Type.TopFunctionsTopDown;
                break;
            }
            break;
        case WI.ScriptProfileTimelineView.ProfileOrientation.BottomUp:
            switch (profileViewType) {
            case WI.ScriptProfileTimelineView.ProfileViewType.Hierarchy:
                type = WI.CallingContextTree.Type.BottomUp;
                break;
            case WI.ScriptProfileTimelineView.ProfileViewType.TopFunctions:
                type = WI.CallingContextTree.Type.TopFunctionsBottomUp;
                break;
            }
            break;
        }
        console.assert(type);
        type ??= WI.CallingContextTree.Type.TopDown;
        let callingContextTree = this.representedObject.callingContextTree(this._target, type);

        // This will happen when the timeline is reset/cleared, so listen for when the given target
        // is finally added to the timeline (i.e. a script event is captured for that target) and
        // then regenerate the data structures used to represent the profiled activity data.
        if (!callingContextTree) {
            callingContextTree = new WI.CallingContextTree(this._target, type);
            this.representedObject.addEventListener(WI.ScriptTimeline.Event.TargetAdded, this._handleScriptTimelineTargetAdded, this);
        }

        return callingContextTree;
    }

    _profileViewSelectionPathComponentsDidChange(event)
    {
        this._updateClearFocusNodesButtonItem();
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _handleScriptTimelineRefreshed(event)
    {
        this._forceNextLayout = true;
        this.needsLayout();
    }

    _handleScriptTimelineTargetAdded(event)
    {
        let {target} = event.data;

        if (target !== this._target)
            return;

        this._showProfileView();

        this.representedObject.removeEventListener(WI.ScriptTimeline.Event.TargetAdded, this._handleScriptTimelineTargetAdded, this);
    }

    _profileOrientationButtonClicked()
    {
        this._profileOrientationButton.activated = !this._profileOrientationButton.activated;
        let isInverted = this._profileOrientationButton.activated;
        let newOrientation;
        if (isInverted)
            newOrientation = WI.ScriptProfileTimelineView.ProfileOrientation.BottomUp;
        else
            newOrientation = WI.ScriptProfileTimelineView.ProfileOrientation.TopDown;

        WI.ScriptProfileTimelineView.profileOrientationSetting.value = newOrientation;

        this._showProfileView();
    }

    _topFunctionsButtonClicked()
    {
        this._topFunctionsButton.activated = !this._topFunctionsButton.activated;
        let isTopFunctionsEnabled = this._topFunctionsButton.activated;
        let newOrientation;
        if (isTopFunctionsEnabled)
            newOrientation = WI.ScriptProfileTimelineView.ProfileViewType.TopFunctions;
        else
            newOrientation = WI.ScriptProfileTimelineView.ProfileViewType.Hierarchy;

        WI.ScriptProfileTimelineView.profileTypeSetting.value = newOrientation;

        this._showProfileView();
    }

    _createProfileView()
    {
        let filterText;
        if (this._profileView) {
            this._profileView.removeEventListener(WI.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);
            this.removeSubview(this._profileView);
            filterText = this._profileView.dataGrid.filterText;
        }

        let callingContextTree = this._callingContextTreeForOrientation(WI.ScriptProfileTimelineView.profileOrientationSetting.value, WI.ScriptProfileTimelineView.profileTypeSetting.value);
        this._profileView = new WI.ProfileView(callingContextTree, this._sharedProfileViewData);
        this._profileView.addEventListener(WI.ContentView.Event.SelectionPathComponentsDidChange, this._profileViewSelectionPathComponentsDidChange, this);

        this.addSubview(this._profileView);
        this.setupDataGrid(this._profileView.dataGrid);

        if (filterText)
            this._profileView.dataGrid.filterText = filterText;
    }

    _showProfileView()
    {
        this._createProfileView();

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);

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

WI.ScriptProfileTimelineView.ProfileOrientation = {
    BottomUp: "bottom-up",
    TopDown: "top-down",
};

WI.ScriptProfileTimelineView.ProfileViewType = {
    Hierarchy: "hierarchy",
    TopFunctions: "top-functions",
};

WI.ScriptProfileTimelineView.ReferencePage = WI.ReferencePage.TimelinesTab.JavaScriptAndEventsTimeline;
