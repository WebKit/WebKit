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

WebInspector.ProfilesPanel = function()
{
    WebInspector.Panel.call(this);

    this.element.addStyleClass("profiles");

    var panelEnablerHeading = WebInspector.UIString("You need to enable profiling before you can use the Profiles panel.");
    var panelEnablerDisclaimer = WebInspector.UIString("Enabling profiling will make scripts run slower.");
    var panelEnablerButton = WebInspector.UIString("Enable Profiling");
    this.panelEnablerView = new WebInspector.PanelEnablerView("profiles", panelEnablerHeading, panelEnablerDisclaimer, panelEnablerButton);
    this.panelEnablerView.addEventListener("enable clicked", this._enableProfiling, this);

    this.element.appendChild(this.panelEnablerView.element);

    this.sidebarElement = document.createElement("div");
    this.sidebarElement.id = "profiles-sidebar";
    this.sidebarElement.className = "sidebar";
    this.element.appendChild(this.sidebarElement);

    this.sidebarResizeElement = document.createElement("div");
    this.sidebarResizeElement.className = "sidebar-resizer-vertical";
    this.sidebarResizeElement.addEventListener("mousedown", this._startSidebarDragging.bind(this), false);
    this.element.appendChild(this.sidebarResizeElement);

    this.sidebarTreeElement = document.createElement("ol");
    this.sidebarTreeElement.className = "sidebar-tree";
    this.sidebarElement.appendChild(this.sidebarTreeElement);

    this.sidebarTree = new TreeOutline(this.sidebarTreeElement);

    this.profileViews = document.createElement("div");
    this.profileViews.id = "profile-views";
    this.element.appendChild(this.profileViews);

    this.enableToggleButton = document.createElement("button");
    this.enableToggleButton.className = "enable-toggle-status-bar-item status-bar-item";
    this.enableToggleButton.addEventListener("click", this._toggleProfiling.bind(this), false);

    this.recordButton = document.createElement("button");
    this.recordButton.title = WebInspector.UIString("Start profiling.");
    this.recordButton.id = "record-profile-status-bar-item";
    this.recordButton.className = "status-bar-item";
    this.recordButton.addEventListener("click", this._recordClicked.bind(this), false);

    this.recording = false;

    this.profileViewStatusBarItemsContainer = document.createElement("div");
    this.profileViewStatusBarItemsContainer.id = "profile-view-status-bar-items";

    this.reset();
}

