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
 * @extends {WebInspector.View}
 * @constructor
 */
WebInspector.NavigatorView = function()
{
    WebInspector.View.call(this);
    this.registerRequiredCSS("navigatorView.css");

    this._treeSearchBoxElement = document.createElement("div");
    this._treeSearchBoxElement.className = "navigator-tree-search-box";
    this.element.appendChild(this._treeSearchBoxElement);

    var scriptsTreeElement = document.createElement("ol");
    this._scriptsTree = new WebInspector.NavigatorTreeOutline(this._treeSearchBoxElement, scriptsTreeElement);

    var scriptsOutlineElement = document.createElement("div");
    scriptsOutlineElement.addStyleClass("outline-disclosure");
    scriptsOutlineElement.addStyleClass("navigator");
    scriptsOutlineElement.appendChild(scriptsTreeElement);

    this.element.addStyleClass("fill");
    this.element.addStyleClass("navigator-container");
    this.element.appendChild(scriptsOutlineElement);
    this.setDefaultFocusedElement(this._scriptsTree.element);

    /** @type {Object.<string, WebInspector.NavigatorFolderTreeElement>} */
    this._folderTreeElements = {};
    /** @type {Object.<string, Array.<WebInspector.UISourceCode>>} */
    this._folderTreeElementsUISourceCodes = {};
    /** @type {Object.<string, WebInspector.UISourceCode>} */
    this._uiSourceCodes = {};
    /** @type {Object.<string, WebInspector.NavigatorSourceTreeElement>} */
    this._scriptTreeElements = {};
}

WebInspector.NavigatorView.Events = {
    ItemSelected: "ItemSelected",
    FileRenamed: "FileRenamed"
}

