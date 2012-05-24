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
    WebInspector.UISourceCode.call(this, resource.url, resource, resource);
}

WebInspector.StyleSource.prototype = {
    /**
     * @param {function(?string)} callback
     */
    workingCopyCommitted: function(callback)
    {  
        this._resource.setContent(this.workingCopy(), true, callback);
    },

    workingCopyChanged: function()
    {  
        function commitIncrementalEdit()
        {
            this._resource.setContent(this.workingCopy(), false, function() {});
        }
        const updateTimeout = 200;
        this._incrementalUpdateTimer = setTimeout(commitIncrementalEdit.bind(this), updateTimeout);
    }
}

WebInspector.StyleSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;

/**
 * @constructor
 * @extends {WebInspector.UISourceCode}
 * @param {CSSAgent.StyleSheetId} styleSheetId
 * @param {string} url
 * @param {string} content
 */
WebInspector.InspectorStyleSource = function(styleSheetId, url, content)
{
    WebInspector.UISourceCode.call(this, "<inspector style>", null, new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Stylesheet, content));
}

WebInspector.InspectorStyleSource.prototype = {
}

WebInspector.InspectorStyleSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;

/**
 * @constructor
 * @extends {WebInspector.SourceFrame}
 * @param {WebInspector.StyleSource} styleSource
 */
WebInspector.StyleSourceFrame = function(styleSource)
{
    this._styleSource = styleSource;
    WebInspector.SourceFrame.call(this, this._styleSource);
    this._styleSource.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._onContentChanged, this);
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
     * @param {string} text
     */
    commitEditing: function(text)
    {
        if (!this._styleSource.isDirty())
            return;

        this._isCommittingEditing = true;
        this._styleSource.commitWorkingCopy(this._didEditContent.bind(this));
    },

    afterTextChanged: function(oldRange, newRange)
    {
        this._styleSource.setWorkingCopy(this.textModel.text);
    },

    _didEditContent: function(error)
    {
        if (error) {
            WebInspector.log(error, WebInspector.ConsoleMessage.MessageLevel.Error, true);
            return;
        }
        delete this._isCommittingEditing;
    },

    _clearIncrementalUpdateTimer: function()
    {
        if (this._incrementalUpdateTimer)
            clearTimeout(this._incrementalUpdateTimer);
        delete this._incrementalUpdateTimer;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onContentChanged: function(event)
    {
        if (!this._isCommittingEditing)
            this.setContent(this._styleSource.content() || "", false, "text/css");
    },

    populateTextAreaContextMenu: function(contextMenu, lineNumber)
    {
        WebInspector.SourceFrame.prototype.populateTextAreaContextMenu.call(this, contextMenu, lineNumber);
        var scriptsPanel = WebInspector.panels.scripts;
        contextMenu.appendApplicableItems(this._styleSource);
        contextMenu.appendSeparator();
    }
}

WebInspector.StyleSourceFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;

/**
 * @constructor
 * @implements {WebInspector.SelectionDialogContentProvider}
 * @param {WebInspector.View} view
 * @param {WebInspector.StyleSource} styleSource
 */
WebInspector.StyleSheetOutlineDialog = function(view, styleSource)
{
    WebInspector.SelectionDialogContentProvider.call(this);

    this._rules = [];
    this._view = view;
    this._styleSource = styleSource;
}

/**
 * @param {WebInspector.View} view
 * @param {WebInspector.StyleSource} styleSource
 */
WebInspector.StyleSheetOutlineDialog.show = function(view, styleSource)
{
    if (WebInspector.Dialog.currentInstance())
        return null;
    var delegate = new WebInspector.StyleSheetOutlineDialog(view, styleSource);
    var filteredItemSelectionDialog = new WebInspector.FilteredItemSelectionDialog(delegate);
    WebInspector.Dialog.show(view.element, filteredItemSelectionDialog);
}

WebInspector.StyleSheetOutlineDialog.prototype = {
    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemTitleAt: function(itemIndex)
    {
        return this._rules[itemIndex].selectorText;
    },

    /**
     * @param {number} itemIndex
     * @return {string}
     */
    itemKeyAt: function(itemIndex)
    {
        return this._rules[itemIndex].selectorText;
    },

    /**
     * @return {number}
     */
    itemsCount: function()
    {
        return this._rules.length;
    },

    /**
     * @param {function(number, number, number, number)} callback
     */
    requestItems: function(callback)
    {
        function didGetAllStyleSheets(error, infos)
        {
            if (error) {
                callback(0, 0, 0, 0);
                return;
            }
  
            for (var i = 0; i < infos.length; ++i) {
                var info = infos[i];
                if (info.sourceURL === this._styleSource.contentURL()) {
                    WebInspector.CSSStyleSheet.createForId(info.styleSheetId, didGetStyleSheet.bind(this));
                    return;
                }
            }
            callback(0, 0, 0, 0);
        }

        CSSAgent.getAllStyleSheets(didGetAllStyleSheets.bind(this));

        /**
         * @param {?WebInspector.CSSStyleSheet} styleSheet
         */
        function didGetStyleSheet(styleSheet)
        {
            if (!styleSheet) {
                callback(0, 0, 0, 0);
                return;
            }

            this._rules = styleSheet.rules;
            callback(0, this._rules.length, 0, 1);
        }
    },

    /**
     * @param {number} itemIndex
     */
    selectItem: function(itemIndex)
    {
        var lineNumber = this._rules[itemIndex].sourceLine;
        if (!isNaN(lineNumber) && lineNumber >= 0)
            this._view.highlightLine(lineNumber);
        this._view.focus();
    }
}

WebInspector.StyleSheetOutlineDialog.prototype.__proto__ = WebInspector.SelectionDialogContentProvider.prototype;
