/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";

/**
 * @constructor
 * @param {string} id
 * @param {string} name
 */
WebInspector.ProfileType = function(id, name)
{
    this._id = id;
    this._name = name;
    /**
     * @type {WebInspector.SidebarSectionTreeElement}
     */
    this.treeElement = null;
}

WebInspector.ProfileType.prototype = {
    get buttonTooltip()
    {
        return "";
    },

    get id()
    {
        return this._id;
    },

    get treeItemTitle()
    {
        return this._name;
    },

    get name()
    {
        return this._name;
    },

    /**
     * @param {WebInspector.ProfilesPanel} profilesPanel
     * @return {boolean}
     */
    buttonClicked: function(profilesPanel)
    {
        return false;
    },

    reset: function()
    {
    },

    get description()
    {
        return "";
    },

    /**
     * @return {Element}
     */
    decorationElement: function()
    {
        return null;
    },

    // Must be implemented by subclasses.
    /**
     * @param {string=} title
     * @return {WebInspector.ProfileHeader}
     */
    createTemporaryProfile: function(title)
    {
        throw new Error("Needs implemented.");
    },

    /**
     * @param {ProfilerAgent.ProfileHeader} profile
     * @return {WebInspector.ProfileHeader}
     */
    createProfile: function(profile)
    {
        throw new Error("Not supported for " + this._name + " profiles.");
    }
}

/**
 * @constructor
 * @param {!WebInspector.ProfileType} profileType
 * @param {string} title
 * @param {number=} uid
 */
WebInspector.ProfileHeader = function(profileType, title, uid)
{
    this._profileType = profileType;
    this.title = title;
    if (uid === undefined) {
        this.uid = -1;
        this.isTemporary = true;
    } else {
        this.uid = uid;
        this.isTemporary = false;
    }
    this._fromFile = false;
}

WebInspector.ProfileHeader.prototype = {
    /**
     * @return {!WebInspector.ProfileType}
     */
    profileType: function()
    {
        return this._profileType;
    },

    /**
     * Must be implemented by subclasses.
     * @return {WebInspector.ProfileSidebarTreeElement}
     */
    createSidebarTreeElement: function()
    {
        throw new Error("Needs implemented.");
    },

    /**
     * @return {?WebInspector.View}
     */
    existingView: function()
    {
        return this._view;
    },

    /**
     * @return {!WebInspector.View}
     */
    view: function()
    {
        if (!this._view)
            this._view = this.createView(WebInspector.ProfilesPanel._instance);
        return this._view;
    },

    /**
     * @param {WebInspector.ProfilesPanel} profilesPanel
     * @return {!WebInspector.View}
     */
    createView: function(profilesPanel)
    {
        throw new Error("Not implemented.");
    },

    /**
     * @param {Function} callback
     */
    load: function(callback) { },

    /**
     * @return {boolean}
     */
    canSaveToFile: function() { return false; },

    saveToFile: function() { throw new Error("Needs implemented"); },

    /**
     * @return {boolean}
     */
    canLoadFromFile: function() { return false; },

    /**
     * @param {File} file
     */
    loadFromFile: function(file) { throw new Error("Needs implemented"); },

    /**
     * @return {boolean}
     */
    fromFile: function() { return this._fromFile; }
}

/**
 * @constructor
 * @extends {WebInspector.Panel}
 * @implements {WebInspector.ContextMenu.Provider}
 */