WebInspector.ProfilesPanel.prototype = {
    toolbarItemClass: "profiles",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Profiles");
    },

    get statusBarItems()
    {
        return [this.enableToggleButton, this.recordButton, this.profileViewStatusBarItemsContainer];
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        this._updateSidebarWidth();
        if (this._shouldPopulateProfiles)
            this._populateProfiles();
    },

    populateInterface: function()
    {
        if (this.visible)
            this._populateProfiles();
        else
            this._shouldPopulateProfiles = true;
    },

    profilerWasEnabled: function()
    {
        this.reset();
        this.populateInterface();
    },

    profilerWasDisabled: function()
    {
        this.reset();
    },

    reset: function()
    {
        if (this._profiles) {
            var profiledLength = this._profiles.length;
            for (var i = 0; i < profiledLength; ++i) {
                var profile = this._profiles[i];
                delete profile._profileView;
            }
        }

        delete this.currentQuery;
        this.searchCanceled();

        this._profiles = [];
        this._profilesIdMap = {};
        this._profileGroups = {};
        this._profileGroupsForLinks = {}

        this.sidebarTreeElement.removeStyleClass("some-expandable");

        this.sidebarTree.removeChildren();
        this.profileViews.removeChildren();

        this.profileViewStatusBarItemsContainer.removeChildren();

        this._updateInterface();
    },

    handleKeyEvent: function(event)
    {
        this.sidebarTree.handleKeyEvent(event);
    },

    addProfile: function(profile)
    {
        this._profiles.push(profile);
        this._profilesIdMap[profile.uid] = profile;

        var sidebarParent = this.sidebarTree;
        var small = false;
        var alternateTitle;

        if (profile.title.indexOf(UserInitiatedProfileName) !== 0) {
            if (!(profile.title in this._profileGroups))
                this._profileGroups[profile.title] = [];

            var group = this._profileGroups[profile.title];
            group.push(profile);

            if (group.length === 2) {
                // Make a group TreeElement now that there are 2 profiles.
                group._profilesTreeElement = new WebInspector.ProfileGroupSidebarTreeElement(profile.title);

                // Insert at the same index for the first profile of the group.
                var index = this.sidebarTree.children.indexOf(group[0]._profilesTreeElement);
                this.sidebarTree.insertChild(group._profilesTreeElement, index);

                // Move the first profile to the group.
                var selected = group[0]._profilesTreeElement.selected;
                this.sidebarTree.removeChild(group[0]._profilesTreeElement);
                group._profilesTreeElement.appendChild(group[0]._profilesTreeElement);
                if (selected) {
                    group[0]._profilesTreeElement.select();
                    group[0]._profilesTreeElement.reveal();
                }

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

        var profileTreeElement = new WebInspector.ProfileSidebarTreeElement(profile);
        profileTreeElement.small = small;
        if (alternateTitle)
            profileTreeElement.mainTitle = alternateTitle;
        profile._profilesTreeElement = profileTreeElement;

        sidebarParent.appendChild(profileTreeElement);
    },

    showProfile: function(profile)
    {
        if (!profile)
            return;

        if (this.visibleView)
            this.visibleView.hide();

        var view = this.profileViewForProfile(profile);

        view.show(this.profileViews);

        profile._profilesTreeElement.select(true);
        profile._profilesTreeElement.reveal();

        this.visibleView = view;

        this.profileViewStatusBarItemsContainer.removeChildren();

        var statusBarItems = view.statusBarItems;
        for (var i = 0; i < statusBarItems.length; ++i)
            this.profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    showView: function(view)
    {
        this.showProfile(view.profile);
    },

    profileViewForProfile: function(profile)
    {
        if (!profile)
            return null;
        if (!profile._profileView)
            profile._profileView = new WebInspector.ProfileView(profile);
        return profile._profileView;
    },

    showProfileById: function(uid)
    {
        this.showProfile(this._profilesIdMap[uid]);
    },

    closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.hide();
        delete this.visibleView;
    },

    displayTitleForProfileLink: function(title)
    {
        title = unescape(title);
        if (title.indexOf(UserInitiatedProfileName) === 0) {
            title = WebInspector.UIString("Profile %d", title.substring(UserInitiatedProfileName.length + 1));
        } else {
            if (!(title in this._profileGroupsForLinks))
                this._profileGroupsForLinks[title] = 0;

            groupNumber = ++this._profileGroupsForLinks[title];

            if (groupNumber > 2)
                // The title is used in the console message announcing that a profile has started so it gets
                // incremented twice as often as it's displayed
                title += " " + WebInspector.UIString("Run %d", groupNumber / 2);
        }
        
        return title;
    },

    get searchableViews()
    {
        var views = [];

        const visibleView = this.visibleView;
        if (visibleView && visibleView.performSearch)
            views.push(visibleView);

        var profilesLength = this._profiles.length;
        for (var i = 0; i < profilesLength; ++i) {
            var view = this.profileViewForProfile(this._profiles[i]);
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

    searchCanceled: function(startingNewSearch)
    {
        WebInspector.Panel.prototype.searchCanceled.call(this, startingNewSearch);

        if (!this._profiles)
            return;

        for (var i = 0; i < this._profiles.length; ++i) {
            var profile = this._profiles[i];
            profile._profilesTreeElement.searchMatches = 0;
        }
    },

    setRecordingProfile: function(isProfiling)
    {
        this.recording = isProfiling;

        if (isProfiling) {
            this.recordButton.addStyleClass("toggled-on");
            this.recordButton.title = WebInspector.UIString("Stop profiling.");
        } else {
            this.recordButton.removeStyleClass("toggled-on");
            this.recordButton.title = WebInspector.UIString("Start profiling.");
        }
    },

    _updateInterface: function()
    {
        if (InspectorController.profilerEnabled()) {
            this.enableToggleButton.title = WebInspector.UIString("Profiling enabled. Click to disable.");
            this.enableToggleButton.addStyleClass("toggled-on");
            this.recordButton.removeStyleClass("hidden");
            this.profileViewStatusBarItemsContainer.removeStyleClass("hidden");
            this.panelEnablerView.visible = false;
        } else {
            this.enableToggleButton.title = WebInspector.UIString("Profiling disabled. Click to enable.");
            this.enableToggleButton.removeStyleClass("toggled-on");
            this.recordButton.addStyleClass("hidden");
            this.profileViewStatusBarItemsContainer.addStyleClass("hidden");
            this.panelEnablerView.visible = true;
        }
    },

    _recordClicked: function()
    {
        this.recording = !this.recording;

        if (this.recording)
            InspectorController.startProfiling();
        else
            InspectorController.stopProfiling();
    },

    _enableProfiling: function()
    {
        if (InspectorController.profilerEnabled())
            return;
        this._toggleProfiling(this.panelEnablerView.alwaysEnabled);
    },

    _toggleProfiling: function(optionalAlways)
    {
        if (InspectorController.profilerEnabled())
            InspectorController.disableProfiler(true);
        else
            InspectorController.enableProfiler(!!optionalAlways);
    },

    _populateProfiles: function()
    {
        if (this.sidebarTree.children.length)
            return;

        var profiles = InspectorController.profiles();
        var profilesLength = profiles.length;
        for (var i = 0; i < profilesLength; ++i) {
            var profile = profiles[i];
            this.addProfile(profile);
        }

        if (this.sidebarTree.children[0])
            this.sidebarTree.children[0].select();

        delete this._shouldPopulateProfiles;
    },

    _startSidebarDragging: function(event)
    {
        WebInspector.elementDragStart(this.sidebarResizeElement, this._sidebarDragging.bind(this), this._endSidebarDragging.bind(this), event, "col-resize");
    },

    _sidebarDragging: function(event)
    {
        this._updateSidebarWidth(event.pageX);

        event.preventDefault();
    },

    _endSidebarDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    },

    _updateSidebarWidth: function(width)
    {
        if (this.sidebarElement.offsetWidth <= 0) {
            // The stylesheet hasn't loaded yet or the window is closed,
            // so we can't calculate what is need. Return early.
            return;
        }

        if (!("_currentSidebarWidth" in this))
            this._currentSidebarWidth = this.sidebarElement.offsetWidth;

        if (typeof width === "undefined")
            width = this._currentSidebarWidth;

        width = Number.constrain(width, Preferences.minSidebarWidth, window.innerWidth / 2);

        this._currentSidebarWidth = width;

        this.sidebarElement.style.width = width + "px";
        this.profileViews.style.left = width + "px";
        this.profileViewStatusBarItemsContainer.style.left = width + "px";
        this.sidebarResizeElement.style.left = (width - 3) + "px";
    }
}

WebInspector.ProfilesPanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.ProfileSidebarTreeElement = function(profile)
{
    this.profile = profile;

    if (this.profile.title.indexOf(UserInitiatedProfileName) === 0)
        this._profileNumber = this.profile.title.substring(UserInitiatedProfileName.length + 1);

    WebInspector.SidebarTreeElement.call(this, "profile-sidebar-tree-item", "", "", profile, false);

    this.refreshTitles();
}

WebInspector.ProfileSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.profiles.showProfile(this.profile);
    },

    get mainTitle()
    {
        if (this._mainTitle)
            return this._mainTitle;
        if (this.profile.title.indexOf(UserInitiatedProfileName) === 0)
            return WebInspector.UIString("Profile %d", this._profileNumber);
        return this.profile.title;
    },

    set mainTitle(x)
    {
        this._mainTitle = x;
        this.refreshTitles();
    },

    get subtitle()
    {
        // There is no subtitle.
    },

    set subtitle(x)
    {
        // Can't change subtitle.
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
    }
}

WebInspector.ProfileSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.ProfileGroupSidebarTreeElement = function(title, subtitle)
{
    WebInspector.SidebarTreeElement.call(this, "profile-group-sidebar-tree-item", title, subtitle, null, true);
}

WebInspector.ProfileGroupSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.profiles.showProfile(this.children[this.children.length - 1].profile);
    }
}

WebInspector.ProfileGroupSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;
