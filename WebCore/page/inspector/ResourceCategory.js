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

WebInspector.ResourceCategory = function(title, name)
{
    this.name = name;
    this.title = title;
    this.resources = [];
    this.listItem = new WebInspector.ResourceCategoryTreeElement(this);
    this.listItem.hidden = true;
    WebInspector.fileOutline.appendChild(this.listItem);
}

WebInspector.ResourceCategory.prototype = {
    toString: function()
    {
        return this.title;
    },

    addResource: function(resource)
    {
        var a = resource;
        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var b = this.resources[i];
            if (a._lastPathComponentLowerCase && b._lastPathComponentLowerCase)
                if (a._lastPathComponentLowerCase < b._lastPathComponentLowerCase)
                    break;
            else if (a.name && b.name)
                if (a.name < b.name)
                    break;
        }

        this.resources.splice(i, 0, resource);
        this.listItem.insertChild(resource.listItem, i);
        this.listItem.hidden = false;

        resource.attach();
    },

    removeResource: function(resource)
    {
        resource.detach();

        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            if (this.resources[i] === resource) {
                this.resources.splice(i, 1);
                break;
            }
        }

        this.listItem.removeChild(resource.listItem);

        if (!this.resources.length)
            this.listItem.hidden = true;
    },

    removeAllResources: function(resource)
    {
        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i)
            this.resources[i].detach();
        this.resources = [];
        this.listItem.removeChildren();
        this.listItem.hidden = true;
    }
}

WebInspector.ResourceCategoryTreeElement = function(category)
{
    TreeElement.call(this, category.title, category, true);
}

WebInspector.ResourceCategoryTreeElement.prototype = {
    selectable: false,
    arrowToggleWidth: 20
}

WebInspector.ResourceCategoryTreeElement.prototype.__proto__ = TreeElement.prototype;