WebInspector.ProfilesPanel = function()
{
    WebInspector.Panel.call(this, "profiles");
    WebInspector.ProfilesPanel._instance = this;
    this.registerRequiredCSS("panelEnablerView.css");
    this.registerRequiredCSS("heapProfiler.css");
    this.registerRequiredCSS("profilesPanel.css");

    this.createSidebarViewWithTree();

    this.profilesItemTreeElement = new WebInspector.ProfilesSidebarTreeElement(this);
    this.sidebarTree.appendChild(this.profilesItemTreeElement);

    this._profileTypesByIdMap = {};

    var panelEnablerHeading = WebInspector.UIString("You need to enable profiling before you can use the Profiles panel.");
    var panelEnablerDisclaimer = WebInspector.UIString("Enabling profiling will make scripts run slower.");
    var panelEnablerButton = WebInspector.UIString("Enable Profiling");
    this.panelEnablerView = new WebInspector.PanelEnablerView("profiles", panelEnablerHeading, panelEnablerDisclaimer, panelEnablerButton);
    this.panelEnablerView.addEventListener("enable clicked", this.enableProfiler, this);

    this.profileViews = document.createElement("div");
    this.profileViews.id = "profile-views";
    this.splitView.mainElement.appendChild(this.profileViews);

    this._statusBarButtons = [];

    this.enableToggleButton = new WebInspector.StatusBarButton("", "enable-toggle-status-bar-item");
    if (Capabilities.profilerCausesRecompilation) {
        this._statusBarButtons.push(this.enableToggleButton);
        this.enableToggleButton.addEventListener("click", this._onToggleProfiling, this);
    }
    this.recordButton = new WebInspector.StatusBarButton("", "record-profile-status-bar-item");
    this.recordButton.addEventListener("click", this.toggleRecordButton, this);
    this._statusBarButtons.push(this.recordButton);

    this.clearResultsButton = new WebInspector.StatusBarButton(WebInspector.UIString("Clear all profiles."), "clear-status-bar-item");
    this.clearResultsButton.addEventListener("click", this._clearProfiles, this);
    this._statusBarButtons.push(this.clearResultsButton);

    if (WebInspector.experimentsSettings.liveNativeMemoryChart.isEnabled()) {
        this.garbageCollectButton = new WebInspector.StatusBarButton(WebInspector.UIString("Collect Garbage"), "garbage-collect-status-bar-item");
        this.garbageCollectButton.addEventListener("click", this._garbageCollectButtonClicked, this);
        this._statusBarButtons.push(this.garbageCollectButton);
    }

    this.profileViewStatusBarItemsContainer = document.createElement("div");
    this.profileViewStatusBarItemsContainer.className = "status-bar-items";

    this._profiles = [];
    this._profilerEnabled = !Capabilities.profilerCausesRecompilation;

    this._launcherView = new WebInspector.ProfileLauncherView(this);
    this._launcherView.addEventListener(WebInspector.ProfileLauncherView.EventTypes.ProfileTypeSelected, this._onProfileTypeSelected, this);
    this._reset();

    this._registerProfileType(new WebInspector.CPUProfileType());
    if (!WebInspector.WorkerManager.isWorkerFrontend())
        this._registerProfileType(new WebInspector.CSSSelectorProfileType());
    if (Capabilities.heapProfilerPresent)
        this._registerProfileType(new WebInspector.HeapSnapshotProfileType());
    if (WebInspector.experimentsSettings.nativeMemorySnapshots.isEnabled())
        this._registerProfileType(new WebInspector.NativeMemoryProfileType());
    if (WebInspector.experimentsSettings.canvasInspection.isEnabled())
        this._registerProfileType(new WebInspector.CanvasProfileType());

    InspectorBackend.registerProfilerDispatcher(new WebInspector.ProfilerDispatcher(this));

    this._createFileSelectorElement();
    this.element.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), true);

    WebInspector.ContextMenu.registerProvider(this);
}

