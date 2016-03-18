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

WebInspector.ScriptClusterTimelineView = class ScriptClusterTimelineView extends WebInspector.ClusterContentView
{
    constructor(timeline, extraArguments)
    {
        super(timeline);

        console.assert(timeline.type === WebInspector.TimelineRecord.Type.Script);

        this._currentContentViewSetting = new WebInspector.Setting("script-cluster-timeline-view-current-view", WebInspector.ScriptClusterTimelineView.EventsIdentifier);

        let showSelectorArrows = this._canShowProfileView();
        function createPathComponent(displayName, className, identifier)
        {
            let pathComponent = new WebInspector.HierarchicalPathComponent(displayName, className, identifier, false, showSelectorArrows);
            pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
            return pathComponent;
        }

        this._eventsPathComponent = createPathComponent.call(this, WebInspector.UIString("Events"), "events-icon", WebInspector.ScriptClusterTimelineView.EventsIdentifier);
        this._profilePathComponent = createPathComponent.call(this, WebInspector.UIString("Call Trees"), "call-trees-icon", WebInspector.ScriptClusterTimelineView.ProfileIdentifier);

        if (this._canShowProfileView()) {
            this._eventsPathComponent.nextSibling = this._profilePathComponent;
            this._profilePathComponent.previousSibling = this._eventsPathComponent;
        }

        // FIXME: We should be able to create these lazily.
        this._eventsContentView = new WebInspector.ScriptDetailsTimelineView(this.representedObject, extraArguments);
        this._profileContentView = this._canShowProfileView() ? new WebInspector.ScriptProfileTimelineView(this.representedObject, extraArguments) : null;

        this._showContentViewForIdentifier(this._currentContentViewSetting.value);

        this.contentViewContainer.addEventListener(WebInspector.ContentViewContainer.Event.CurrentContentViewDidChange, this._scriptClusterViewCurrentContentViewDidChange, this)
    }

    // TimelineView

    // FIXME: Determine a better way to bridge TimelineView methods to the sub-timeline views.
    get zeroTime() { return this._contentViewContainer.currentContentView.zeroTime; }
    set zeroTime(x) { this._contentViewContainer.currentContentView.zeroTime = x; }
    get startTime() { return this._contentViewContainer.currentContentView.startTime; }
    set startTime(x) { this._contentViewContainer.currentContentView.startTime = x; }
    get endTime() { return this._contentViewContainer.currentContentView.endTime; }
    set endTime(x) { this._contentViewContainer.currentContentView.endTime = x; }
    get currentTime() { return this._contentViewContainer.currentContentView.currentTime; }
    set currentTime(x) { this._contentViewContainer.currentContentView.currentTime = x; }
    get navigationSidebarTreeOutline() { return this._contentViewContainer.currentContentView.navigationSidebarTreeOutline; }
    reset() { return this._contentViewContainer.currentContentView.reset(); }
    filterDidChange() { return this._contentViewContainer.currentContentView.filterDidChange(); }
    matchTreeElementAgainstCustomFilters(treeElement) { return this._contentViewContainer.currentContentView.matchTreeElementAgainstCustomFilters(treeElement); }

    // Public

    get eventsContentView()
    {
        return this._eventsContentView;
    }

    get profileContentView()
    {
        return this._profileContentView;
    }

    get selectionPathComponents()
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        if (!currentContentView)
            return [];

        let components = [this._pathComponentForContentView(currentContentView)];
        let subComponents = currentContentView.selectionPathComponents;
        if (subComponents)
            return components.concat(subComponents);
        return components;
    }

    saveToCookie(cookie)
    {
        cookie[WebInspector.ScriptClusterTimelineView.ContentViewIdentifierCookieKey] = this._currentContentViewSetting.value;
    }

    restoreFromCookie(cookie)
    {
        this._showContentViewForIdentifier(cookie[WebInspector.ScriptClusterTimelineView.ContentViewIdentifierCookieKey]);
    }

    showEvents()
    {
        return this._showContentViewForIdentifier(WebInspector.ScriptClusterTimelineView.EventsIdentifier);
    }

    showProfile()
    {
        if (!this._canShowProfileView())
            return this.showEvents();

        return this._showContentViewForIdentifier(WebInspector.ScriptClusterTimelineView.ProfileIdentifier);
    }

    // Private

    _canShowProfileView()
    {
        // COMPATIBILITY (iOS 9): Legacy backends did not include CallingContextTree ScriptProfiler data.
        return window.ScriptProfilerAgent;
    }

    _pathComponentForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._eventsContentView)
            return this._eventsPathComponent;
        if (contentView === this._profileContentView)
            return this._profilePathComponent;
        console.error("Unknown contentView.");
        return null;
    }

    _identifierForContentView(contentView)
    {
        console.assert(contentView);
        if (!contentView)
            return null;
        if (contentView === this._eventsContentView)
            return WebInspector.ScriptClusterTimelineView.EventsIdentifier;
        if (contentView === this._profileContentView)
            return WebInspector.ScriptClusterTimelineView.ProfileIdentifier;
        console.error("Unknown contentView.");
        return null;
    }

    _showContentViewForIdentifier(identifier)
    {
        let contentViewToShow = null;

        switch (identifier) {
        case WebInspector.ScriptClusterTimelineView.EventsIdentifier:
            contentViewToShow = this.eventsContentView;
            break;
        case WebInspector.ScriptClusterTimelineView.ProfileIdentifier:
            contentViewToShow = this.profileContentView;
            break;
        }

        if (!contentViewToShow)
            contentViewToShow = this.eventsContentView;

        console.assert(contentViewToShow);

        this._currentContentViewSetting.value = this._identifierForContentView(contentViewToShow);

        return this.contentViewContainer.showContentView(contentViewToShow);
    }

    _pathComponentSelected(event)
    {
        this._showContentViewForIdentifier(event.data.pathComponent.representedObject);
    }

    _scriptClusterViewCurrentContentViewDidChange(event)
    {
        let currentContentView = this._contentViewContainer.currentContentView;
        let previousContentView = currentContentView === this._eventsContentView ? this._profileContentView : this._eventsContentView;

        currentContentView.zeroTime = previousContentView.zeroTime;
        currentContentView.startTime = previousContentView.startTime;
        currentContentView.endTime = previousContentView.endTime;
        currentContentView.currentTime = previousContentView.currentTime;

        // FIXME: <https://webkit.org/b/154924> Web Inspector: hook up grid row filtering in the new Timelines UI
    }
};

WebInspector.ScriptClusterTimelineView.ContentViewIdentifierCookieKey = "script-cluster-timeline-view-identifier";

WebInspector.ScriptClusterTimelineView.EventsIdentifier = "events";
WebInspector.ScriptClusterTimelineView.ProfileIdentifier = "profile";
