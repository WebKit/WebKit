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

WebInspector.CanvasProfileView = function(profile)
{
    console.assert(profile instanceof WebInspector.CanvasProfileObject);

    WebInspector.ProfileView.call(this, profile);
}

WebInspector.CanvasProfileView.prototype = {

    constructor: WebInspector.CanvasProfileView,

    updateLayout: function()
    {
        if (this.dataGrid)
            this.dataGrid.updateLayout();
    },

    get recordingTitle()
    {
        return WebInspector.UIString("Recording Canvas Profile…");
    },

    displayProfile: function()
    {
        var columns = {"number": {title: "#", width: "5%", sortable: false},
                       "source": {title: WebInspector.UIString("Call"), width: "75%", sortable: false, disclosure: true},
                       "location": {title: WebInspector.UIString("Location"), width: "20%", sortable: false}};

        this.dataGrid = new WebInspector.DataGrid(columns);
        this.dataGrid.element.classList.add("canvas-profile-view");

        this.element.appendChild(this.dataGrid.element);
        this.dataGrid.updateLayout();

        this._createProfileNodes();
    },

    _createProfileNodes: function()
    {
        var data = this.profile.data;
        if (!data) {
            // The profiler may have been terminated with the "Clear all profiles." button.
            return;
        }

        this.profile.children = [];
        for (var i = 0; i < data.length; ++i) {
            var node = new WebInspector.CanvasDataGridNode(this, data[i]);
            this.profile.children.push(node);
        }
    },

    rebuildGridItems: function()
    {
        this.dataGrid.removeChildren();

        var children = this.profile.children;
        var count = children.length;

        for (var index = 0; index < count; ++index)
            this.dataGrid.appendChild(children[index]);
    },

    refreshData: function()
    {
        var child = this.dataGrid.children[0];
        while (child) {
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
    }

}

WebInspector.CanvasProfileView.prototype.__proto__ = WebInspector.ProfileView.prototype;