WebInspector.ProfilesPanel.prototype = {
    _createFileSelectorElement: function()
    {
        if (this._fileSelectorElement)
            this.element.removeChild(this._fileSelectorElement);
        this._fileSelectorElement = WebInspector.createFileSelectorElement(this._loadFromFile.bind(this));
        this.element.appendChild(this._fileSelectorElement);
    },

    /**
     * @param {!File} file
     */
    _loadFromFile: function(file)
    {
        if (!file.name.endsWith(".heapsnapshot")) {
            WebInspector.log(WebInspector.UIString("Only heap snapshots from files with extension '.heapsnapshot' can be loaded."));
            return;
        }

        if (!!this.findTemporaryProfile(WebInspector.HeapSnapshotProfileType.TypeId)) {
            WebInspector.log(WebInspector.UIString("Can't load profile when other profile is recording."));
            return;
        }

        var profileType = this.getProfileType(WebInspector.HeapSnapshotProfileType.TypeId);
        var temporaryProfile = profileType.createTemporaryProfile(UserInitiatedProfileName + "." + file.name);
        this.addProfileHeader(temporaryProfile);

        temporaryProfile._fromFile = true;
        temporaryProfile.loadFromFile(file);
        this._createFileSelectorElement();
    },

    get statusBarItems()
    {
        return this._statusBarButtons.select("element").concat([this.profileViewStatusBarItemsContainer]);
    },

    toggleRecordButton: function()
    {
        var isProfiling = this._selectedProfileType.buttonClicked(this);
        this.recordButton.toggled = isProfiling;
        this.recordButton.title = this._selectedProfileType.buttonTooltip;
        if (isProfiling)
            this._launcherView.profileStarted();
        else
            this._launcherView.profileFinished();
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        this._populateProfiles();
    },

    _profilerWasEnabled: function()
    {
        if (this._profilerEnabled)
            return;

        this._profilerEnabled = true;

        this._reset();
        if (this.isShowing())
            this._populateProfiles();
    },

    _profilerWasDisabled: function()
    {
        if (!this._profilerEnabled)
            return;

        this._profilerEnabled = false;
        this._reset();
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onProfileTypeSelected: function(event)
    {
        this._selectedProfileType = /** @type {!WebInspector.ProfileType} */ (event.data);
        this.recordButton.title = this._selectedProfileType.buttonTooltip;
    },

    _reset: function()
    {
        WebInspector.Panel.prototype.reset.call(this);

        for (var i = 0; i < this._profiles.length; ++i) {
            var view = this._profiles[i].existingView();
            if (view) {
                view.detach();
                if ("dispose" in view)
                    view.dispose();
            }
        }
        delete this.visibleView;

        delete this.currentQuery;
        this.searchCanceled();

        for (var id in this._profileTypesByIdMap) {
            var profileType = this._profileTypesByIdMap[id];
            var treeElement = profileType.treeElement;
            treeElement.removeChildren();
            treeElement.hidden = true;
            profileType.reset();
        }

        this._profiles = [];
        this._profilesIdMap = {};
        this._profileGroups = {};
        this._profileGroupsForLinks = {};
        this._profilesWereRequested = false;
        this.recordButton.toggled = false;
        if (this._selectedProfileType)
            this.recordButton.title = this._selectedProfileType.buttonTooltip;
        this._launcherView.profileFinished();

        this.sidebarTreeElement.removeStyleClass("some-expandable");

        this.profileViews.removeChildren();
        this.profileViewStatusBarItemsContainer.removeChildren();

        this.removeAllListeners();

        this._updateInterface();
        this.profilesItemTreeElement.select();
        this._showLauncherView();
    },

    _showLauncherView: function()
    {
        this.closeVisibleView();
        this.profileViewStatusBarItemsContainer.removeChildren();
        this._launcherView.show(this.splitView.mainElement);
        this.visibleView = this._launcherView;
    },

    _clearProfiles: function()
    {
        ProfilerAgent.clearProfiles();
        this._reset();
    },

    _garbageCollectButtonClicked: function()
    {
        ProfilerAgent.collectGarbage();
    },

    /**
     * @param {WebInspector.ProfileType} profileType
     */
    _registerProfileType: function(profileType)
    {
        this._profileTypesByIdMap[profileType.id] = profileType;
        this._launcherView.addProfileType(profileType);
        profileType.treeElement = new WebInspector.SidebarSectionTreeElement(profileType.treeItemTitle, null, true);
        profileType.treeElement.hidden = true;
        this.sidebarTree.appendChild(profileType.treeElement);
        profileType.treeElement.childrenListElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), true);
    },

    /**
     * @param {Event} event
     */
    _handleContextMenuEvent: function(event)
    {
        var element = event.srcElement;
        while (element && !element.treeElement && element !== this.element)
            element = element.parentElement;
        if (!element)
            return;
        if (element.treeElement && element.treeElement.handleContextMenuEvent) {
            element.treeElement.handleContextMenuEvent(event);
            return;
        }
        if (element !== this.element || event.srcElement === this.sidebarElement) {
            var contextMenu = new WebInspector.ContextMenu(event);
            if (this.visibleView instanceof WebInspector.HeapSnapshotView)
                this.visibleView.populateContextMenu(contextMenu, event);
            contextMenu.appendItem(WebInspector.UIString("Load Heap Snapshot\u2026"), this._fileSelectorElement.click.bind(this._fileSelectorElement));
            contextMenu.show();
        }

    },

    /**
     * @param {string} text
     * @param {string} profileTypeId
     * @return {string}
     */
    _makeTitleKey: function(text, profileTypeId)
    {
        return escape(text) + '/' + escape(profileTypeId);
    },

    /**
     * @param {number} id
     * @param {string} profileTypeId
     * @return {string}
     */
    _makeKey: function(id, profileTypeId)
    {
        return id + '/' + escape(profileTypeId);
    },

    /**
     * @param {WebInspector.ProfileHeader} profile
     */
    addProfileHeader: function(profile)
    {
        this._removeTemporaryProfile(profile.profileType().id);

        var profileType = profile.profileType();
        var typeId = profileType.id;
        var sidebarParent = profileType.treeElement;
        sidebarParent.hidden = false;
        var small = false;
        var alternateTitle;

        this._profiles.push(profile);
        this._profilesIdMap[this._makeKey(profile.uid, typeId)] = profile;

        if (!profile.title.startsWith(UserInitiatedProfileName)) {
            var profileTitleKey = this._makeTitleKey(profile.title, typeId);
            if (!(profileTitleKey in this._profileGroups))
                this._profileGroups[profileTitleKey] = [];

            var group = this._profileGroups[profileTitleKey];
            group.push(profile);

            if (group.length === 2) {
                // Make a group TreeElement now that there are 2 profiles.
                group._profilesTreeElement = new WebInspector.ProfileGroupSidebarTreeElement(profile.title);

                // Insert at the same index for the first profile of the group.
                var index = sidebarParent.children.indexOf(group[0]._profilesTreeElement);
                sidebarParent.insertChild(group._profilesTreeElement, index);

                // Move the first profile to the group.
                var selected = group[0]._profilesTreeElement.selected;
                sidebarParent.removeChild(group[0]._profilesTreeElement);
                group._profilesTreeElement.appendChild(group[0]._profilesTreeElement);
                if (selected)
                    group[0]._profilesTreeElement.revealAndSelect();

                group[0]._profilesTreeElement.small = true;
                group[0]._profilesTreeElement.mainTitle = WebInspector.UIString("Run %d", 1);

                this.sidebarTreeElement.addStyleClass("some-expandable");
            }

            if (group.length >= 2) {
                sidebarParent = group._profilesTreeElement;
                alternateTitle = WebInspector.UIString("Run %d", group.length);
                small = true;
            }
        }

        var profileTreeElement = profile.createSidebarTreeElement();
        profile.sidebarElement = profileTreeElement;
        profileTreeElement.small = small;
        if (alternateTitle)
            profileTreeElement.mainTitle = alternateTitle;
        profile._profilesTreeElement = profileTreeElement;

        sidebarParent.appendChild(profileTreeElement);
        if (!profile.isTemporary) {
            if (!this.visibleView)
                this.showProfile(profile);
            this.dispatchEventToListeners("profile added", {
                type: typeId
            });
        }
    },

    /**
     * @param {WebInspector.ProfileHeader} profile
     */
    _removeProfileHeader: function(profile)
    {
        var sidebarParent = profile.profileType().treeElement;

        for (var i = 0; i < this._profiles.length; ++i) {
            if (this._profiles[i].uid === profile.uid) {
                profile = this._profiles[i];
                this._profiles.splice(i, 1);
                break;
            }
        }
        delete this._profilesIdMap[this._makeKey(profile.uid, profile.profileType().id)];

        var profileTitleKey = this._makeTitleKey(profile.title, profile.profileType().id);
        delete this._profileGroups[profileTitleKey];

        sidebarParent.removeChild(profile._profilesTreeElement);

        if (!profile.isTemporary)
            ProfilerAgent.removeProfile(profile.profileType().id, profile.uid);

        // No other item will be selected if there aren't any other profiles, so
        // make sure that view gets cleared when the last profile is removed.
        if (!this._profiles.length)
            this.closeVisibleView();
    },

    /**
     * @param {WebInspector.ProfileHeader} profile
     */
    showProfile: function(profile)
    {
        if (!profile || profile.isTemporary)
            return;

        var view = profile.view();
        if (view === this.visibleView)
            return;

        this.closeVisibleView();

        view.show(this.profileViews);

        profile._profilesTreeElement._suppressOnSelect = true;
        profile._profilesTreeElement.revealAndSelect();
        delete profile._profilesTreeElement._suppressOnSelect;

        this.visibleView = view;

        this.profileViewStatusBarItemsContainer.removeChildren();

        var statusBarItems = view.statusBarItems;
        if (statusBarItems)
            for (var i = 0; i < statusBarItems.length; ++i)
                this.profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    /**
     * @param {string} typeId
     * @return {!Array.<!WebInspector.ProfileHeader>}
     */
    getProfiles: function(typeId)
    {
        var result = [];
        var profilesCount = this._profiles.length;
        for (var i = 0; i < profilesCount; ++i) {
            var profile = this._profiles[i];
            if (!profile.isTemporary && profile.profileType().id === typeId)
                result.push(profile);
        }
        return result;
    },

    /**
     * @param {ProfilerAgent.HeapSnapshotObjectId} snapshotObjectId
     * @param {string} viewName
     */
    showObject: function(snapshotObjectId, viewName)
    {
        var heapProfiles = this.getProfiles(WebInspector.HeapSnapshotProfileType.TypeId);
        for (var i = 0; i < heapProfiles.length; i++) {
            var profile = heapProfiles[i];
            // TODO: allow to choose snapshot if there are several options.
            if (profile.maxJSObjectId >= snapshotObjectId) {
                this.showProfile(profile);
                profile.view().changeView(viewName, function() {
                    profile.view().dataGrid.highlightObjectByHeapSnapshotId(snapshotObjectId);
                });
                break;
            }
        }
    },

    /**
     * @param {string} typeId
     * @return {WebInspector.ProfileHeader}
     */
    findTemporaryProfile: function(typeId)
    {
        var profilesCount = this._profiles.length;
        for (var i = 0; i < profilesCount; ++i)
            if (this._profiles[i].profileType().id === typeId && this._profiles[i].isTemporary)
                return this._profiles[i];
        return null;
    },

    /**
     * @param {string} typeId
     */
    _removeTemporaryProfile: function(typeId)
    {
        var temporaryProfile = this.findTemporaryProfile(typeId);
        if (temporaryProfile)
            this._removeProfileHeader(temporaryProfile);
    },

    /**
     * @param {string} typeId
     * @param {number} uid
     */
    getProfile: function(typeId, uid)
    {
        return this._profilesIdMap[this._makeKey(uid, typeId)];
    },

    /**
     * @param {number} uid
     * @param {string} chunk
     */
    _addHeapSnapshotChunk: function(uid, chunk)
    {
        var profile = this._profilesIdMap[this._makeKey(uid, WebInspector.HeapSnapshotProfileType.TypeId)];
        if (!profile)
            return;
        profile.transferChunk(chunk);
    },

    /**
     * @param {number} uid
     */
    _finishHeapSnapshot: function(uid)
    {
        var profile = this._profilesIdMap[this._makeKey(uid, WebInspector.HeapSnapshotProfileType.TypeId)];
        if (!profile)
            return;
        profile.finishHeapSnapshot();
    },

    /**
     * @param {WebInspector.View} view
     */
    showView: function(view)
    {
        this.showProfile(view.profile);
    },

    /**
     * @param {string} typeId
     */
    getProfileType: function(typeId)
    {
        return this._profileTypesByIdMap[typeId];
    },

    /**
     * @param {string} url
     */
    showProfileForURL: function(url)
    {
        var match = url.match(WebInspector.ProfileURLRegExp);
        if (!match)
            return;
        this.showProfile(this._profilesIdMap[this._makeKey(Number(match[3]), match[1])]);
    },

    closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.detach();
        delete this.visibleView;
    },

    /**
     * @param {string} title
     * @param {string} typeId
     */
    displayTitleForProfileLink: function(title, typeId)
    {
        title = unescape(title);
        if (title.startsWith(UserInitiatedProfileName)) {
            title = WebInspector.UIString("Profile %d", title.substring(UserInitiatedProfileName.length + 1));
        } else {
            var titleKey = this._makeTitleKey(title, typeId);
            if (!(titleKey in this._profileGroupsForLinks))
                this._profileGroupsForLinks[titleKey] = 0;

            var groupNumber = ++this._profileGroupsForLinks[titleKey];

            if (groupNumber > 2)
                // The title is used in the console message announcing that a profile has started so it gets
                // incremented twice as often as it's displayed
                title += " " + WebInspector.UIString("Run %d", (groupNumber + 1) / 2);
        }

        return title;
    },

    /**
     * @param {string} query
     */
    performSearch: function(query)
    {
        this.searchCanceled();

        var searchableViews = this._searchableViews();
        if (!searchableViews || !searchableViews.length)
            return;

        var visibleView = this.visibleView;

        var matchesCountUpdateTimeout = null;

        function updateMatchesCount()
        {
            WebInspector.searchController.updateSearchMatchesCount(this._totalSearchMatches, this);
            WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex, this);
            matchesCountUpdateTimeout = null;
        }

        function updateMatchesCountSoon()
        {
            if (matchesCountUpdateTimeout)
                return;
            // Update the matches count every half-second so it doesn't feel twitchy.
            matchesCountUpdateTimeout = setTimeout(updateMatchesCount.bind(this), 500);
        }

        function finishedCallback(view, searchMatches)
        {
            if (!searchMatches)
                return;

            this._totalSearchMatches += searchMatches;
            this._searchResults.push(view);

            if (this.searchMatchFound)
                this.searchMatchFound(view, searchMatches);

            updateMatchesCountSoon.call(this);

            if (view === visibleView)
                view.jumpToFirstSearchResult();
        }

        var i = 0;
        var panel = this;
        var boundFinishedCallback = finishedCallback.bind(this);
        var chunkIntervalIdentifier = null;

        // Split up the work into chunks so we don't block the
        // UI thread while processing.

        function processChunk()
        {
            var view = searchableViews[i];

            if (++i >= searchableViews.length) {
                if (panel._currentSearchChunkIntervalIdentifier === chunkIntervalIdentifier)
                    delete panel._currentSearchChunkIntervalIdentifier;
                clearInterval(chunkIntervalIdentifier);
            }

            if (!view)
                return;

            view.currentQuery = query;
            view.performSearch(query, boundFinishedCallback);
        }

        processChunk();

        chunkIntervalIdentifier = setInterval(processChunk, 25);
        this._currentSearchChunkIntervalIdentifier = chunkIntervalIdentifier;
    },

    jumpToNextSearchResult: function()
    {
        if (!this.showView || !this._searchResults || !this._searchResults.length)
            return;

        var showFirstResult = false;

        this._currentSearchResultIndex = this._searchResults.indexOf(this.visibleView);
        if (this._currentSearchResultIndex === -1) {
            this._currentSearchResultIndex = 0;
            showFirstResult = true;
        }

        var currentView = this._searchResults[this._currentSearchResultIndex];

        if (currentView.showingLastSearchResult()) {
            if (++this._currentSearchResultIndex >= this._searchResults.length)
                this._currentSearchResultIndex = 0;
            currentView = this._searchResults[this._currentSearchResultIndex];
            showFirstResult = true;
        }

        WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex, this);

        if (currentView !== this.visibleView) {
            this.showView(currentView);
            WebInspector.searchController.showSearchField();
        }

        if (showFirstResult)
            currentView.jumpToFirstSearchResult();
        else
            currentView.jumpToNextSearchResult();
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this.showView || !this._searchResults || !this._searchResults.length)
            return;

        var showLastResult = false;

        this._currentSearchResultIndex = this._searchResults.indexOf(this.visibleView);
        if (this._currentSearchResultIndex === -1) {
            this._currentSearchResultIndex = 0;
            showLastResult = true;
        }

        var currentView = this._searchResults[this._currentSearchResultIndex];

        if (currentView.showingFirstSearchResult()) {
            if (--this._currentSearchResultIndex < 0)
                this._currentSearchResultIndex = (this._searchResults.length - 1);
            currentView = this._searchResults[this._currentSearchResultIndex];
            showLastResult = true;
        }

        WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex, this);

        if (currentView !== this.visibleView) {
            this.showView(currentView);
            WebInspector.searchController.showSearchField();
        }

        if (showLastResult)
            currentView.jumpToLastSearchResult();
        else
            currentView.jumpToPreviousSearchResult();
    },

    _searchableViews: function()
    {
        var views = [];

        const visibleView = this.visibleView;
        if (visibleView && visibleView.performSearch)
            views.push(visibleView);

        var profilesLength = this._profiles.length;
        for (var i = 0; i < profilesLength; ++i) {
            var profile = this._profiles[i];
            var view = profile.view();
            if (!view.performSearch || view === visibleView)
                continue;
            views.push(view);
        }

        return views;
    },

    searchMatchFound: function(view, matches)
    {
        view.profile._profilesTreeElement.searchMatches = matches;
    },

    searchCanceled: function()
    {
        if (this._searchResults) {
            for (var i = 0; i < this._searchResults.length; ++i) {
                var view = this._searchResults[i];
                if (view.searchCanceled)
                    view.searchCanceled();
                delete view.currentQuery;
            }
        }

        WebInspector.Panel.prototype.searchCanceled.call(this);

        if (this._currentSearchChunkIntervalIdentifier) {
            clearInterval(this._currentSearchChunkIntervalIdentifier);
            delete this._currentSearchChunkIntervalIdentifier;
        }

        this._totalSearchMatches = 0;
        this._currentSearchResultIndex = 0;
        this._searchResults = [];

        if (!this._profiles)
            return;

        for (var i = 0; i < this._profiles.length; ++i) {
            var profile = this._profiles[i];
            profile._profilesTreeElement.searchMatches = 0;
        }
    },

    _updateInterface: function()
    {
        // FIXME: Replace ProfileType-specific button visibility changes by a single ProfileType-agnostic "combo-button" visibility change.
        if (this._profilerEnabled) {
            this.enableToggleButton.title = WebInspector.UIString("Profiling enabled. Click to disable.");
            this.enableToggleButton.toggled = true;
            this.recordButton.visible = true;
            this.profileViewStatusBarItemsContainer.removeStyleClass("hidden");
            this.clearResultsButton.element.removeStyleClass("hidden");
            this.panelEnablerView.detach();
        } else {
            this.enableToggleButton.title = WebInspector.UIString("Profiling disabled. Click to enable.");
            this.enableToggleButton.toggled = false;
            this.recordButton.visible = false;
            this.profileViewStatusBarItemsContainer.addStyleClass("hidden");
            this.clearResultsButton.element.addStyleClass("hidden");
            this.panelEnablerView.show(this.element);
        }
    },

    get profilerEnabled()
    {
        return this._profilerEnabled;
    },

    enableProfiler: function()
    {
        if (this._profilerEnabled)
            return;
        this._toggleProfiling(this.panelEnablerView.alwaysEnabled);
    },

    disableProfiler: function()
    {
        if (!this._profilerEnabled)
            return;
        this._toggleProfiling(this.panelEnablerView.alwaysEnabled);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onToggleProfiling: function(event) {
        this._toggleProfiling(true);
    },

    /**
     * @param {boolean} always
     */
    _toggleProfiling: function(always)
    {
        if (this._profilerEnabled) {
            WebInspector.settings.profilerEnabled.set(false);
            ProfilerAgent.disable(this._profilerWasDisabled.bind(this));
        } else {
            WebInspector.settings.profilerEnabled.set(always);
            ProfilerAgent.enable(this._profilerWasEnabled.bind(this));
        }
    },

    _populateProfiles: function()
    {
        if (!this._profilerEnabled || this._profilesWereRequested)
            return;

        /**
         * @param {?string} error
         * @param {Array.<ProfilerAgent.ProfileHeader>} profileHeaders
         */
        function populateCallback(error, profileHeaders) {
            if (error)
                return;
            profileHeaders.sort(function(a, b) { return a.uid - b.uid; });
            var profileHeadersLength = profileHeaders.length;
            for (var i = 0; i < profileHeadersLength; ++i) {
                var profileHeader = profileHeaders[i];
                var profileType = this.getProfileType(profileHeader.typeId);
                this.addProfileHeader(profileType.createProfile(profileHeader));
            }
        }

        ProfilerAgent.getProfileHeaders(populateCallback.bind(this));

        this._profilesWereRequested = true;
    },

    /**
     * @param {!WebInspector.Event} event
     */
    sidebarResized: function(event)
    {
        var sidebarWidth = /** @type {number} */ (event.data);
        this._resize(sidebarWidth);
    },

    onResize: function()
    {
        this._resize(this.splitView.sidebarWidth());
    },

    /**
     * @param {number} sidebarWidth
     */
    _resize: function(sidebarWidth)
    {
        var lastItemElement = this._statusBarButtons[this._statusBarButtons.length - 1].element;
        var minFloatingStatusBarItemsOffset = lastItemElement.totalOffsetLeft() + lastItemElement.offsetWidth;
        this.profileViewStatusBarItemsContainer.style.left = Math.max(minFloatingStatusBarItemsOffset, sidebarWidth) + "px";
    },

    /**
     * @param {string} profileType
     * @param {boolean} isProfiling
     */
    setRecordingProfile: function(profileType, isProfiling)
    {
        var profileTypeObject = this.getProfileType(profileType);
        profileTypeObject.setRecordingProfile(isProfiling);
        var temporaryProfile = this.findTemporaryProfile(profileType);
        if (!!temporaryProfile === isProfiling)
            return;
        if (!temporaryProfile)
            temporaryProfile = profileTypeObject.createTemporaryProfile();
        if (isProfiling)
            this.addProfileHeader(temporaryProfile);
        else
            this._removeTemporaryProfile(profileType);
        this.recordButton.toggled = isProfiling;
        this.recordButton.title = profileTypeObject.buttonTooltip;
        if (isProfiling)
            this._launcherView.profileStarted();
        else
            this._launcherView.profileFinished();
    },

    takeHeapSnapshot: function()
    {
        var temporaryRecordingProfile = this.findTemporaryProfile(WebInspector.HeapSnapshotProfileType.TypeId);
        if (!temporaryRecordingProfile) {
            var profileTypeObject = this.getProfileType(WebInspector.HeapSnapshotProfileType.TypeId);
            this.addProfileHeader(profileTypeObject.createTemporaryProfile());
        }
        this._launcherView.profileStarted();
        function done() {
            this._launcherView.profileFinished();
        }
        ProfilerAgent.takeHeapSnapshot(done.bind(this));
        WebInspector.userMetrics.ProfilesHeapProfileTaken.record();
    },

    /**
     * @param {number} done
     * @param {number} total
     */
    _reportHeapSnapshotProgress: function(done, total)
    {
        var temporaryProfile = this.findTemporaryProfile(WebInspector.HeapSnapshotProfileType.TypeId);
        if (temporaryProfile) {
            temporaryProfile.sidebarElement.subtitle = WebInspector.UIString("%.2f%", (done / total) * 100);
            temporaryProfile.sidebarElement.wait = true;
            if (done >= total)
                this._removeTemporaryProfile(WebInspector.HeapSnapshotProfileType.TypeId);
        }
    },

    /** 
     * @param {WebInspector.ContextMenu} contextMenu
     * @param {Object} target
     */
    appendApplicableItems: function(event, contextMenu, target)
    {
        if (WebInspector.inspectorView.currentPanel() !== this)
            return;

        var object = /** @type {WebInspector.RemoteObject} */ (target);
        var objectId = object.objectId;
        if (!objectId)
            return;

        var heapProfiles = this.getProfiles(WebInspector.HeapSnapshotProfileType.TypeId);
        if (!heapProfiles.length)
            return;

        function revealInView(viewName)
        {
            ProfilerAgent.getHeapObjectId(objectId, didReceiveHeapObjectId.bind(this, viewName));
        }

        function didReceiveHeapObjectId(viewName, error, result)
        {
            if (WebInspector.inspectorView.currentPanel() !== this)
                return;
            if (!error)
                this.showObject(result, viewName);
        }

        contextMenu.appendItem(WebInspector.UIString("Reveal in Dominators View"), revealInView.bind(this, "Dominators"));
        contextMenu.appendItem(WebInspector.UIString("Reveal in Summary View"), revealInView.bind(this, "Summary"));
    },

    __proto__: WebInspector.Panel.prototype
}

