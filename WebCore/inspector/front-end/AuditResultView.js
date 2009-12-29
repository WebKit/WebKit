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

WebInspector.AuditResultView = function(categoryResults)
{
    WebInspector.View.call(this);

    this.element.id = "audit-result-view";

    function entrySortFunction(a, b)
    {
        var result = b.type - a.type;
        if (!result)
            result = (a.value || "").localeCompare(b.value || "");
        return result;
    }

    for (var i = 0; i < categoryResults.length; ++i) {
        var entries = categoryResults[i].entries;
        if (entries) {
            entries.sort(entrySortFunction);
            this.element.appendChild(new WebInspector.AuditCategoryResultPane(categoryResults[i]).element);
        }
    }
}

WebInspector.AuditResultView.prototype.__proto__ = WebInspector.View.prototype;


WebInspector.AuditCategoryResultPane = function(categoryResult)
{
    WebInspector.SidebarPane.call(this, categoryResult.title);
    this.expand();
    for (var i = 0; i < categoryResult.entries.length; ++i)
        this.bodyElement.appendChild(new WebInspector.AuditRuleResultPane(categoryResult.entries[i]).element);
}

WebInspector.AuditCategoryResultPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;


WebInspector.AuditRuleResultPane = function(ruleResult)
{
    WebInspector.SidebarPane.call(this, ruleResult.value);
    if (!ruleResult.children)
        return;

    this._decorateRuleResult(ruleResult);

    for (var i = 0; i < ruleResult.children.length; ++i) {
        var section = new WebInspector.AuditRuleResultChildSection(ruleResult.children[i]);
        this.bodyElement.appendChild(section.element);
    }
}

WebInspector.AuditRuleResultPane.prototype = {
    _decorateRuleResult: function(ruleResult)
    {
        if (ruleResult.type == WebInspector.AuditRuleResult.Type.NA)
            return;

        var scoreElement = document.createElement("img");
        scoreElement.className = "score";
        var className = (ruleResult.type == WebInspector.AuditRuleResult.Type.Violation) ? "red" : "green";
        scoreElement.addStyleClass(className);
        this.element.insertBefore(scoreElement, this.element.firstChild);
    }
}

WebInspector.AuditRuleResultPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;


WebInspector.AuditRuleResultChildSection = function(entry)
{
    WebInspector.Section.call(this, entry.value);
    var children = entry.children;
    this._hasChildren = !!children && children.length;
    if (!this._hasChildren)
        this.element.addStyleClass("blank-section");
    else {
        this.contentElement = document.createElement("div");
        this.contentElement.addStyleClass("section-content");
        for (var i = 0; i < children.length; ++i) {
            var paraElement = document.createElement("p");
            paraElement.innerHTML = children[i].value;
            this.contentElement.appendChild(paraElement);
        }
        this.contentElement.appendChild(paraElement);
        this.element.appendChild(this.contentElement);
    }
}

WebInspector.AuditRuleResultChildSection.prototype = {

    // title is considered pure HTML
    set title(x)
    {
        if (this._title === x)
            return;
        this._title = x;

        this.titleElement.innerHTML = x;
    },

    expand: function()
    {
        if (this._hasChildren)
            WebInspector.Section.prototype.expand.call(this);
    }
}

WebInspector.AuditRuleResultChildSection.prototype.__proto__ = WebInspector.Section.prototype;