WebInspector.NavigatorView.prototype = {
    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    addUISourceCode: function(uiSourceCode)
    {
        if (this._uiSourceCodes[uiSourceCode.uri()])
            return;
        this._uiSourceCodes[uiSourceCode.uri()] = uiSourceCode;

        var folderTreeElement = this.getOrCreateFolderTreeElement(uiSourceCode);
        this._adoptUISourceCode(folderTreeElement, uiSourceCode);
    },

    _getOrCreateScriptTreeElement: function(uiSourceCode)
    {
        return this._getScriptTreeElement(uiSourceCode) || this._createScriptTreeElement(uiSourceCode);
    },

    _getScriptTreeElement: function(uiSourceCode)
    {
        return this._scriptTreeElements[uiSourceCode.uri()];
    },

    _createScriptTreeElement: function(uiSourceCode)
    {
        var scriptTreeElement = new WebInspector.NavigatorSourceTreeElement(this, uiSourceCode, "");
        this._scriptTreeElements[uiSourceCode.uri()] = scriptTreeElement;
        this._updateScriptTitle(uiSourceCode);
        this._addUISourceCodeListeners(uiSourceCode);
        return scriptTreeElement;
    },

    _removeScriptTreeElement: function(uiSourceCode)
    {
        var scriptTreeElement = this._scriptTreeElements[uiSourceCode.uri()];
        if (!scriptTreeElement)
            return;
        if (scriptTreeElement.parent)
            scriptTreeElement.parent.removeChild(scriptTreeElement);
        delete this._scriptTreeElements[uiSourceCode.uri()];
        this._removeUISourceCodeListeners(uiSourceCode);
    },

    _uiSourceCodeTitleChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ (event.target);
        this._updateScriptTitle(uiSourceCode)
    },

    _uiSourceCodeWorkingCopyChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ (event.target);
        this._updateScriptTitle(uiSourceCode)
    },

    _uiSourceCodeWorkingCopyCommitted: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ (event.target);
        this._updateScriptTitle(uiSourceCode)
    },

    _uiSourceCodeFormattedChanged: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ (event.target);
        this._updateScriptTitle(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {boolean=} ignoreIsDirty
     */
    _updateScriptTitle: function(uiSourceCode, ignoreIsDirty)
    {
        var scriptTreeElement = this._getScriptTreeElement(uiSourceCode);
        if (!scriptTreeElement)
            return;

        var titleText = uiSourceCode.name().trimEnd(100);
        if (!titleText)
            titleText = WebInspector.UIString("(program)");
        if (!ignoreIsDirty && uiSourceCode.isDirty())
            titleText = "*" + titleText;
        scriptTreeElement.titleText = titleText;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    revealUISourceCode: function(uiSourceCode)
    {
        if (this._scriptsTree.selectedTreeElement)
            this._scriptsTree.selectedTreeElement.deselect();

        this._lastSelectedUISourceCode = uiSourceCode;

        var folderTreeElement = this.getOrCreateFolderTreeElement(uiSourceCode);
        if (!folderTreeElement._populated && folderTreeElement.parent)
            folderTreeElement.onpopulate();
        var scriptTreeElement = this._getOrCreateScriptTreeElement(uiSourceCode);
        scriptTreeElement.revealAndSelect(true);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {boolean} focusSource
     */
    _scriptSelected: function(uiSourceCode, focusSource)
    {
        this._lastSelectedUISourceCode = uiSourceCode;
        var data = { uiSourceCode: uiSourceCode, focusSource: focusSource};
        this.dispatchEventToListeners(WebInspector.NavigatorView.Events.ItemSelected, data);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    removeUISourceCode: function(uiSourceCode)
    {
        this._removeScriptTreeElement(uiSourceCode);
        var treeElement = this.getOrCreateFolderTreeElement(uiSourceCode);
        while (treeElement) {
            var parent = treeElement.parent;
            if (parent) {
                delete this._folderTreeElements[treeElement.folderIdentifier];
                delete this._folderTreeElementsUISourceCodes[treeElement.folderIdentifier];
                parent.removeChild(treeElement);
                if (parent.children.length)
                    break;
            }
            treeElement = parent;
        }
        delete this._uiSourceCodes[uiSourceCode.uri()];
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _addUISourceCodeListeners: function(uiSourceCode)
    {
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.TitleChanged, this._uiSourceCodeTitleChanged, this);
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged, this._uiSourceCodeWorkingCopyChanged, this);
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted, this._uiSourceCodeWorkingCopyCommitted, this);
        uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.FormattedChanged, this._uiSourceCodeFormattedChanged, this);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _removeUISourceCodeListeners: function(uiSourceCode)
    {
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.TitleChanged, this._uiSourceCodeTitleChanged, this);
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged, this._uiSourceCodeWorkingCopyChanged, this);
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted, this._uiSourceCodeWorkingCopyCommitted, this);
        uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.FormattedChanged, this._uiSourceCodeFormattedChanged, this);
    },

    _fileRenamed: function(uiSourceCode, newTitle)
    {    
        var data = { uiSourceCode: uiSourceCode, name: newTitle };
        this.dispatchEventToListeners(WebInspector.NavigatorView.Events.FileRenamed, data);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {function(boolean)=} callback
     */
    rename: function(uiSourceCode, callback)
    {
        var scriptTreeElement = this._getScriptTreeElement(uiSourceCode);
        if (!scriptTreeElement)
            return;

        // Tree outline should be marked as edited as well as the tree element to prevent search from starting.
        var treeOutlineElement = scriptTreeElement.treeOutline.element;
        WebInspector.markBeingEdited(treeOutlineElement, true);

        function commitHandler(element, newTitle, oldTitle)
        {
            if (newTitle && newTitle !== oldTitle)
                this._fileRenamed(uiSourceCode, newTitle);
            afterEditing.call(this, true);
        }

        function cancelHandler()
        {
            afterEditing.call(this, false);
        }

        /**
         * @param {boolean} committed
         */
        function afterEditing(committed)
        {
            WebInspector.markBeingEdited(treeOutlineElement, false);
            this._updateScriptTitle(uiSourceCode);
            if (callback)
                callback(committed);
        }

        var editingConfig = new WebInspector.EditingConfig(commitHandler.bind(this), cancelHandler.bind(this));
        this._updateScriptTitle(uiSourceCode, true);
        WebInspector.startEditing(scriptTreeElement.titleElement, editingConfig);
        window.getSelection().setBaseAndExtent(scriptTreeElement.titleElement, 0, scriptTreeElement.titleElement, 1);
    },

    reset: function()
    {
        var uiSourceCodes = this._scriptsTree.scriptTreeElements;
        for (var i = 0; i < uiSourceCodes.length; ++i)
            this._removeUISourceCodeListeners(uiSourceCodes[i]);

        this._scriptsTree.stopSearch();
        this._scriptsTree.removeChildren();
        this._folderTreeElements = {};
        this._folderTreeElementsUISourceCodes = {};
        this._uiSourceCodes = {};
        this._scriptTreeElements = {};
    },

    /**
     * @param {string} folderIdentifier
     * @param {string} domain
     * @param {string} folderName
     */
    createFolderTreeElement: function(parentFolderElement, folderIdentifier, domain, folderName)
    {
        var folderTreeElement = new WebInspector.NavigatorFolderTreeElement(this, folderIdentifier, domain, folderName);
        this._folderTreeElements[folderIdentifier] = folderTreeElement;
        this._folderTreeElementsUISourceCodes[folderIdentifier] = [];
        parentFolderElement.appendChild(folderTreeElement);
        return folderTreeElement;
    },

    /**
     * @param {WebInspector.NavigatorFolderTreeElement} folderTreeElement
     */
    _populateFolderTreeElement: function(folderTreeElement)
    {
        if (folderTreeElement._populated)
            return;
        folderTreeElement._populated = true;
        var uiSourceCodes = this._folderTreeElementsUISourceCodes[folderTreeElement.folderIdentifier];
        for (var i = 0; i < uiSourceCodes.length; ++i)
            this._addScriptTreeElement(folderTreeElement, uiSourceCodes[i]);
        delete this._folderTreeElementsUISourceCodes[folderTreeElement.folderIdentifier];
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    getOrCreateFolderTreeElement: function(uiSourceCode)
    {
        var folders = uiSourceCode.path().slice();
        folders.pop();
        return this._getOrCreateFolderTreeElement(uiSourceCode.project().displayName(), folders.join("/"));
    },

    /**
     * @param {string} domain
     * @param {string} folderName
     */
    _getOrCreateFolderTreeElement: function(domain, folderName)
    {
        var folderIdentifier = domain + "/" + folderName;
        
        if (this._folderTreeElements[folderIdentifier])
            return this._folderTreeElements[folderIdentifier];

        if (!domain && !folderName)
            return this._scriptsTree;

        var parentFolderElement;
        if (!folderName)
            parentFolderElement = this._scriptsTree;
        else
            parentFolderElement = this._getOrCreateFolderTreeElement(domain, "");
        
        return this.createFolderTreeElement(parentFolderElement, folderIdentifier, domain, folderName);
    },

    _addScriptTreeElement: function(folderTreeElement, uiSourceCode)
    {
        var scriptTreeElement = this._getOrCreateScriptTreeElement(uiSourceCode);
        folderTreeElement.appendChild(scriptTreeElement);
    },

    _adoptUISourceCode: function(folderTreeElement, uiSourceCode)
    {
        if (folderTreeElement._populated || !folderTreeElement.parent) {
            this._addScriptTreeElement(folderTreeElement, uiSourceCode);
            return;
        }
        this._folderTreeElementsUISourceCodes[folderTreeElement.folderIdentifier].push(uiSourceCode);
    },

    handleContextMenu: function(event, uiSourceCode)
    {
        var contextMenu = new WebInspector.ContextMenu(event);
        contextMenu.appendApplicableItems(uiSourceCode);
        contextMenu.show();
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @constructor
 * @extends {TreeOutline}
 * @param {Element} treeSearchBoxElement
 * @param {Element} element
 */
WebInspector.NavigatorTreeOutline = function(treeSearchBoxElement, element)
{
    TreeOutline.call(this, element);
    this.element = element;

    this._treeSearchBoxElement = treeSearchBoxElement;
    
    this.comparator = WebInspector.NavigatorTreeOutline._treeElementsCompare;

    this.searchable = true;
    this.searchInputElement = document.createElement("input");
}

WebInspector.NavigatorTreeOutline._treeElementsCompare = function compare(treeElement1, treeElement2)
{
    // Insert in the alphabetical order, first domains, then folders, then scripts.
    function typeWeight(treeElement)
    {
        if (treeElement instanceof WebInspector.NavigatorFolderTreeElement) {
            if (treeElement.isDomain) {
                if (treeElement.titleText === WebInspector.inspectedPageDomain)
                    return 1;
                return 2;
            }
            return 3;
        }
        return 4;
    }

    var typeWeight1 = typeWeight(treeElement1);
    var typeWeight2 = typeWeight(treeElement2);

    var result;
    if (typeWeight1 > typeWeight2)
        result = 1;
    else if (typeWeight1 < typeWeight2)
        result = -1;
    else {
        var title1 = treeElement1.titleText;
        var title2 = treeElement2.titleText;
        result = title1.compareTo(title2);
    }
    return result;
}

WebInspector.NavigatorTreeOutline.prototype = {
   /**
    * @return {Array.<WebInspector.UISourceCode>}
    */
   scriptTreeElements: function()
   {
       var result = [];
       if (this.children.length) {
           for (var treeElement = this.children[0]; treeElement; treeElement = treeElement.traverseNextTreeElement(false, this, true)) {
               if (treeElement instanceof WebInspector.NavigatorSourceTreeElement)
                   result.push(treeElement.uiSourceCode);
           }
       }
       return result;
   },

   searchStarted: function()
   {
       this._treeSearchBoxElement.appendChild(this.searchInputElement);
       this._treeSearchBoxElement.addStyleClass("visible");
   },

   searchFinished: function()
   {
       this._treeSearchBoxElement.removeChild(this.searchInputElement);
       this._treeSearchBoxElement.removeStyleClass("visible");
   },

    __proto__: TreeOutline.prototype
}

/**
 * @constructor
 * @extends {TreeElement}
 * @param {string} title
 * @param {Array.<string>} iconClasses
 * @param {boolean} hasChildren
 * @param {boolean=} noIcon
 */
WebInspector.BaseNavigatorTreeElement = function(title, iconClasses, hasChildren, noIcon)
{
    TreeElement.call(this, "", null, hasChildren);
    this._titleText = title;
    this._iconClasses = iconClasses;
    this._noIcon = noIcon;
}

WebInspector.BaseNavigatorTreeElement.prototype = {
    onattach: function()
    {
        this.listItemElement.removeChildren();
        if (this._iconClasses) {
            for (var i = 0; i < this._iconClasses.length; ++i)
                this.listItemElement.addStyleClass(this._iconClasses[i]);
        }

        var selectionElement = document.createElement("div");
        selectionElement.className = "selection";
        this.listItemElement.appendChild(selectionElement);

        if (!this._noIcon) {
            this.imageElement = document.createElement("img");
            this.imageElement.className = "icon";
            this.listItemElement.appendChild(this.imageElement);
        }
        
        this.titleElement = document.createElement("div");
        this.titleElement.className = "base-navigator-tree-element-title";
        this._titleTextNode = document.createTextNode("");
        this._titleTextNode.textContent = this._titleText;
        this.titleElement.appendChild(this._titleTextNode);
        this.listItemElement.appendChild(this.titleElement);
    },

    onreveal: function()
    {
        if (this.listItemElement)
            this.listItemElement.scrollIntoViewIfNeeded(true);
    },

    /**
     * @return {string}
     */
    get titleText()
    {
        return this._titleText;
    },

    set titleText(titleText)
    {
        if (this._titleText === titleText)
            return;
        this._titleText = titleText || "";
        if (this.titleElement)
            this.titleElement.textContent = this._titleText;
    },
    
    /**
     * @param {string} searchText
     */
    matchesSearchText: function(searchText)
    {
        return this.titleText.match(new RegExp("^" + searchText.escapeForRegExp(), "i"));
    },

    __proto__: TreeElement.prototype
}

/**
 * @constructor
 * @extends {WebInspector.BaseNavigatorTreeElement}
 * @param {WebInspector.NavigatorView} navigatorView
 * @param {string} folderIdentifier
 * @param {string} domain
 * @param {string} folderName
 */
WebInspector.NavigatorFolderTreeElement = function(navigatorView, folderIdentifier, domain, folderName)
{
    this._navigatorView = navigatorView;
    this._folderIdentifier = folderIdentifier;
    this._folderName = folderName;
    
    var iconClass = this.isDomain ? "navigator-domain-tree-item" : "navigator-folder-tree-item";
    var title = this.isDomain ? domain : folderName;

    WebInspector.BaseNavigatorTreeElement.call(this, title, [iconClass], true);
    this.tooltip = folderName;
}

WebInspector.NavigatorFolderTreeElement.prototype = {
    /**
     * @return {string}
     */
    get folderIdentifier()
    {
        return this._folderIdentifier;
    },

    /**
     * @return {boolean}
     */
    get isDomain()
    {
        return this._folderName === "";
    },
    
    onpopulate: function()
    {
        this._navigatorView._populateFolderTreeElement(this);
    },

    onattach: function()
    {
        WebInspector.BaseNavigatorTreeElement.prototype.onattach.call(this);
        if (this.isDomain && this.titleText === WebInspector.inspectedPageDomain)
            this.expand();
        else
            this.collapse();
    },

    __proto__: WebInspector.BaseNavigatorTreeElement.prototype
}

/**
 * @constructor
 * @extends {WebInspector.BaseNavigatorTreeElement}
 * @param {WebInspector.NavigatorView} navigatorView
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {string} title
 */
WebInspector.NavigatorSourceTreeElement = function(navigatorView, uiSourceCode, title)
{
    WebInspector.BaseNavigatorTreeElement.call(this, title, ["navigator-" + uiSourceCode.contentType().name() + "-tree-item"], false);
    this._navigatorView = navigatorView;
    this._uiSourceCode = uiSourceCode;
    this.tooltip = uiSourceCode.originURL();
}

WebInspector.NavigatorSourceTreeElement.prototype = {
    /**
     * @return {WebInspector.UISourceCode}
     */
    get uiSourceCode()
    {
        return this._uiSourceCode;
    },

    onattach: function()
    {
        WebInspector.BaseNavigatorTreeElement.prototype.onattach.call(this);
        this.listItemElement.draggable = true;
        this.listItemElement.addEventListener("click", this._onclick.bind(this), false);
        this.listItemElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), false);
        this.listItemElement.addEventListener("mousedown", this._onmousedown.bind(this), false);
        this.listItemElement.addEventListener("dragstart", this._ondragstart.bind(this), false);
    },

    _onmousedown: function(event)
    {
        if (event.which === 1) // Warm-up data for drag'n'drop
            this._uiSourceCode.requestContent(callback.bind(this));
        /**
         * @param {?string} content
         * @param {boolean} contentEncoded
         * @param {string} mimeType
         */
        function callback(content, contentEncoded, mimeType)
        {
            this._warmedUpContent = content;
        }
    },

    _ondragstart: function(event)
    {
        event.dataTransfer.setData("text/plain", this._warmedUpContent);
        event.dataTransfer.effectAllowed = "copy";
        return true;
    },

    onspace: function()
    {
        this._navigatorView._scriptSelected(this.uiSourceCode, true);
        return true;
    },

    /**
     * @param {Event} event
     */
    _onclick: function(event)
    {
        this._navigatorView._scriptSelected(this.uiSourceCode, false);
    },

    /**
     * @param {Event} event
     */
    ondblclick: function(event)
    {
        var middleClick = event.button === 1;
        this._navigatorView._scriptSelected(this.uiSourceCode, !middleClick);
    },

    onenter: function()
    {
        this._navigatorView._scriptSelected(this.uiSourceCode, true);
        return true;
    },

    /**
     * @param {Event} event
     */
    _handleContextMenuEvent: function(event)
    {
        this._navigatorView.handleContextMenu(event, this._uiSourceCode);
    },

    __proto__: WebInspector.BaseNavigatorTreeElement.prototype
}