/**
 * @constructor
 * @implements {ProfilerAgent.Dispatcher}
 * @param {WebInspector.ProfilesPanel} profilesPanel
 */
WebInspector.ProfilerDispatcher = function(profilesPanel)
{
    this._profilesPanel = profilesPanel;
}

WebInspector.ProfilerDispatcher.prototype = {
    /**
     * @param {ProfilerAgent.ProfileHeader} profile
     */
    addProfileHeader: function(profile)
    {
        var profileType = this._profilesPanel.getProfileType(profile.typeId);
        this._profilesPanel.addProfileHeader(profileType.createProfile(profile));
    },

    /**
     * @override
     * @param {number} uid
     * @param {string} chunk
     */
    addHeapSnapshotChunk: function(uid, chunk)
    {
        this._profilesPanel._addHeapSnapshotChunk(uid, chunk);
    },

    /**
     * @override
     * @param {number} uid
     */
    finishHeapSnapshot: function(uid)
    {
        this._profilesPanel._finishHeapSnapshot(uid);
    },

    /**
     * @override
     * @param {boolean} isProfiling
     */
    setRecordingProfile: function(isProfiling)
    {
        this._profilesPanel.setRecordingProfile(WebInspector.CPUProfileType.TypeId, isProfiling);
    },

    /**
     * @override
     */
    resetProfiles: function()
    {
        this._profilesPanel._reset();
    },

    /**
     * @override
     * @param {number} done
     * @param {number} total
     */
    reportHeapSnapshotProgress: function(done, total)
    {
        this._profilesPanel._reportHeapSnapshotProgress(done, total);
    }
}

