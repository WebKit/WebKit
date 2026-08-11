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

WI.ScriptClusterTimelineView = class ScriptClusterTimelineView extends WI.ClusterContentView
{
    constructor(timeline)
    {
        console.assert(timeline.type === WI.TimelineRecord.Type.Script, timeline);

        super(timeline);

        this._currentContentViewSetting = new WI.Setting("script-cluster-timeline-view-current-view", WI.ScriptClusterTimelineView.EventsIdentifier);

        function createPathComponent(displayName, className, identifier)
        {
            const showSelectorArrows = true;
            let pathComponent = new WI.HierarchicalPathComponent(displayName, className, identifier, false, showSelectorArrows);
            pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._handleViewPathComponentSelected, this);
            pathComponent.comparisonData = this.representedObject;
            return pathComponent;
        }

        this._eventsPathComponent = createPathComponent.call(this, WI.UIString("Events"), "events-icon", WI.ScriptClusterTimelineView.EventsIdentifier);
        this._profilePathComponent = createPathComponent.call(this, WI.UIString("Call Trees"), "call-trees-icon", WI.ScriptClusterTimelineView.ProfileIdentifier);

        this._eventsPathComponent.nextSibling = this._profilePathComponent;
        this._profilePathComponent.previousSibling = this._eventsPathComponent;

        let targets = this.representedObject.targets;

        this._selectedTarget = null;
        this._displayedTarget = (!this.representedObject.imported && targets.includes(WI.mainTarget)) ? WI.mainTarget : (targets.firstValue || WI.assumingMainTarget());

        this._pathComponentForTarget = new Map;
        for (let target of targets)
            this._addPathComponentForTarget(target);
        this._sortTargetPathComponents();

        this._contentViewsForTarget = new Map;

        this._previousContentView = null;
        this._updateCurrentContentView();

        this.representedObject.addEventListener(WI.ScriptTimeline.Event.TargetAdded, this._handleTargetAdded, this);

        this.contentViewContainer.addEventListener(WI.ContentViewContainer.Event.CurrentContentViewDidChange, this._scriptClusterViewCurrentContentViewDidChange, this);
    }

    // TimelineView

    // FIXME: Determine a better way to bridge TimelineView methods to the sub-timeline views.
    get showsLiveRecordingData() { return this._contentViewContainer.currentContentView.showsLiveRecordingData; }
    get showsFilterBar() { return this._contentViewContainer.currentContentView.showsFilterBar; }
    get zeroTime() { return this._contentViewContainer.currentContentView.zeroTime; }
    set zeroTime(x) { this._contentViewContainer.currentContentView.zeroTime = x; }
    get startTime() { return this._contentViewContainer.currentContentView.startTime; }
    set startTime(x) { this._contentViewContainer.currentContentView.startTime = x; }
    get endTime() { return this._contentViewContainer.currentContentView.endTime; }
    set endTime(x) { this._contentViewContainer.currentContentView.endTime = x; }
    get currentTime() { return this._contentViewContainer.currentContentView.currentTime; }
    set currentTime(x) { this._contentViewContainer.currentContentView.currentTime = x; }
    updateFilter(filters) { return this._contentViewContainer.currentContentView.updateFilter(filters); }
    filterDidChange() { return this._contentViewContainer.currentContentView.filterDidChange(); }
    matchDataGridNodeAgainstCustomFilters(node) { return this._contentViewContainer.currentContentView.matchDataGridNodeAgainstCustomFilters(node); }

    selectRecord(record)
    {
        if (record) {
            this._selectedTarget = this._displayedTarget = record.target;
            this._currentContentViewSetting.value = WI.ScriptClusterTimelineView.EventsIdentifier;
            this._updateCurrentContentView();
        }
        this._contentViewContainer.currentContentView.selectRecord(record);
    }

    reset()
    {
        this._selectedTarget = null;
        this._displayedTarget = WI.assumingMainTarget();

        this._pathComponentForTarget.clear();

        for (let contentViews of this._contentViewsForTarget.values()) {
            for (let contentView of contentViews.values())
                contentView.reset();
        }
        this._contentViewsForTarget.clear();

        this._previousContentView = null;
        this._updateCurrentContentView();
    }

    // Public

    get selectionPathComponents()
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return [];

        let components = [];

        let targetPathComponent = this._pathComponentForTarget.get(this._displayedTarget);
        if (targetPathComponent)
            components.push(targetPathComponent);

        switch (this._currentContentViewSetting.value) {
        case WI.ScriptClusterTimelineView.EventsIdentifier:
            components.push(this._eventsPathComponent);
            break;
        case WI.ScriptClusterTimelineView.ProfileIdentifier:
            components.push(this._profilePathComponent);
            break;
        }

        let subComponents = currentContentView.selectionPathComponents;
        if (subComponents)
            components.pushAll(subComponents);

        return components;
    }

    saveToCookie(cookie)
    {
        cookie[WI.ScriptClusterTimelineView.TargetIdentifierCookieKey] = this._displayedTarget.identifier;
        cookie[WI.ScriptClusterTimelineView.ViewIdentifierCookieKey] = this._currentContentViewSetting.value;
    }

    restoreFromCookie(cookie)
    {
        let targetId = cookie[WI.ScriptClusterTimelineView.TargetIdentifierCookieKey];
        this._displayedTarget = this.representedObject.imported ? WI.ImportedTarget.forIdentifier(targetId) : WI.targetManager.targetForIdentifier(targetId);

        this._currentContentViewSetting.value = cookie[WI.ScriptClusterTimelineView.ViewIdentifierCookieKey];
        this._updateCurrentContentView();
    }

    closed()
    {
        this.representedObject.removeEventListener(WI.ScriptTimeline.Event.TargetAdded, this._handleTargetAdded, this);

        super.closed();
    }

    // Private

    _updateCurrentContentView()
    {
        let contentViews = this._contentViewsForTarget.getOrInsert(this._displayedTarget, new Map);
        let contentViewToShow = contentViews.getOrInsertComputed(this._currentContentViewSetting.value, (contentViewIdentifier) => {
            switch (contentViewIdentifier) {
            case WI.ScriptClusterTimelineView.EventsIdentifier:
                return new WI.ScriptDetailsTimelineView(this._displayedTarget, this.representedObject);
            case WI.ScriptClusterTimelineView.ProfileIdentifier:
                return new WI.ScriptProfileTimelineView(this._displayedTarget, this.representedObject);
            }
            console.assert(false, "not reached");
        });
        this.contentViewContainer.showContentView(contentViewToShow);
    }

    _addPathComponentForTarget(target)
    {
        if (this._pathComponentForTarget.size === 1)
            this._pathComponentForTarget.firstValue.selectorArrows = true;

        let className = target.type === WI.TargetType.Worker ? "worker-icon" : "page-icon";
        const textOnly = false;
        let showSelectorArrows = this._pathComponentForTarget.size >= 1;
        let pathComponent = new WI.HierarchicalPathComponent(target.displayName, className, target, textOnly, showSelectorArrows);
        pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._handleTargetPathComponentSelected, this);
        pathComponent.comparisonData = target;

        console.assert(!this._pathComponentForTarget.has(target), target, this);
        this._pathComponentForTarget.set(target, pathComponent);
    }

    _sortTargetPathComponents()
    {
        const rankFunctions = [
            (target) => target === WI.mainTarget,
            (target) => target.type === WI.TargetType.Page,
            (target) => target.type === WI.TargetType.Worker,
            () => true, // Fallback for all other targets (which shouldn't be reached, but just to be safe).
        ];
        let sortedComponents = Array.from(this._pathComponentForTarget.values()).sort((a, b) => {
            let aRank = rankFunctions.findIndex((rankFunction) => rankFunction(a.representedObject));
            let bRank = rankFunctions.findIndex((rankFunction) => rankFunction(b.representedObject));
            return aRank - bRank || a.displayName.extendedLocaleCompare(b.displayName);
        });
        for (let [a, b] of sortedComponents.adjacencies()) {
            a.nextSibling = b;
            b.previousSibling = a;
        }
    }

    _handleViewPathComponentSelected(event)
    {
        let {pathComponent} = event.data;

        this._currentContentViewSetting.value = pathComponent.representedObject;
        this._updateCurrentContentView();
    }

    _handleTargetAdded(event)
    {
        let {target} = event.data;

        this._addPathComponentForTarget(target);
        this._sortTargetPathComponents();

        console.assert(target === this._displayedTarget || !this._contentViewsForTarget.has(target), target, this);

        if (!this._selectedTarget) {
            console.assert(this._pathComponentForTarget.size >= 1, this._pathComponentForTarget);
            let displayedTarget = (!this.representedObject.imported && this._pathComponentForTarget.has(WI.mainTarget)) ? WI.mainTarget : this._pathComponentForTarget.firstKey;
            if (displayedTarget !== this._displayedTarget) {
                this._displayedTarget = displayedTarget;
                this._updateCurrentContentView();
            }
        }

        this.dispatchEventToListeners(WI.ContentView.Event.SelectionPathComponentsDidChange);
    }

    _handleTargetPathComponentSelected(event)
    {
        let {pathComponent} = event.data;

        this._selectedTarget = this._displayedTarget = pathComponent.representedObject;
        this._updateCurrentContentView();
    }

    _scriptClusterViewCurrentContentViewDidChange(event)
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return;

        if (this._previousContentView) {
            currentContentView.zeroTime = this._previousContentView.zeroTime;
            currentContentView.startTime = this._previousContentView.startTime;
            currentContentView.endTime = this._previousContentView.endTime;
            currentContentView.currentTime = this._previousContentView.currentTime;
        }

        this._previousContentView = currentContentView;
    }
};

WI.ScriptClusterTimelineView.TargetIdentifierCookieKey = "script-cluster-timeline-target-identifier";
WI.ScriptClusterTimelineView.ViewIdentifierCookieKey = "script-cluster-timeline-view-identifier";

WI.ScriptClusterTimelineView.EventsIdentifier = "events";
WI.ScriptClusterTimelineView.ProfileIdentifier = "profile";
