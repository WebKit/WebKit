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

    this.reset();
}

WebInspector.ProfilesPanel.prototype = {
    toolbarItemClass: "profiles",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Profiles");
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        this._updateSidebarWidth();

        // FIXME: the only way to get profiles right now is to ask for the array of all profiles
        // from the InspectorController. We need to add callback to the profiler that will notify
        // the Inspector when a new profile is made. That way we can keep the UI updated.

        this.sidebarTree.removeChildren();

        var profiles = InspectorController.allProfiles();
        var profilesLength = profiles.length;
        for (var i = 0; i < profilesLength; ++i) {
            var profile = profiles[i];
            this.addProfile(profile);
        }

        if (profiles[0])
            profiles[0]._profilesTreeElement.select();
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

        this._profiles = [];

        this.sidebarTree.removeChildren();
        this.profileViews.removeChildren();
    },

    handleKeyEvent: function(event)
    {
        this.sidebarTree.handleKeyEvent(event);
    },

    addProfile: function(profile)
    {
        this._profiles.push(profile);

        var profileTreeElement = new WebInspector.ProfileSidebarTreeElement(profile);
        profile._profilesTreeElement = profileTreeElement;

        this.sidebarTree.appendChild(profileTreeElement);
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
    },

    closeVisibleView: function()
    {
        if (this.visibleProfileView)
            this.visibleProfileView.hide();
        delete this.visibleProfileView;
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
            // The stylesheet hasn't loaded yet, so we need to update later.
            setTimeout(this._updateSidebarWidth.bind(this), 0, width);
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
        this.sidebarResizeElement.style.left = (width - 3) + "px";
    }
}

WebInspector.ProfilesPanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.ProfileSidebarTreeElement = function(profile)
{
    this.profile = profile;

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
        return this.profile.title;
    },

    set mainTitle(x)
    {
        // Can't change mainTitle.
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