/**
 * @constructor
 * @extends {WebInspector.SidebarTreeElement}
 * @param {!WebInspector.ProfileHeader} profile
 * @param {string} titleFormat
 * @param {string} className
 */
WebInspector.ProfileSidebarTreeElement = function(profile, titleFormat, className)
{
    this.profile = profile;
    this._titleFormat = titleFormat;

    if (this.profile.title.startsWith(UserInitiatedProfileName))
        this._profileNumber = this.profile.title.substring(UserInitiatedProfileName.length + 1);

    WebInspector.SidebarTreeElement.call(this, className, "", "", profile, false);

    this.refreshTitles();
}

WebInspector.ProfileSidebarTreeElement.prototype = {
    onselect: function()
    {
        if (!this._suppressOnSelect)
            this.treeOutline.panel.showProfile(this.profile);
    },

    ondelete: function()
    {
        this.treeOutline.panel._removeProfileHeader(this.profile);
        return true;
    },

    get mainTitle()
    {
        if (this._mainTitle)
            return this._mainTitle;
        if (this.profile.title.startsWith(UserInitiatedProfileName))
            return WebInspector.UIString(this._titleFormat, this._profileNumber);
        return this.profile.title;
    },

    set mainTitle(x)
    {
        this._mainTitle = x;
        this.refreshTitles();
    },

    set searchMatches(matches)
    {
        if (!matches) {
            if (!this.bubbleElement)
                return;
            this.bubbleElement.removeStyleClass("search-matches");
            this.bubbleText = "";
            return;
        }

        this.bubbleText = matches;
        this.bubbleElement.addStyleClass("search-matches");
    },

    /**
     * @param {!Event} event
     */
    handleContextMenuEvent: function(event)
    {
        var profile = this.profile;
        var contextMenu = new WebInspector.ContextMenu(event);
        var profilesPanel = WebInspector.ProfilesPanel._instance;
        // FIXME: use context menu provider
        if (profile.canSaveToFile()) {
            contextMenu.appendItem(WebInspector.UIString("Save Heap Snapshot\u2026"), profile.saveToFile.bind(profile));
            contextMenu.appendItem(WebInspector.UIString("Load Heap Snapshot\u2026"), profilesPanel._fileSelectorElement.click.bind(profilesPanel._fileSelectorElement));
            contextMenu.appendItem(WebInspector.UIString("Delete Heap Snapshot"), this.ondelete.bind(this));
        } else {
            contextMenu.appendItem(WebInspector.UIString("Load Heap Snapshot\u2026"), profilesPanel._fileSelectorElement.click.bind(profilesPanel._fileSelectorElement));
            contextMenu.appendItem(WebInspector.UIString("Delete profile"), this.ondelete.bind(this));
        }
        contextMenu.show();
    },

    __proto__: WebInspector.SidebarTreeElement.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SidebarTreeElement}
 * @param {string} title
 * @param {string=} subtitle
 */
