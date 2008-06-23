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

WebInspector.ProfilesPanel = function()
{
    WebInspector.Panel.call(this);

    this.element.addStyleClass("profiles");

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
        return [this.recordButton, this.profileViewStatusBarItemsContainer];
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

    reset: function()
    {
        this.nextUserInitiatedProfileNumber = 1;

        if (this._profiles) {
            var profiledLength = this._profiles.length;
            for (var i = 0; i < profiledLength; ++i) {
                var profile = this._profiles[i];
                delete profile._profileView;
            }
        }

        this._profiles = [];
        this._profileGroups = {};

        this.sidebarTreeElement.removeStyleClass("some-expandable");

        this.sidebarTree.removeChildren();
        this.profileViews.removeChildren();

        this._shouldPopulateProfiles = true;
    },

    handleKeyEvent: function(event)
    {
        this.sidebarTree.handleKeyEvent(event);
    },

    addProfile: function(profile)
    {
        this._profiles.push(profile);

        var sidebarParent = this.sidebarTree;
        var small = false;
        var alternateTitle;

        if (profile.title !== "org.webkit.profiles.user-initiated") {
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

        if (this.visibleProfileView)
            this.visibleProfileView.hide();

        var view = profile._profileView;
        if (!view) {
            view = new WebInspector.ProfileView(profile);
            profile._profileView = view;
        }

        view.show(this.profileViews);

        this.visibleProfileView = view;

        this.profileViewStatusBarItemsContainer.removeChildren();

        var statusBarItems = view.statusBarItems;
        for (var i = 0; i < statusBarItems.length; ++i)
            this.profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    closeVisibleView: function()
    {
        if (this.visibleProfileView)
            this.visibleProfileView.hide();
        delete this.visibleProfileView;
    },

    _recordClicked: function()
    {
        this.recording = !this.recording;

        if (this.recording) {
            this.recordButton.addStyleClass("toggled-on");
            this.recordButton.title = WebInspector.UIString("Stop profiling.");
            InspectorController.inspectedWindow().console.profile("org.webkit.profiles.user-initiated");
        } else {
            this.recordButton.removeStyleClass("toggled-on");
            this.recordButton.title = WebInspector.UIString("Start profiling.");
            InspectorController.inspectedWindow().console.profileEnd("org.webkit.profiles.user-initiated");
        }
    },

    _populateProfiles: function()
    {
        this.sidebarTree.removeChildren();

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

    if (this.profile.title === "org.webkit.profiles.user-initiated")
        this._profileNumber = WebInspector.panels.profiles.nextUserInitiatedProfileNumber++;

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
        if (this.profile.title === "org.webkit.profiles.user-initiated")
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
