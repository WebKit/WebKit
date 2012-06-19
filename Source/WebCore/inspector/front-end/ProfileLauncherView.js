/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.ProfileLauncherView = function(profilesPanel)
{
    WebInspector.View.call(this);

    this._panel = profilesPanel;
    this._profileRunning = false;

    this.element.addStyleClass("profile-launcher-view");
    this.element.addStyleClass("panel-enabler-view");

    this._contentElement = document.createElement("div");
    this._contentElement.className = "profile-launcher-view-content";
    this.element.appendChild(this._contentElement);

    var header = this._contentElement.createChild("h1");
    header.textContent = WebInspector.UIString("Select profiling type");

    this._profileTypeSelectorForm = this._contentElement.createChild("form");
    this._contentElement.createChild("div", "flexible-space");

    this._controlButton = this._contentElement.createChild("button", "control-profiling");
    this._controlButton.addEventListener("click", this._controlButtonClicked.bind(this), false);
    this._updateControls();
}

WebInspector.ProfileLauncherView.EventTypes = {
    ProfileTypeSelected: "profile-type-selected"
}

WebInspector.ProfileLauncherView.prototype = {
    /**
     * @param {WebInspector.ProfileType} profileType
     */
    addProfileType: function(profileType)
    {
        var checked = !this._profileTypeSelectorForm.children.length;
        var labelElement = this._profileTypeSelectorForm.createChild("label");
        labelElement.textContent = profileType.name;
        var optionElement = document.createElement("input");
        labelElement.insertBefore(optionElement, labelElement.firstChild);
        optionElement.type = "radio";
        optionElement.name = "profile-type";
        if (checked) {
            optionElement.checked = checked;
            this.dispatchEventToListeners(WebInspector.ProfileLauncherView.EventTypes.ProfileTypeSelected, profileType);
        }
        optionElement.addEventListener("change", this._profileTypeChanged.bind(this, profileType), false);
        var descriptionElement = labelElement.createChild("p");
        descriptionElement.textContent = profileType.description;
    },

    _controlButtonClicked: function()
    {
        this._panel.toggleRecordButton();
    },

    _updateControls: function()
    {
        if (this._isProfiling) {
            this._profileTypeSelectorForm.disabled = true;
            this._controlButton.addStyleClass("running");
            this._controlButton.textContent = WebInspector.UIString("Stop");
        } else {
            this._profileTypeSelectorForm.disabled = false;
            this._controlButton.removeStyleClass("running");
            this._controlButton.textContent = WebInspector.UIString("Start");
        }
    },

    /**
     * @param {WebInspector.ProfileType} profileType
     */
    _profileTypeChanged: function(profileType, event)
    {
        this.dispatchEventToListeners(WebInspector.ProfileLauncherView.EventTypes.ProfileTypeSelected, profileType);
    },

    profileStarted: function()
    {
        this._isProfiling = true;
        this._updateControls();
    },

    profileFinished: function()
    {
        this._isProfiling = false;
        this._updateControls();
    }
}

WebInspector.ProfileLauncherView.prototype.__proto__ = WebInspector.View.prototype;
