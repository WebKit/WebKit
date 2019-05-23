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

WI.StyleDetailsPanel = class StyleDetailsPanel extends WI.View
{
    constructor(delegate, className, identifier, label)
    {
        super();

        this._delegate = delegate || null;

        this.element.classList.add(className);

        this._navigationInfo = {identifier, label};

        this._nodeStyles = null;
        this._visible = false;
    }

    // Public

    get navigationInfo()
    {
        return this._navigationInfo;
    }

    get nodeStyles()
    {
        return this._nodeStyles;
    }

    shown()
    {
        if (this._visible)
            return;

        this._visible = true;

        this._refreshNodeStyles();

        // FIXME: remove once <https://webkit.org/b/150741> is fixed.
        this.updateLayoutIfNeeded();
    }

    hidden()
    {
        this._visible = false;

        this.cancelLayout();
    }

    markAsNeedsRefresh(domNode)
    {
        console.assert(domNode);
        if (!domNode)
            return;

        if (!this._nodeStyles || this._nodeStyles.node !== domNode) {
            if (this._nodeStyles) {
                this._nodeStyles.removeEventListener(WI.DOMNodeStyles.Event.Refreshed, this.nodeStylesRefreshed, this);
                this._nodeStyles.removeEventListener(WI.DOMNodeStyles.Event.NeedsRefresh, this._nodeStylesNeedsRefreshed, this);
            }

            this._nodeStyles = WI.cssManager.stylesForNode(domNode);

            console.assert(this._nodeStyles);
            if (!this._nodeStyles)
                return;

            this._nodeStyles.addEventListener(WI.DOMNodeStyles.Event.Refreshed, this.nodeStylesRefreshed, this);
            this._nodeStyles.addEventListener(WI.DOMNodeStyles.Event.NeedsRefresh, this._nodeStylesNeedsRefreshed, this);

            this._forceSignificantChange = true;
        }

        if (this._visible)
            this._refreshNodeStyles();
    }

    refresh(significantChange)
    {
        // Implemented by subclasses.
        this.dispatchEventToListeners(WI.StyleDetailsPanel.Event.Refreshed);
    }

    // Protected

    nodeStylesRefreshed(event)
    {
        if (this._visible)
            this._refreshPreservingScrollPosition(event.data.significantChange);
    }

    filterDidChange(filterBar)
    {
        // Implemented by subclasses.
    }

    // Private

    get _initialScrollOffset()
    {
        if (!WI.cssManager.canForcePseudoClasses())
            return 0;
        return this.nodeStyles.node.enabledPseudoClasses.length ? 0 : WI.GeneralStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset;
    }

    _refreshNodeStyles()
    {
        if (!this._nodeStyles)
            return;
        this._nodeStyles.refresh();
    }

    _refreshPreservingScrollPosition(significantChange)
    {
        significantChange = this._forceSignificantChange || significantChange || false;

        var previousScrollTop = this._initialScrollOffset;

        // Only remember the scroll position if the previous node is the same as this one.
        if (this.element.parentNode && this._previousRefreshNodeIdentifier === this._nodeStyles.node.id)
            previousScrollTop = this.element.parentNode.scrollTop;

        this.refresh(significantChange);

        this._previousRefreshNodeIdentifier = this._nodeStyles.node.id;

        if (this.element.parentNode)
            this.element.parentNode.scrollTop = previousScrollTop;

        this._forceSignificantChange = false;
    }

    _nodeStylesNeedsRefreshed(event)
    {
        if (this._visible)
            this._refreshNodeStyles();
    }
};

WI.StyleDetailsPanel.Event = {
    Refreshed: "style-details-panel-refreshed"
};
