/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.SidebarPanel = class SidebarPanel extends WebInspector.Object
{
    constructor(identifier, displayName, element, role, label)
    {
        super();

        this._identifier = identifier;

        this._savedScrollPosition = 0;

        this._element = element || document.createElement("div");
        this._element.classList.add("panel", identifier);

        this._element.setAttribute("role", role || "group");
        this._element.setAttribute("aria-label", label || displayName);

        this._contentElement = document.createElement("div");
        this._contentElement.className = "content";
        this._element.appendChild(this._contentElement);
    }

    // Public

    get identifier()
    {
        return this._identifier;
    }

    get element()
    {
        return this._element;
    }

    get contentElement()
    {
        return this._contentElement;
    }

    get visible()
    {
        return this.selected && this._parentSidebar && !this._parentSidebar.collapsed;
    }

    get selected()
    {
        return this._element.classList.contains(WebInspector.SidebarPanel.SelectedStyleClassName);
    }

    set selected(flag)
    {
        if (flag)
            this._element.classList.add(WebInspector.SidebarPanel.SelectedStyleClassName);
        else
            this._element.classList.remove(WebInspector.SidebarPanel.SelectedStyleClassName);
    }

    get parentSidebar()
    {
        return this._parentSidebar;
    }

    show()
    {
        if (!this._parentSidebar)
            return;

        this._parentSidebar.collapsed = false;
        this._parentSidebar.selectedSidebarPanel = this;
    }

    hide()
    {
        if (!this._parentSidebar)
            return;

        this._parentSidebar.collapsed = true;
        this._parentSidebar.selectedSidebarPanel = null;
    }

    toggle()
    {
        if (this.visible)
            this.hide();
        else
            this.show();
    }

    added()
    {
        console.assert(this._parentSidebar);

        // Implemented by subclasses.
    }

    removed()
    {
        console.assert(!this._parentSidebar);

        // Implemented by subclasses.
    }

    willRemove()
    {
        // Implemented by subclasses.
    }

    shown()
    {
        this._contentElement.scrollTop = this._savedScrollPosition;

        // Implemented by subclasses.
    }

    hidden()
    {
        this._savedScrollPosition = this._contentElement.scrollTop;

        // Implemented by subclasses.
    }

    widthDidChange()
    {
        // Implemented by subclasses.
    }

    visibilityDidChange()
    {
        // Implemented by subclasses.
    }
};

WebInspector.SidebarPanel.SelectedStyleClassName = "selected";
