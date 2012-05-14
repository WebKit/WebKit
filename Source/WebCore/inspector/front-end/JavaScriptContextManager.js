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
WebInspector.JavaScriptContextManager = function(resourceTreeModel, consoleView)
{
    resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameAdded, this._frameAdded, this);
    resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameNavigated, this._frameNavigated, this);
    resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.FrameDetached, this._frameDetached, this);
    resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, this._didLoadCachedResources, this);
    this._consoleView = consoleView;
    this._frameIdToContext = {};
}

WebInspector.JavaScriptContextManager.prototype = {
    /**
     * @param {WebInspector.ResourceTreeFrame} frame
     * @param {string} securityOrigin
     */
    contextByFrameAndSecurityOrigin: function(frame, securityOrigin)
    {
        var frameContext = this._frameIdToContext[frame.id];
        return frameContext && frameContext.contextBySecurityOrigin(securityOrigin);
    },

    _frameAdded: function(event)
    {
        var frame = event.data;
        var context = new WebInspector.FrameEvaluationContext(frame);
        this._frameIdToContext[frame.id] = context;
        this._consoleView.addContext(context);
    },

    _frameNavigated: function(event)
    {
        var frame = event.data;
        var context = this._frameIdToContext[frame.id];
        if (context)
            context._frameNavigated(frame);
    },

    _frameDetached: function(event)
    {
        var frame = event.data;
        var context = this._frameIdToContext[frame.id];
        if (!context)
            return;
        this._consoleView.removeContext(context);
        delete this._frameIdToContext[frame.id];
    },

    _didLoadCachedResources: function()
    {
        InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeDispatcher(this));
        RuntimeAgent.setReportExecutionContextCreation(true);
    },

    isolatedContextCreated: function(context)
    {
        var frameEvaluationContext = this._frameIdToContext[context.frameId];
        // FIXME(85708): this should never happen
        if (!frameEvaluationContext)
            return;
        frameEvaluationContext._addExecutionContext(new WebInspector.ExecutionContext(context.id, context.name, context.isPageContext));
    }
}

WebInspector.JavaScriptContextManager.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @type {WebInspector.JavaScriptContextManager}
 */
WebInspector.javaScriptContextManager = null;

/**
 * @constructor
 * @implements {RuntimeAgent.Dispatcher}
 * @param {WebInspector.JavaScriptContextManager} contextManager
 */
WebInspector.RuntimeDispatcher = function(contextManager)
{
    this._contextManager = contextManager;
}

WebInspector.RuntimeDispatcher.prototype = {
    isolatedContextCreated: function(context)
    {
        this._contextManager.isolatedContextCreated(context);
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.ExecutionContext = function(id, name, isPageContext)
{
    this.id = id;
    this.name = (isPageContext && !name) ? "<page context>" : name;
    this.isMainWorldContext = isPageContext;
}

/**
 * @param {*} a
 * @param {*} b
 * @return {number}
 */
WebInspector.ExecutionContext.comparator = function(a, b)
{
    // Main world context should always go first.
    if (a.isMainWorldContext)
        return -1;
    if (b.isMainWorldContext)
        return +1;
    return a.name.localeCompare(b.name);
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.FrameEvaluationContext = function(frame)
{
    this._frame = frame;
    this._mainWorldContext = null;
    this._isolatedContexts = [];
}

WebInspector.FrameEvaluationContext.EventTypes = {
    Updated: "Updated",
    AddedExecutionContext: "AddedExecutionContext"
}

WebInspector.FrameEvaluationContext.prototype =
{
    _frameNavigated: function(frame)
    {
        this._frame = frame;
        this._mainWorldContext = null;
        this._isolatedContexts = [];
        this.dispatchEventToListeners(WebInspector.FrameEvaluationContext.EventTypes.Updated, this);
    },

    /**
     * @param {WebInspector.ExecutionContext} context
     */
    _addExecutionContext: function(context)
    {
        if (context.isMainWorldContext)
            this._mainWorldContext = context;
        else {
            var insertAt = insertionIndexForObjectInListSortedByFunction(context, this._isolatedContexts, WebInspector.ExecutionContext.comparator);
            this._isolatedContexts.splice(insertAt, 0, context);
        }
        this.dispatchEventToListeners(WebInspector.FrameEvaluationContext.EventTypes.AddedExecutionContext, this);
    },

    mainWorldContext: function()
    {
        return this._mainWorldContext;
    },

    isolatedContexts: function()
    {
        return this._isolatedContexts;
    },

    /**
     * @param {string} securityOrigin
     */
    contextBySecurityOrigin: function(securityOrigin)
    {
        for (var i = 0; i < this._isolatedContexts.length; ++i) {
            var context = this._isolatedContexts[i];
            if (!context.isMainWorldContext && context.name === securityOrigin)
                return context; 
        }
    },

    get frameId()
    {
        return this._frame.id;
    },

    get url()
    {
        return this._frame.url;
    },

    get displayName()
    {
        if (!this._frame.parentFrame)
            return "<top frame>";
        var name = this._frame.name || "";
        var subtitle = new WebInspector.ParsedURL(this._frame.url).displayName;
        if (subtitle) {
            if (!name)
                return subtitle;
            return name + "( " + subtitle + " )";
        }
        return "<iframe>";
    }
}

WebInspector.FrameEvaluationContext.prototype.__proto__ = WebInspector.Object.prototype;
