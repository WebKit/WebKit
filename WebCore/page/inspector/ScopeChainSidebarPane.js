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

        if (!callFrame) {
            var infoElement = document.createElement("div");
            infoElement.className = "info";
            infoElement.textContent = WebInspector.UIString("Not Paused");
            this.bodyElement.appendChild(infoElement);
            return;
        }

        var foundLocalScope = false;
        var scopeChain = callFrame.scopeChain;
        for (var i = 0; i < scopeChain.length; ++i) {
            var scopeObject = scopeChain[i];
            var title = null;
            var subtitle = Object.describe(scopeObject, true);
            var emptyPlaceholder = null;
            var localScope = false;

            if (Object.prototype.toString.call(scopeObject) === "[object Activation]") {
                if (!foundLocalScope)
                    title = WebInspector.UIString("Local");
                else
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

            var section = new WebInspector.ObjectPropertiesSection(scopeObject, title, subtitle, emptyPlaceholder);
            if (!foundLocalScope || localScope)
                section.expanded = true;

            this.sections.push(section);
            this.bodyElement.appendChild(section.element);
        }
    }
}

WebInspector.ScopeChainSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;
