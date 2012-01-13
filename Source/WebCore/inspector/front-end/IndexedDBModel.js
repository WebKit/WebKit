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
 */
WebInspector.IndexedDBModel = function()
{
    IndexedDBAgent.enable();
    InspectorBackend.registerIndexedDBDispatcher(new WebInspector.IndexedDBDispatcher(this));
    
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);
    
    this._securityOriginByFrameId = {};
    this._frameIdsBySecurityOrigin = {};
    this._databaseNamesBySecurityOrigin = {};

    this.refreshDatabaseNames();
}

WebInspector.IndexedDBModel.prototype = {
    refreshDatabaseNames: function()
    {
        this._reset();
        this._framesNavigatedRecursively(WebInspector.resourceTreeModel.mainFrame);
    },
    
    /**
     * @param {WebInspector.ResourceTreeFrame} resourceTreeFrame
     */
    _framesNavigatedRecursively: function(resourceTreeFrame)
    {
        this._processFrameNavigated(resourceTreeFrame);
        for (var i = 0; i < resourceTreeFrame.childFrames.length; ++i)
            this._framesNavigatedRecursively(resourceTreeFrame.childFrames[i]);            
    },
    
    /**
     * @param {WebInspector.Event} event
     */
    _frameNavigated: function(event)
    {
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._processFrameNavigated(resourceTreeFrame);
    },
    
    /**
     * @param {WebInspector.Event} event
     */
    _frameDetached: function(event)
    {
        var resourceTreeFrame = /** @type {WebInspector.ResourceTreeFrame} */ event.data;
        this._originRemovedFromFrame(resourceTreeFrame.id);
    },

    _reset: function()
    {
        this._securityOriginByFrameId = {};
        this._frameIdsBySecurityOrigin = {};
        this._databaseNamesBySecurityOrigin = {};
        // FIXME: dispatch events?
    },
    
    /**
     * @param {WebInspector.ResourceTreeFrame} resourceTreeFrame
     */
    _processFrameNavigated: function(resourceTreeFrame)
    {
        if (resourceTreeFrame.securityOrigin === "null")
            return;
        if (this._frameIdsBySecurityOrigin[resourceTreeFrame.securityOrigin])
            this._originAddedToFrame(resourceTreeFrame.id, resourceTreeFrame.securityOrigin);
        else
            this._loadDatabaseNamesForFrame(resourceTreeFrame.id);
    },
    
    /**
     * @param {string} frameId
     * @param {string} securityOrigin
     */
    _originAddedToFrame: function(frameId, securityOrigin)
    {
        if (!this._frameIdsBySecurityOrigin[securityOrigin]) {
            this._frameIdsBySecurityOrigin[securityOrigin] = [];
            this._frameIdsBySecurityOrigin[securityOrigin].push(frameId);
            this._databaseNamesBySecurityOrigin[securityOrigin] = [];
            // FIXME: dispatch origin added event.
        }
        this._securityOriginByFrameId[frameId] = securityOrigin;
    },
    
    /**
     * @param {string} frameId
     */
    _originRemovedFromFrame: function(frameId)
    {
        var currentSecurityOrigin = this._securityOriginByFrameId[frameId];
        if (!currentSecurityOrigin)
            return;
        
        delete this._securityOriginByFrameId[frameId];

        var frameIdsForOrigin = this._frameIdsBySecurityOrigin[currentSecurityOrigin];
        for (var i = 0; i < frameIdsForOrigin; ++i) {
            if (frameIdsForOrigin[i] === frameId) {
                frameIdsForOrigin.splice(i, 1);
                break;
            }
        }
        if (!frameIdsForOrigin.length)
            this._originRemoved(currentSecurityOrigin);
    },

    /**
     * @param {string} securityOrigin
     */
    _originRemoved: function(securityOrigin)
    {
        var frameIdsForOrigin = this._frameIdsBySecurityOrigin[securityOrigin];
        for (var i = 0; i < frameIdsForOrigin; ++i)
            delete this._securityOriginByFrameId[frameIdsForOrigin[i]]
        delete this._frameIdsBySecurityOrigin[securityOrigin];
        for (var i = 0; i < this._databaseNamesBySecurityOrigin[securityOrigin]; ++i)
            this._databaseRemoved(securityOrigin, this._databaseNamesBySecurityOrigin[securityOrigin][i]);                
        delete this._databaseNamesBySecurityOrigin[securityOrigin];
        // FIXME: dispatch origin removed event.
    },

    /**
     * @param {string} securityOrigin
     * @param {Array.<string>} databaseNames
     */
    _updateOriginDatabaseNames: function(securityOrigin, databaseNames)
    {
        var newDatabaseNames = {};
        for (var i = 0; i < databaseNames.length; ++i)
            newDatabaseNames[databaseNames[i]] = true;
        var oldDatabaseNames = {};
        for (var i = 0; i < this._databaseNamesBySecurityOrigin[securityOrigin].length; ++i)
            oldDatabaseNames[databaseNames[i]] = true;
        
        this._databaseNamesBySecurityOrigin[securityOrigin] = databaseNames;
        
        for (var databaseName in oldDatabaseNames) {
            if (!newDatabaseNames[databaseName])
                this._databaseRemoved(securityOrigin, databaseName);
        }
        for (var databaseName in newDatabaseNames) {
            if (!oldDatabaseNames[databaseName])
                this._databaseAdded(securityOrigin, databaseName);
        }
        
        if (!this._databaseNamesBySecurityOrigin[securityOrigin].length)
            this._originRemoved(securityOrigin);
    },
    
    /**
     * @param {string} securityOrigin
     * @param {string} databaseName
     */
    _databaseAdded: function(securityOrigin, databaseName)
    {
        // FIXME: dispatch database added event.
    },
    
    /**
     * @param {string} securityOrigin
     * @param {string} databaseName
     */
    _databaseRemoved: function(securityOrigin, databaseName)
    {
        // FIXME: dispatch database removed event.
    },

    /**
     * @param {IndexedDBAgent.FrameWithDatabaseNames} frameWithDatabaseNames
     */
    _databaseNamesLoaded: function(frameWithDatabaseNames)
    {
        // FIXME: We should implement events order, so that we could not update databaseNames for a frame that was already detached.
        
        var databaseNames = frameWithDatabaseNames.databaseNames;
        var oldSecurityOrigin = this._securityOriginByFrameId[frameWithDatabaseNames.frameId];
        if (oldSecurityOrigin && oldSecurityOrigin === frameWithDatabaseNames.securityOrigin)
            this._updateOriginDatabaseNames(frameWithDatabaseNames.securityOrigin, frameWithDatabaseNames.databaseNames);
        else {
            this._originRemovedFromFrame(frameWithDatabaseNames.frameId);
            this._originAddedToFrame(frameWithDatabaseNames.frameId, frameWithDatabaseNames.securityOrigin);
            this._updateOriginDatabaseNames(frameWithDatabaseNames.securityOrigin, frameWithDatabaseNames.databaseNames);
        }
    },

    _loadDatabaseNamesForFrame: function(frameId)
    {
        function callback(error)
        {
            if (error) {
                console.error("IndexedDBAgent error: " + error);
                return;
            }
        }
        
        IndexedDBAgent.requestDatabaseNamesForFrame(frameId, callback);
    }
}

WebInspector.IndexedDBModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @implements {IndexedDBAgent.Dispatcher}
 */
WebInspector.IndexedDBDispatcher = function(indexedDBModel)
{
    this._indexedDBModel = indexedDBModel;
}

WebInspector.IndexedDBDispatcher._callbacks = {};

WebInspector.IndexedDBDispatcher.prototype = {
    /**
     * @param {IndexedDBAgent.FrameWithDatabaseNames} frameWithDatabaseNames
     */
    databaseNamesLoaded: function(frameWithDatabaseNames)
    {
        this._indexedDBModel._databaseNamesLoaded(frameWithDatabaseNames);
    }
}
