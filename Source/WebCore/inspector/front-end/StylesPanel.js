/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
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
 */
WebInspector.StylesUISourceCodeProvider = function()
{
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, this._initialize, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.WillLoadCachedResources, this._reset, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.MainFrameNavigated, this._reset, this);

    this._uiSourceCodes = [];
}

WebInspector.StylesUISourceCodeProvider.prototype = {
    /**
     * @return {Array.<WebInspector.UISourceCode>}
     */
    uiSourceCodes: function()
    {
        return this._uiSourceCodes;
    },

    _initialize: function()
    {
        if (this._initialized)
            return;

        function populateFrame(frame)
        {
            for (var i = 0; i < frame.childFrames.length; ++i)
                populateFrame.call(this, frame.childFrames[i]);

            var resources = frame.resources();
            for (var i = 0; i < resources.length; ++i)
                this._resourceAdded({data:resources[i]});
        }
        populateFrame.call(this, WebInspector.resourceTreeModel.mainFrame);

        WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, this._resourceAdded, this);
        this._initialized = true;
    },

    _resourceAdded: function(event)
    {
        var resource = event.data;
        if (resource.type !== WebInspector.resourceTypes.Stylesheet)
            return;
        var uiSourceCode = new WebInspector.StyleSource(resource);
        this._uiSourceCodes.push(uiSourceCode);
        this.dispatchEventToListeners(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, uiSourceCode);
    },

    _reset: function()
    {
        this._uiSourceCodes = [];
    }
}

WebInspector.StylesUISourceCodeProvider.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.UISourceCode}
 * @param {WebInspector.Resource} resource
 */
WebInspector.StyleSource = function(resource)
{
    WebInspector.UISourceCode.call(this, resource.url, resource);
    this._resource = resource;
}

WebInspector.StyleSource.prototype = {
    
}

WebInspector.StyleSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;

/**
 * @constructor
 * @extends {WebInspector.SourceFrame}
 * @param {WebInspector.StyleSource} styleSource
 */
WebInspector.StyleSourceFrame = function(styleSource)
{
    this._resource = styleSource._resource;
    this._styleSource = styleSource;
    WebInspector.SourceFrame.call(this, this._resource.url);
    this._resource.addEventListener(WebInspector.Resource.Events.RevisionAdded, this._contentChanged, this);
}

WebInspector.StyleSourceFrame.prototype = {
    /**
     * @return {boolean}
     */
    canEditSource: function()
    {
        return true;
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestContent: function(callback)
    {
        this._styleSource.requestContent(callback);
    },

    /**
     * @param {string} text
     */
    commitEditing: function(text)
    {
        this._resource.setContent(text, true, function() {});
    },

    afterTextChanged: function(oldRange, newRange)
    {
        function commitIncrementalEdit()
        {
            var text = this._textModel.text;
            this._styleSource.setWorkingCopy(text);
            this._resource.setContent(text, false, function() {});
        }
        const updateTimeout = 200;
        this._incrementalUpdateTimer = setTimeout(commitIncrementalEdit.bind(this), updateTimeout);
    },

    _clearIncrementalUpdateTimer: function()
    {
        if (this._incrementalUpdateTimer)
            clearTimeout(this._incrementalUpdateTimer);
        delete this._incrementalUpdateTimer;
    },

    _contentChanged: function(event)
    {
        this._styleSource.contentChanged(this._resource.content || "");
        this.setContent(this._resource.content, false, "text/stylesheet");
    }
}

WebInspector.StyleSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;
