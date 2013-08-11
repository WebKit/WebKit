/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.CanvasProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.CanvasProfileType.TypeId, WebInspector.UIString("Collect Canvas Profile"));
    this._recording = false;
    this._profileId = 1;
    WebInspector.CanvasProfileType.instance = this;
}

WebInspector.CanvasProfileType.TypeId = "CANVAS";

WebInspector.CanvasProfileType.prototype = {

    constructor: WebInspector.CanvasProfileType,

    get buttonTooltip()
    {
        return this._recording ? WebInspector.UIString("Stop Canvas profiling.") : WebInspector.UIString("Start Canvas profiling.");
    },

    buttonClicked: function()
    {
        if (this._recording)
            this.stopRecordingProfile();
        else
            this.startRecordingProfile();
    },

    get treeItemTitle()
    {
        return WebInspector.UIString("CANVAS PROFILES");
    },

    get description()
    {
        return WebInspector.UIString("Canvas profiles allow you to examine drawing operations on canvas elements.");
    },

    reset: function()
    {
        this._profileId = 1;
    },

    nextProfileId: function()
    {
        return this._profileId++;
    },

    isRecordingProfile: function()
    {
        return this._recording;
    },

    setRecordingProfile: function(isProfiling)
    {
        this._recording = isProfiling;
    },

    startRecordingProfile: function()
    {
        this._recording = true;
        // FIXME: This is where we would ask the Canvas Agent to profile.
    },

    stopRecordingProfile: function(callback)
    {
        this._recording = false;
        // FIXME: This is where we would ask the Canvas Agent to stop profiling.
        // For now pass in an empty array of canvas calls.
        callback(null, {data: []});
    },

    createSidebarTreeElementForProfile: function(profile)
    {
        return new WebInspector.ProfileSidebarTreeElement(profile, profile.title, "profile-sidebar-tree-item");
    },

    createView: function(profile)
    {
        return new WebInspector.CanvasProfileView(profile);
    }
}

WebInspector.CanvasProfileType.prototype.__proto__ = WebInspector.ProfileType.prototype;
