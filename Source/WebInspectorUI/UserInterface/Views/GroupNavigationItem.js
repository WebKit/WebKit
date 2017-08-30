/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.GroupNavigationItem = class GroupNavigationItem extends WI.NavigationItem
{
    constructor(navigationItems)
    {
        console.assert(Array.isArray(navigationItems));

        super();

        this._navigationItems = navigationItems;

        for (let item of this._navigationItems) {
            console.assert(item instanceof WI.NavigationItem);
            this.element.appendChild(item.element);
        }
    }

    // Public

    get navigationItems() { return this._navigationItems; }

    get minimumWidth()
    {
        return this._navigationItems.reduce((total, item) => total + item.minimumWidth, 0);
    }

    // Protected

    updateLayout(expandOnly)
    {
        super.updateLayout(expandOnly);

        for (let item of this._navigationItems)
            item.updateLayout(expandOnly);
    }

    didAttach(navigationBar)
    {
        super.didAttach(navigationBar);

        for (let item of this._navigationItems)
            item.didAttach(navigationBar);
    }

    didDetach()
    {
        for (let item of this._navigationItems)
            item.didDetach();

        super.didDetach();
    }
}
