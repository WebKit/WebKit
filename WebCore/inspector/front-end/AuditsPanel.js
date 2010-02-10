/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

WebInspector.AuditsPanel = function()
{
    WebInspector.Panel.call(this);

    this._constructCategories();

    this.createSidebar();
    this.auditsTreeElement = new WebInspector.SidebarSectionTreeElement("", {}, true);
    this.sidebarTree.appendChild(this.auditsTreeElement);
    this.auditsTreeElement.expand();

    this.auditsItemTreeElement = new WebInspector.AuditsSidebarTreeElement();
    this.auditsTreeElement.appendChild(this.auditsItemTreeElement);

    this.auditResultsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RESULTS"), {}, true);
    this.sidebarTree.appendChild(this.auditResultsTreeElement);
    this.auditResultsTreeElement.expand();

    this.element.addStyleClass("audits");

    this.clearResultsButton = new WebInspector.StatusBarButton(WebInspector.UIString("Clear audit results."), "clear-audit-results-status-bar-item");
    this.clearResultsButton.addEventListener("click", this._clearButtonClicked.bind(this), false);

    this.viewsContainerElement = document.createElement("div");
    this.viewsContainerElement.id = "audit-views";
    this.element.appendChild(this.viewsContainerElement);
}

WebInspector.AuditsPanel.prototype = {
    toolbarItemClass: "audits",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Audits");
    },

    get statusBarItems()
    {
        return [this.clearResultsButton.element];
    },

    get mainResourceLoadTime()
    {
        return this._mainResourceLoadTime;
    },

    set mainResourceLoadTime(x)
    {
        this._mainResourceLoadTime = x;
        this._didMainResourceLoad();
    },

    get mainResourceDOMContentTime()
    {
        return this._mainResourceDOMContentTime;
    },

    set mainResourceDOMContentTime(x)
    {
        this._mainResourceDOMContentTime = x;
    },

    get categoriesById()
    {
        return this._auditCategoriesById;
    },

    get visibleView()
    {
        return this._visibleView;
    },

    _constructCategories: function()
    {
        this._auditCategoriesById = {};
        for (var categoryCtorID in WebInspector.AuditCategories) {
            var auditCategory = new WebInspector.AuditCategories[categoryCtorID]();
            auditCategory._id = categoryCtorID;
            this.categoriesById[categoryCtorID] = auditCategory;
        }
    },

    _executeAudit: function(categories, resultCallback)
    {
        var resources = [];
        for (var id in WebInspector.resources)
            resources.push(WebInspector.resources[id]);

        var rulesRemaining = 0;
        for (var i = 0; i < categories.length; ++i)
            rulesRemaining += categories[i].ruleCount;

        var results = [];
        var mainResourceURL = WebInspector.mainResource.url;

        function ruleResultReadyCallback(categoryResult, ruleResult)
        {
            if (ruleResult.children)
                categoryResult.entries.push(ruleResult);

            --rulesRemaining;

            if (!rulesRemaining && resultCallback)
                resultCallback(mainResourceURL, results);
        }

        if (!rulesRemaining) {
            resultCallback(mainResourceURL, results);
            return;
        }

        for (var i = 0; i < categories.length; ++i) {
            var category = categories[i];
            var result = new WebInspector.AuditCategoryResult(category);
            results.push(result);
            category.runRules(resources, ruleResultReadyCallback.bind(null, result));
        }
    },

    _auditFinishedCallback: function(launcherCallback, mainResourceURL, results)
    {
        var children = this.auditResultsTreeElement.children;
        var ordinal = 1;
        for (var i = 0; i < children.length; ++i) {
            if (children[i].mainResourceURL === mainResourceURL)
                ordinal++;
        }

        var resultTreeElement = new WebInspector.AuditResultSidebarTreeElement(results, mainResourceURL, ordinal);
        this.auditResultsTreeElement.appendChild(resultTreeElement);
        resultTreeElement.reveal();
        resultTreeElement.select();
        if (launcherCallback)
            launcherCallback();
    },

    initiateAudit: function(categoryIds, runImmediately, launcherCallback)
    {
        if (!categoryIds || !categoryIds.length)
            return;

        var categories = [];
        for (var i = 0; i < categoryIds.length; ++i)
            categories.push(this.categoriesById[categoryIds[i]]);

        function initiateAuditCallback(categories, launcherCallback)
        {
            this._executeAudit(categories, this._auditFinishedCallback.bind(this, launcherCallback));
        }

        if (runImmediately)
            initiateAuditCallback.call(this, categories, launcherCallback);
        else
            this._reloadResources(initiateAuditCallback.bind(this, categories, launcherCallback));
    },

    _reloadResources: function(callback)
    {
        this._resourceTrackingCallback = callback;

        if (!InspectorBackend.resourceTrackingEnabled()) {
            InspectorBackend.enableResourceTracking(false);
            this._updateLauncherViewControls(true);
        } else
            InjectedScriptAccess.getDefault().evaluate("window.location.reload()", switchCallback);
    },

    _didMainResourceLoad: function()
    {
        if (this._resourceTrackingCallback) {
            var callback = this._resourceTrackingCallback;
            this._resourceTrackingCallback = null;
            callback();
        }
    },

    showResults: function(categoryResults)
    {
        if (!categoryResults._resultView)
            categoryResults._resultView = new WebInspector.AuditResultView(categoryResults);

        this.showView(categoryResults._resultView);
    },

    showLauncherView: function()
    {
        if (!this._launcherView)
            this._launcherView = new WebInspector.AuditLauncherView(this.categoriesById, this.initiateAudit.bind(this));

        this.showView(this._launcherView);
    },

    showView: function(view)
    {
        if (view) {
            if (this._visibleView === view)
                return;
            this._closeVisibleView();
            this._visibleView = view;
        }
        var visibleView = this.visibleView;
        if (visibleView)
            visibleView.show(this.viewsContainerElement);
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);

        this.showView();
        this._updateLauncherViewControls(InspectorBackend.resourceTrackingEnabled());
    },

    attach: function()
    {
        WebInspector.Panel.prototype.attach.call(this);

        this.auditsItemTreeElement.select();
    },

    updateMainViewWidth: function(width)
    {
        this.viewsContainerElement.style.left = width + "px";
    },

    _updateLauncherViewControls: function(isTracking)
    {
        if (this._launcherView)
            this._launcherView.updateResourceTrackingState(isTracking);
    },

    _clearButtonClicked: function()
    {
        this.auditsItemTreeElement.reveal();
        this.auditsItemTreeElement.select();
        this.auditResultsTreeElement.removeChildren();
    },

    _closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.hide();
    }
}

