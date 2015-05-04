/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DebuggerTabContentView = function(identifier)
{
    var tabBarItem = new WebInspector.TabBarItem("Images/Debugger.svg", WebInspector.UIString("Debugger"));
    var detailsSidebarPanels = [WebInspector.resourceDetailsSidebarPanel, WebInspector.scopeChainDetailsSidebarPanel, WebInspector.probeDetailsSidebarPanel];

    WebInspector.ContentBrowserTabContentView.call(this, identifier || "debugger", "debugger", tabBarItem, WebInspector.DebuggerSidebarPanel, detailsSidebarPanels);
};

WebInspector.DebuggerTabContentView.prototype = {
    constructor: WebInspector.DebuggerTabContentView,
    __proto__: WebInspector.ContentBrowserTabContentView.prototype,

    // Public

    get type()
    {
        return WebInspector.DebuggerTabContentView.Type;
    },

    canShowRepresentedObject: function(representedObject)
    {
        if (representedObject instanceof WebInspector.Script)
            return true;

        if (!(representedObject instanceof WebInspector.Resource))
            return false;

        return representedObject.type === WebInspector.Resource.Type.Document || representedObject.type === WebInspector.Resource.Type.Script;
    },

    showDetailsSidebarPanels: function()
    {
        WebInspector.ContentBrowserTabContentView.prototype.showDetailsSidebarPanels.call(this);

        if (!this._showScopeChainDetailsSidebarPanel || !WebInspector.scopeChainDetailsSidebarPanel.parentSidebar)
            return;

        WebInspector.scopeChainDetailsSidebarPanel.show();

        this._showScopeChainDetailsSidebarPanel = false;
    },

    showScopeChainDetailsSidebarPanel: function()
    {
        this._showScopeChainDetailsSidebarPanel = true;
    },

    revealAndSelectBreakpoint: function(breakpoint)
    {
        console.assert(breakpoint instanceof WebInspector.Breakpoint);

        var treeElement = this.navigationSidebarPanel.treeElementForRepresentedObject(breakpoint);
        if (treeElement)
            treeElement.revealAndSelect();
    }
};

WebInspector.DebuggerTabContentView.Type = "debugger";
