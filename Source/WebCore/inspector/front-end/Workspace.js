/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */
WebInspector.WorkspaceController = function(workspace)
{
    this._workspace = workspace;
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._mainFrameNavigated, this);
}

WebInspector.WorkspaceController.prototype = {
    _mainFrameNavigated: function()
    {
        WebInspector.Revision.filterOutStaleRevisions();
        this._workspace.project().reset();
    }
}

/**
 * @type {?WebInspector.WorkspaceController}
 */
WebInspector.workspaceController = null;

/**
 * @constructor
 */
WebInspector.Project = function(workspace)
{
    this._uiSourceCodes = [];
    this._workspace = workspace;
}

WebInspector.Project.prototype = {
    reset: function()
    {
        this._workspace.dispatchEventToListeners(WebInspector.Workspace.Events.ProjectWillReset, this);
        this._uiSourceCodes = [];
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    addUISourceCode: function(uiSourceCode)
    {
        this._uiSourceCodes.push(uiSourceCode);
        this._workspace.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} oldUISourceCode
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    replaceUISourceCode: function(oldUISourceCode, uiSourceCode)
    {
        this._uiSourceCodes.splice(this._uiSourceCodes.indexOf(oldUISourceCode), 1);
        this._uiSourceCodes.push(uiSourceCode);
        var data = { oldUISourceCode: oldUISourceCode, uiSourceCode: uiSourceCode };
        this._workspace.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, data);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    removeUISourceCode: function(uiSourceCode)
    {
        this._uiSourceCodes.splice(this._uiSourceCodes.indexOf(uiSourceCode), 1);
        this._workspace.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, uiSourceCode);
    },

    /**
     * @param {string} url
     * @return {?WebInspector.UISourceCode}
     */
    uiSourceCodeForURL: function(url)
    {
        for (var i = 0; i < this._uiSourceCodes.length; ++i) {
            if (this._uiSourceCodes[i].url === url)
                return this._uiSourceCodes[i];
        }
        return null;
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        return this._uiSourceCodes;
    }
}

/**
 * @constructor
 * @implements {WebInspector.UISourceCodeProvider}
 * @extends {WebInspector.Object}
 */
WebInspector.Workspace = function()
{
    this._project = new WebInspector.Project(this);
}

WebInspector.Workspace.Events = {
    UISourceCodeContentCommitted: "UISourceCodeContentCommitted",
    ProjectWillReset: "ProjectWillReset"
}

WebInspector.Workspace.prototype = {
    /**
     * @param {string} url
     * @return {?WebInspector.UISourceCode}
     */
    uiSourceCodeForURL: function(url)
    {
        return this._project.uiSourceCodeForURL(url);
    },

    /**
     * @return {WebInspector.Project}
     */
    project: function()
    {
        // FIXME: support several projects.
        return this._project;
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        return this._project.uiSourceCodes();
    }
}

WebInspector.Workspace.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @type {?WebInspector.Workspace}
 */
WebInspector.workspace = null;
