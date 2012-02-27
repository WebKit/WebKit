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
WebInspector.TimelinePresentationModel = function()
{
    this._categories = {};
    this.reset();
}

WebInspector.TimelinePresentationModel.Events = {
    WindowChanged: "WindowChanged",
    CategoryVisibilityChanged: "CategoryVisibilityChanged"
}

WebInspector.TimelinePresentationModel.prototype = {
    reset: function()
    {
        this.windowLeft = 0.0;
        this.windowRight = 1.0;
        this.windowIndexLeft = 0;
        this.windowIndexRight = null;
    },

    get categories()
    {
        return this._categories;
    },

    /**
     * @param {WebInspector.TimelineCategory} category
     */
    addCategory: function(category)
    {
        this._categories[category.name] = category;
    },

    /**
     * @param {number} left
     * @param {number} right
     */
    setWindowPosition: function(left, right)
    {
        this.windowLeft = left;
        this.windowRight = right;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.WindowChanged);
    },

    /**
     * @param {number} left
     * @param {?number} right
     */
    setWindowIndices: function(left, right)
    {
        this.windowIndexLeft = left;
        this.windowIndexRight = right;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.WindowChanged);
    },

    /**
     * @param {WebInspector.TimelineCategory} category
     * @param {boolean} visible
     */
    setCategoryVisibility: function(category, visible)
    {
        category.hidden = !visible;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.CategoryVisibilityChanged, category);
    }
}

WebInspector.TimelinePresentationModel.prototype.__proto__ = WebInspector.Object.prototype;
