/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ScopeChainSidebarPane = function()
{
    WebInspector.SidebarPane.call(this, WebInspector.UIString("Scope Variables"));
}

WebInspector.ScopeChainSidebarPane.prototype = {
    update: function(callFrame)
    {
        this.bodyElement.removeChildren();

        this.sections = [];
        this.callFrame = callFrame;

        if (!callFrame) {
            var infoElement = document.createElement("div");
            infoElement.className = "info";
            infoElement.textContent = WebInspector.UIString("Not Paused");
            this.bodyElement.appendChild(infoElement);
            return;
        }

        if (!callFrame._expandedProperties) {
            // FIXME: fix this when https://bugs.webkit.org/show_bug.cgi?id=19410 is fixed.
            // The callFrame is a JSInspectedObjectWrapper, so we are not allowed to assign
            // an object created in the Inspector's context to that object. So create an
            // Object from the inspectedWindow.
            var inspectedWindow = InspectorController.inspectedWindow();
            callFrame._expandedProperties = new inspectedWindow.Object;
        }

        var foundLocalScope = false;
        var scopeChain = callFrame.scopeChain;
        for (var i = 0; i < scopeChain.length; ++i) {
            var scopeObject = scopeChain[i];
            var title = null;
            var subtitle = Object.describe(scopeObject, true);
            var emptyPlaceholder = null;
            var localScope = false;
            var extraProperties = null;

            if (Object.prototype.toString.call(scopeObject) === "[object JSActivation]") {
                if (!foundLocalScope) {
                    extraProperties = { "this": callFrame.thisObject };
                    title = WebInspector.UIString("Local");
                } else
                    title = WebInspector.UIString("Closure");
                emptyPlaceholder = WebInspector.UIString("No Variables");
                subtitle = null;
                foundLocalScope = true;
                localScope = true;
            } else if (i === (scopeChain.length - 1))
                title = WebInspector.UIString("Global");
            else if (foundLocalScope && scopeObject instanceof InspectorController.inspectedWindow().Element)
                title = WebInspector.UIString("Event Target");
            else if (foundLocalScope && scopeObject instanceof InspectorController.inspectedWindow().Document)
                title = WebInspector.UIString("Event Document");
            else if (!foundLocalScope && !localScope)
                title = WebInspector.UIString("With Block");

            if (!title || title === subtitle)
                subtitle = null;

            var section = new WebInspector.ObjectPropertiesSection(scopeObject, title, subtitle, emptyPlaceholder, true, extraProperties, WebInspector.ScopeVariableTreeElement);
            section.editInSelectedCallFrameWhenPaused = true;
            section.pane = this;

            if (!foundLocalScope || localScope)
                section.expanded = true;

            this.sections.push(section);
            this.bodyElement.appendChild(section.element);
        }
    }
}

WebInspector.ScopeChainSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;

WebInspector.ScopeVariableTreeElement = function(parentObject, propertyName)
{
    WebInspector.ObjectPropertyTreeElement.call(this, parentObject, propertyName);
}

WebInspector.ScopeVariableTreeElement.prototype = {
    onattach: function()
    {
        WebInspector.ObjectPropertyTreeElement.prototype.onattach.call(this);
        if (this.hasChildren && this.propertyIdentifier in this.treeOutline.section.pane.callFrame._expandedProperties)
            this.expand();
    },

    onexpand: function()
    {
        this.treeOutline.section.pane.callFrame._expandedProperties[this.propertyIdentifier] = true;
    },

    oncollapse: function()
    {
        delete this.treeOutline.section.pane.callFrame._expandedProperties[this.propertyIdentifier];
    },

    get propertyIdentifier()
    {
        if ("_propertyIdentifier" in this)
            return this._propertyIdentifier;
        var section = this.treeOutline.section;
        this._propertyIdentifier = section.title + ":" + (section.subtitle ? section.subtitle + ":" : "") + this.propertyPath;
        return this._propertyIdentifier;
    },

    get propertyPath()
    {
        if ("_propertyPath" in this)
            return this._propertyPath;

        var current = this;
        var result;

        do {
            if (result)
                result = current.propertyName + "." + result;
            else
                result = current.propertyName;
            current = current.parent;
        } while (current && !current.root);

        this._propertyPath = result;
        return result;
    }
}

WebInspector.ScopeVariableTreeElement.prototype.__proto__ = WebInspector.ObjectPropertyTreeElement.prototype;
