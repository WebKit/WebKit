/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) IBM Corp. 2009  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @extends {WebInspector.View}
 * @constructor
 */
WebInspector.ResourceView = function(resource)
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("resourceView.css");

    this.element.addStyleClass("resource-view");
    this.resource = resource;
}

WebInspector.ResourceView.prototype = {
    hasContent: function()
    {
        return false;
    }
}

WebInspector.ResourceView.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @param {WebInspector.Resource} resource
 */
WebInspector.ResourceView.hasTextContent = function(resource)
{
    if (resource.type.isTextType())
        return true; 
    if (resource.type === WebInspector.resourceTypes.Other)
        return resource.content && !resource.contentEncoded;
    return false;
}

/**
 * @param {WebInspector.Resource} resource
 */
WebInspector.ResourceView.nonSourceViewForResource = function(resource)
{
    switch (resource.type) {
    case WebInspector.resourceTypes.Image:
        return new WebInspector.ImageView(resource);
    case WebInspector.resourceTypes.Font:
        return new WebInspector.FontView(resource);
    default:
        return new WebInspector.ResourceView(resource);
    }
}

/**
 * @extends {WebInspector.SourceFrame}
 * @constructor
 */
WebInspector.ResourceSourceFrame = function(resource)
{
    this._resource = resource;
    WebInspector.SourceFrame.call(this, resource);
    this._resource.addEventListener(WebInspector.Resource.Events.RevisionAdded, this._contentChanged, this);
}

WebInspector.ResourceSourceFrame.prototype = {
    get resource()
    {
        return this._resource;
    },

    _contentChanged: function(event)
    {
        this.setContent(this._resource.content, false, this._resource.canonicalMimeType());
    }
}

WebInspector.ResourceSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;

/**
 * @constructor
 * @extends {WebInspector.ResourceSourceFrame}
 */
WebInspector.EditableResourceSourceFrame = function(resource)
{
    WebInspector.ResourceSourceFrame.call(this, resource);
}

WebInspector.EditableResourceSourceFrame.Events = {
    TextEdited: "TextEdited"
}

WebInspector.EditableResourceSourceFrame.prototype = {
    /**
     * @return {boolean}
     */
    canEditSource: function()
    {
        //FIXME: make live edit stop using resource content binding.
        return this._resource.isEditable() && this._resource.type === WebInspector.resourceTypes.Stylesheet;
    },

    /**
     * @param {string} text
     */
    commitEditing: function(text)
    {
        this._clearIncrementalUpdateTimer();
        var majorChange = true;
        this._settingContent = true;
        function callbackWrapper(text)
        {
            delete this._settingContent;
            this.dispatchEventToListeners(WebInspector.EditableResourceSourceFrame.Events.TextEdited, this);
        }
        this.resource.setContent(text, majorChange, callbackWrapper.bind(this));
    },
    
    afterTextChanged: function(oldRange, newRange)
    {
        function commitIncrementalEdit()
        {
            var majorChange = false;
            this.resource.setContent(this._textModel.text, majorChange, function() {});
            this.dispatchEventToListeners(WebInspector.EditableResourceSourceFrame.Events.TextEdited, this);
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
        if (!this._settingContent)
            WebInspector.ResourceSourceFrame.prototype._contentChanged.call(this, event);
    },

    isDirty: function()
    {
        return this._resource.content !== this.textModel.text;
    }
}

WebInspector.EditableResourceSourceFrame.prototype.__proto__ = WebInspector.ResourceSourceFrame.prototype;

/**
 * @extends {WebInspector.ResourceSourceFrame}
 * @constructor
 */
WebInspector.ResourceRevisionSourceFrame = function(revision)
{
    WebInspector.ResourceSourceFrame.call(this, revision.resource);
    this._revision = revision;
}

WebInspector.ResourceRevisionSourceFrame.prototype = {
    get resource()
    {
        return this._revision.resource;
    },

    /**
     * @param {function(?string,boolean,string)} callback
     */
    requestContent: function(callback)
    {
        this._revision.requestContent(callback);
    },
}

WebInspector.ResourceRevisionSourceFrame.prototype.__proto__ = WebInspector.ResourceSourceFrame.prototype;