WebInspector.AuditsPanel.prototype.__proto__ = WebInspector.Panel.prototype;



WebInspector.AuditCategory = function(displayName)
{
    this._displayName = displayName;
    this._rules = [];
}

WebInspector.AuditCategory.prototype = {
    get id()
    {
        // this._id value is injected at construction time.
        return this._id;
    },

    get displayName()
    {
        return this._displayName;
    },

    get ruleCount()
    {
        this._ensureInitialized();
        return this._rules.length;
    },

    addRule: function(rule)
    {
        this._rules.push(rule);
    },

    runRules: function(resources, callback)
    {
        this._ensureInitialized();
        for (var i = 0; i < this._rules.length; ++i)
            this._rules[i].run(resources, callback);
    },

    _ensureInitialized: function()
    {
        if (!this._initialized) {
            if ("initialize" in this)
                this.initialize();
            this._initialized = true;
        }
    }
}


WebInspector.AuditRule = function(id, displayName, parametersObject)
{
    this._id = id;
    this._displayName = displayName;
    this._parametersObject = parametersObject;
}

WebInspector.AuditRule.prototype = {
    get id()
    {
        return this._id;
    },

    get displayName()
    {
        return this._displayName;
    },

    run: function(resources, callback)
    {
        this.doRun(resources, new WebInspector.AuditRuleResult(this.displayName), callback);
    },

    doRun: function(resources, result, callback)
    {
        throw new Error("doRun() not implemented");
    },

    getValue: function(key)
    {
        if (key in this._parametersObject)
            return this._parametersObject[key];
        else
            throw new Error(key + " not found in rule parameters");
    }
}


WebInspector.AuditCategoryResult = function(category)
{
    this.title = category.displayName;
    this.entries = [];
}

WebInspector.AuditCategoryResult.prototype = {
    addEntry: function(value)
    {
        var entry = new WebInspector.AuditRuleResult(value);
        this.entries.push(entry);
        return entry;
    }
}

/**
 * @param {string} value The result message HTML contents.
 */
WebInspector.AuditRuleResult = function(value)
{
    this.value = value;
    this.type = WebInspector.AuditRuleResult.Type.NA;
}

WebInspector.AuditRuleResult.Type = {
    // Does not denote a discovered flaw but rather represents an informational message.
    NA: 0,

    // Denotes a minor impact on the checked metric.
    Hint: 1,

    // Denotes a major impact on the checked metric.
    Violation: 2
}

WebInspector.AuditRuleResult.prototype = {
    appendChild: function(value)
    {
        if (!this.children)
            this.children = [];
        var entry = new WebInspector.AuditRuleResult(value);
        this.children.push(entry);
        return entry;
    },

    set type(x)
    {
        this._type = x;
    },

    get type()
    {
        return this._type;
    }
}


WebInspector.AuditsSidebarTreeElement = function()
{
    this.small = false;

    WebInspector.SidebarTreeElement.call(this, "audits-sidebar-tree-item", WebInspector.UIString("Audits"), "", null, false);
}

WebInspector.AuditsSidebarTreeElement.prototype = {
    onattach: function()
    {
        WebInspector.SidebarTreeElement.prototype.onattach.call(this);
    },

    onselect: function()
    {
        WebInspector.panels.audits.showLauncherView();
    },

    get selectable()
    {
        return true;
    },

    refresh: function()
    {
        this.refreshTitles();
    }
}

WebInspector.AuditsSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;


WebInspector.AuditResultSidebarTreeElement = function(results, mainResourceURL, ordinal)
{
    this.results = results;
    this.mainResourceURL = mainResourceURL;

    WebInspector.SidebarTreeElement.call(this, "audit-result-sidebar-tree-item", String.sprintf("%s (%d)", mainResourceURL, ordinal), "", {}, false);
}

WebInspector.AuditResultSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.audits.showResults(this.results);
    },

    get selectable()
    {
        return true;
    }
}

WebInspector.AuditResultSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

// Contributed audit rules should go into this namespace.
WebInspector.AuditRules = {};

// Contributed audit categories should go into this namespace.
WebInspector.AuditCategories = {};
