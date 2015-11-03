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

WebInspector.View = class View extends WebInspector.Object
{
    constructor(element)
    {
        super();

        this._element = element || document.createElement("div");
        this._element.__view = this;
        this._subviews = [];
    }

    // Public

    get element()
    {
        return this._element;
    }

    get subviews()
    {
        return this._subviews;
    }

    addSubview(view)
    {
        this.insertSubviewBefore(view, null);
    }

    insertSubviewBefore(view, referenceView)
    {
        console.assert(view instanceof WebInspector.View);
        console.assert(!referenceView || referenceView instanceof WebInspector.View);

        if (this._subviews.includes(view)) {
            console.assert(false, "Cannot add view that is already a subview.", view);
            return;
        }

        const beforeIndex = referenceView ? this._subviews.indexOf(referenceView) : this._subviews.length;
        if (beforeIndex === -1) {
            console.assert(false, "Cannot insert view. Invalid reference view.", referenceView);
            return;
        }

        this._subviews.insertAtIndex(view, beforeIndex);
        this._element.insertBefore(view.element, referenceView ? referenceView.element : null);
    }

    removeSubview(view)
    {
        console.assert(view instanceof WebInspector.View);
        console.assert(view.element.parentNode === this._element, "Subview DOM element must be a child of the parent view element.");

        if (!this._subviews.includes(view)) {
            console.assert(false, "Cannot remove view which isn't a subview.", view);
            return;
        }

        this._subviews.remove(view, true);
        this._element.removeChild(view.element);
    }

    replaceSubview(oldView, newView)
    {
        console.assert(oldView !== newView, "Cannot replace subview with itself.");

        this.insertSubviewBefore(newView, oldView);
        this.removeSubview(oldView);
    }

    updateLayout()
    {
        this._layoutSubtree();
    }

    updateLayoutIfNeeded()
    {
        if (!this._scheduledLayoutUpdateIdentifier)
            return;

        this.updateLayout();
    }

    needsLayout()
    {
        if (this._scheduledLayoutUpdateIdentifier)
            return;

        this._scheduledLayoutUpdateIdentifier = requestAnimationFrame(() => {
            this._scheduledLayoutUpdateIdentifier = undefined;
            this._layoutSubtree();
        });
    }

    // Protected

    layout()
    {
        // Overridden by subclasses.
        // Not responsible for recursing to child views.
        // Should not be called directly; use updateLayout() instead.
    }

    // Private

    _layoutSubtree()
    {
        if (this._scheduledLayoutUpdateIdentifier) {
            cancelAnimationFrame(this._scheduledLayoutUpdateIdentifier);
            this._scheduledLayoutUpdateIdentifier = undefined;
        }

        this.layout();

        for (let view of this._subviews)
            view._layoutSubtree();
    }
};