WebInspector.ProfileGroupSidebarTreeElement = function(title, subtitle)
{
    WebInspector.SidebarTreeElement.call(this, "profile-group-sidebar-tree-item", title, subtitle, null, true);
}

WebInspector.ProfileGroupSidebarTreeElement.prototype = {
    onselect: function()
    {
        if (this.children.length > 0)
            WebInspector.ProfilesPanel._instance.showProfile(this.children[this.children.length - 1].profile);
    },

    __proto__: WebInspector.SidebarTreeElement.prototype
}

/**
 * @constructor
 * @extends {WebInspector.SidebarTreeElement}
 * @param {!WebInspector.ProfilesPanel} panel
 */
WebInspector.ProfilesSidebarTreeElement = function(panel)
{
    this._panel = panel;
    this.small = false;

    WebInspector.SidebarTreeElement.call(this, "profile-launcher-view-tree-item", WebInspector.UIString("Profiles"), "", null, false);
}

WebInspector.ProfilesSidebarTreeElement.prototype = {
    onselect: function()
    {
        this._panel._showLauncherView();
    },

    get selectable()
    {
        return true;
    },

    __proto__: WebInspector.SidebarTreeElement.prototype
}

importScript("ProfileDataGridTree.js");
importScript("BottomUpProfileDataGridTree.js");
importScript("CPUProfileView.js");
importScript("CSSSelectorProfileView.js");
importScript("HeapSnapshot.js");
importScript("HeapSnapshotDataGrids.js");
importScript("HeapSnapshotGridNodes.js");
importScript("HeapSnapshotLoader.js");
importScript("HeapSnapshotProxy.js");
importScript("HeapSnapshotView.js");
importScript("HeapSnapshotWorkerDispatcher.js");
importScript("JSHeapSnapshot.js");
importScript("NativeHeapGraph.js");
importScript("NativeMemorySnapshotView.js");
importScript("ProfileLauncherView.js");
importScript("TopDownProfileDataGridTree.js");
importScript("CanvasProfileView.js");
