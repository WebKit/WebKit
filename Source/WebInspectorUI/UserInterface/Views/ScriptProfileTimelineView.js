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
    constructor(timeline, extraArguments)
    {
        super(timeline, extraArguments);

        console.assert(timeline.type === WI.TimelineRecord.Type.Script);

        this.element.classList.add("script");

        this._recording = extraArguments.recording;
        this._recording.addEventListener(WI.TimelineRecording.Event.TargetAdded, this._handleRecordingTargetAdded, this);

        this._selectedTarget = null;
        this._displayedTarget = null;

        this._forceNextLayout = false;
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

        this._targetNavigationItem = new WI.NavigationItem("script-profile-target");
        WI.addMouseDownContextMenuHandlers(this._targetNavigationItem.element, this._populateTargetNavigationItemContextMenu.bind(this));

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

        timeline.addEventListener(WI.Timeline.Event.Refreshed, this._scriptTimelineRecordRefreshed, this);
    }

    // Public

    get scrollableElements() { return this._profileView.scrollableElements; }

    get showsLiveRecordingData() { return false; }

    closed()
    {
        this.representedObject.removeEventListener(WI.Timeline.Event.Refreshed, this._scriptTimelineRecordRefreshed, this);

        this._recording.removeEventListener(WI.TimelineRecording.Event.TargetAdded, this._handleRecordingTargetAdded, this);

        super.closed();
    }

    get navigationItems()
    {
        let navigationItems = [];
        navigationItems.push(this._clearFocusNodesButtonItem);
        if (this._recording.targets.length > 1)
            navigationItems.push(this._targetNavigationItem);
        navigationItems.push(this._profileOrientationButton);
        navigationItems.push(this._topFunctionsButton);
        return navigationItems;
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

        if (!this._displayedTarget)
            return new WI.CallingContextTree(WI.mainTarget, type);
        return this._recording.callingContextTree(this._displayedTarget, type);
    }

    _updateTargetNavigationItemDisplay()
    {
        this._targetNavigationItem.element.textContent = this._displayNameForTarget(this._displayedTarget);

        this._targetNavigationItem.element.appendChild(WI.ImageUtilities.useSVGSymbol("Images/UpDownArrows.svg", "selector-arrows"));

        this.dispatchEventToListeners(WI.ContentView.Event.NavigationItemsDidChange);
    }

    _displayNameForTarget(target)
    {
        if (target.type === WI.TargetType.Worker)
            return WI.UIString("Worker \u201C%s\u201D").format(target.displayName);
        return WI.UIString("Page");
    }

    _profileViewSelectionPathComponentsDidChange(event)
    {
        this._updateClearFocusNodesButtonItem();
        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
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

    _handleRecordingTargetAdded(event)
    {
        if (this._selectedTarget)
            return;

        let targets = this._recording.targets;
        if (!targets.length)
            return;

        let displayedTarget = targets.includes(WI.mainTarget) ? WI.mainTarget : targets[0];
        if (displayedTarget !== this._displayedTarget) {
            this._displayedTarget = displayedTarget;
            this._showProfileView();
        }

        if (targets.length > 1)
            this._updateTargetNavigationItemDisplay();
    }

    _populateTargetNavigationItemContextMenu(contextMenu)
    {
        const rankFunctions = [
            (target) => target === WI.mainTarget,
            (target) => target.type === WI.TargetType.Page,
            (target) => target.type === WI.TargetType.Worker,
        ];
        let sortedTargets = this._recording.targets.sort((a, b) => {
            let aRank = rankFunctions.findIndex((rankFunction) => rankFunction(a));
            let bRank = rankFunctions.findIndex((rankFunction) => rankFunction(b));
            if ((aRank >= 0 && bRank < 0) || aRank < bRank)
                return -1;
            if ((bRank >= 0 && aRank < 0) || bRank < aRank)
                return 1;

            return this._displayNameForTarget(a).extendedLocaleCompare(this._displayNameForTarget(b));
        });
        for (let target of sortedTargets) {
            contextMenu.appendCheckboxItem(this._displayNameForTarget(target), () => {
                this._selectedTarget = target;
                this._displayedTarget = target;
                this._updateTargetNavigationItemDisplay();

                this._showProfileView();
            }, target === this._displayedTarget);
        }
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
