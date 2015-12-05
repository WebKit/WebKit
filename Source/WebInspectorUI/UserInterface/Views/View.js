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
        this._parentView = null;
        this._subviews = [];
        this._dirty = false;
        this._dirtyDescendantsCount = 0;
    }

    // Public

    get element()
    {
        return this._element;
    }

    get layoutPending()
    {
        return this._dirty;
    }

    get parentView()
    {
        return this._parentView;
    }

    get subviews()
    {
        return this._subviews;
    }

    makeRootView()
    {
        console.assert(!WebInspector.View._rootView, "Root view already exists.");
        console.assert(!this._parentView, "Root view cannot be a subview.");
        if (WebInspector.View._rootView)
            return;

        WebInspector.View._rootView = this;
    }

    addSubview(view)
    {
        this.insertSubviewBefore(view, null);
    }

    insertSubviewBefore(view, referenceView)
    {
        console.assert(view instanceof WebInspector.View);
        console.assert(!referenceView || referenceView instanceof WebInspector.View);
        console.assert(view !== WebInspector.View._rootView, "Root view cannot be a subview.");

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

        if (!view.element.parentNode)
            this._element.insertBefore(view.element, referenceView ? referenceView.element : null);

        view.didAttach(this);
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
        view.didDetach();
    }

    replaceSubview(oldView, newView)
    {
        console.assert(oldView !== newView, "Cannot replace subview with itself.");

        this.insertSubviewBefore(newView, oldView);
        this.removeSubview(oldView);
    }

    updateLayout()
    {
        WebInspector.View._cancelScheduledLayoutForView(this);
        this._layoutSubtree();
    }

    updateLayoutIfNeeded()
    {
        if (!this._dirty)
            return;

        this.updateLayout();
    }

    needsLayout()
    {
        if (this._dirty)
            return;

        WebInspector.View._scheduleLayoutForView(this);
    }

    // Protected

    didAttach(parentView)
    {
        console.assert(!this._parentView, "Attached view already has a parent.", this._parentView);
        this._parentView = parentView;
    }

    didDetach()
    {
        console.assert(this._parentView, "Detached view has no parent.");
        this._parentView = null;
    }

    layout()
    {
        // Overridden by subclasses.
        // Not responsible for recursing to child views.
        // Should not be called directly; use updateLayout() instead.
    }

    // Private

    _layoutSubtree()
    {
        this._dirty = false;
        this._dirtyDescendantsCount = 0;

        this.layout();

        for (let view of this._subviews)
            view._layoutSubtree();
    }

    // Private layout controller logic

    static _scheduleLayoutForView(view)
    {
        let isDescendantOfRoot = false;
        let parentView = view.parentView;
        while (parentView) {
            parentView._dirtyDescendantsCount++;
            if (parentView === WebInspector.View._rootView) {
                isDescendantOfRoot = true;
                break;
            }
            parentView = parentView.parentView;
        }

        // If the view is not attached to the main view tree, switch to a synchronous layout.
        if (!isDescendantOfRoot) {
            parentView = view.parentView;
            while (parentView) {
                parentView._dirtyDescendantsCount--;
                parentView = parentView.parentView;
            }

            view._layoutSubtree();
            return;
        }

        view._dirty = true;

        if (WebInspector.View._scheduledLayoutUpdateIdentifier)
            return;

        WebInspector.View._scheduledLayoutUpdateIdentifier = requestAnimationFrame(WebInspector.View._visitViewTreeForLayout);
    }

    static _cancelScheduledLayoutForView(view)
    {
        // Asynchronous layouts aren't scheduled until the root view has been set.
        if (!WebInspector.View._rootView)
            return;

        let cancelledLayouts = view._dirtyDescendantsCount;
        if (this._dirty)
            cancelledLayouts++;

        let parentView = view.parentView;
        while (parentView) {
            parentView._dirtyDescendantsCount = Math.max(0, parentView._dirtyDescendantsCount - cancelledLayouts);
            parentView = parentView.parentView;
        }

        if (WebInspector.View._rootView._dirtyDescendantsCount || !WebInspector.View._scheduledLayoutUpdateIdentifier)
            return;

        // No views need layout, so cancel the pending requestAnimationFrame.
        cancelAnimationFrame(WebInspector.View._scheduledLayoutUpdateIdentifier);
        WebInspector.View._scheduledLayoutUpdateIdentifier = undefined;
    }

    static _visitViewTreeForLayout()
    {
        console.assert(WebInspector.View._rootView, "Cannot layout view tree without a root.");

        WebInspector.View._scheduledLayoutUpdateIdentifier = undefined;

        let views = [WebInspector.View._rootView];
        while (views.length) {
            let view = views.shift();
            if (view.layoutPending)
                view._layoutSubtree();
            else if (view._dirtyDescendantsCount) {
                views = views.concat(view.subviews);
                view._dirtyDescendantsCount = 0;
            }
        }
    }
};

WebInspector.View._rootView = null;
WebInspector.View._scheduledLayoutUpdateIdentifier = undefined;
