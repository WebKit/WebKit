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
 * @extends {WebInspector.Object}
 * @implements {WebInspector.UISourceCodeProvider}
 * @param {Array.<WebInspector.UISourceCodeProvider>} uiSourceCodeProviders
 */
WebInspector.CompositeUISourceCodeProvider = function(uiSourceCodeProviders)
{
    WebInspector.Object.call(this);
    this._uiSourceCodeProviders = [];
    for (var i = 0; i < uiSourceCodeProviders.length; ++i)
        this._registerUISourceCodeProvider(uiSourceCodeProviders[i]);
}

WebInspector.CompositeUISourceCodeProvider.prototype = {
    /**
     * @param {WebInspector.UISourceCodeProvider} uiSourceCodeProvider
     */
    _registerUISourceCodeProvider: function(uiSourceCodeProvider)
    {
        this._uiSourceCodeProviders.push(uiSourceCodeProvider);
        uiSourceCodeProvider.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, this._handleUISourceCodeAdded, this);
        uiSourceCodeProvider.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, this._handleUISourceCodeReplaced, this);
        uiSourceCodeProvider.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, this._handleUISourceCodeRemoved, this);
    },

    _handleUISourceCodeAdded: function(event)
    {
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, event.data);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeReplaced: function(event)
    {
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, event.data);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _handleUISourceCodeRemoved: function(event)
    {
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, event.data);
    },

    /**
     * @return {Array.<WebInspector.UISourceCodeProvider>}
     */
    uiSourceCodeProviders: function()
    {
        return this._uiSourceCodeProviders.slice(0);
    },

    /**
     * @param {string} url
     * @return {?WebInspector.UISourceCode}
     */
    uiSourceCodeForURL: function(url)
    {
        for (var i = 0; i < this._uiSourceCodeProviders.length; ++i) {
            var uiSourceCodes = this._uiSourceCodeProviders[i].uiSourceCodes();
            for (var j = 0; j < uiSourceCodes.length; ++j) {
                if (uiSourceCodes[j].url === url)
                    return uiSourceCodes[j];
            }
        }
        return null;
    },

    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        var result = [];
        for (var i = 0; i < this._uiSourceCodeProviders.length; ++i) {
            var uiSourceCodes = this._uiSourceCodeProviders[i].uiSourceCodes();
            for (var j = 0; j < uiSourceCodes.length; ++j)
                result.push(uiSourceCodes[j]);
        }
        return result;
    }
}

WebInspector.CompositeUISourceCodeProvider.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.CompositeUISourceCodeProvider}
 */
WebInspector.Workspace = function()
{
    var scriptMapping = new WebInspector.DebuggerScriptMapping();
    var styleProviders = [new WebInspector.StylesUISourceCodeProvider()];
    if (WebInspector.experimentsSettings.sass.isEnabled())
        styleProviders.push(new WebInspector.SASSSourceMapping());
    var providers = scriptMapping.uiSourceCodeProviders().concat(styleProviders);
    WebInspector.CompositeUISourceCodeProvider.call(this, providers);

    new WebInspector.PresentationConsoleMessageHelper(this);
    
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._reset, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, this._reset, this);
}

WebInspector.Workspace.Events = {
    UISourceCodeContentCommitted: "uiSourceCodeContentCommitted",
    WorkspaceReset: "WorkspaceReset"
}

WebInspector.Workspace.prototype = {
    /**
     * @param {WebInspector.UISourceCodeProvider} uiSourceCodeProvider
     */
    registerUISourceCodeProvider: function(uiSourceCodeProvider)
    {
        this._registerUISourceCodeProvider(uiSourceCodeProvider);
    },

    _reset: function()
    {
        var uiSourceCodeProviders = this.uiSourceCodeProviders();
        for (var i = 0; i < uiSourceCodeProviders.length; ++i) {
            uiSourceCodeProviders[i].reset();
        }
        WebInspector.Revision.filterOutStaleRevisions();
        this.dispatchEventToListeners(WebInspector.Workspace.Events.WorkspaceReset, null);
    }
}

WebInspector.Workspace.prototype.__proto__ = WebInspector.CompositeUISourceCodeProvider.prototype;

/**
 * @type {?WebInspector.Workspace}
 */
WebInspector.workspace = null;
