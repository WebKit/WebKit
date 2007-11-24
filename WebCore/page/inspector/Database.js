/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

WebInspector.Database = function(database, domain, name, version)
{
    this.database = database;
    this.domain = domain;
    this.name = name;
    this.version = version;

    this.listItem = new WebInspector.ResourceTreeElement(this);
    this.updateTitle();

    this.category.addResource(this);
}

WebInspector.Database.prototype = {
    get database()
    {
        return this._database;
    },

    set database(x)
    {
        if (this._database === x)
            return;
        this._database = x;
    },

    get name()
    {
        return this._name;
    },

    set name(x)
    {
        if (this._name === x)
            return;
        this._name = x;
        this.updateTitleSoon();
    },

    get version()
    {
        return this._version;
    },

    set version(x)
    {
        if (this._version === x)
            return;
        this._version = x;
    },

    get domain()
    {
        return this._domain;
    },

    set domain(x)
    {
        if (this._domain === x)
            return;
        this._domain = x;
        this.updateTitleSoon();
    },

    get category()
    {
        return WebInspector.resourceCategories.databases;
    },

    updateTitle: function()
    {
        delete this.updateTitleTimeout;

        var title = this.name;

        var info = "";
        if (this.domain && (!WebInspector.mainResource || (WebInspector.mainResource && this.domain !== WebInspector.mainResource.domain)))
            info = this.domain;

        var fullTitle = "<span class=\"title" + (info && info.length ? "" : " only") + "\">" + title.escapeHTML() + "</span>";
        if (info && info.length)
            fullTitle += "<span class=\"info\">" + info.escapeHTML() + "</span>";
        fullTitle += "<div class=\"icon database\"></div>";

        this.listItem.title = fullTitle;
    },

    get panel()
    {
        if (!this._panel)
            this._panel = new WebInspector.DatabasePanel(this);
        return this._panel;
    },

    // Inherit the other functions from the Resource prototype.
    updateTitleSoon: WebInspector.Resource.prototype.updateTitleSoon,
    select: WebInspector.Resource.prototype.select,
    deselect: WebInspector.Resource.prototype.deselect,
    attach: WebInspector.Resource.prototype.attach,
    detach: WebInspector.Resource.prototype.detach
}
