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

WI.NavigationItem = class NavigationItem extends WI.Object
{
    constructor(identifier, role, label)
    {
        super();

        this._identifier = identifier || null;

        this._element = document.createElement("div");
        this._hidden = false;
        this._parentNavigationBar = null;

        if (role)
            this._element.setAttribute("role", role);
        if (label)
            this._element.setAttribute("aria-label", label);

        this._element.classList.add(...this._classNames);
        this._element.navigationItem = this;
    }

    // Public

    get identifier() { return this._identifier; }
    get element() { return this._element; }
    get minimumWidth() { return this._element.realOffsetWidth; }
    get parentNavigationBar() { return this._parentNavigationBar; }

    updateLayout(expandOnly)
    {
        // Implemented by subclasses.
    }

    get hidden()
    {
        return this._hidden;
    }

    set hidden(flag)
    {
        if (this._hidden === flag)
            return;

        this._hidden = flag;

        this._element.classList.toggle("hidden", flag);

        if (this._parentNavigationBar)
            this._parentNavigationBar.needsLayout();
    }

    // Private

    get _classNames()
    {
        var classNames = ["item"];
        if (this._identifier)
            classNames.push(this._identifier);
        if (this.additionalClassNames instanceof Array)
            classNames = classNames.concat(this.additionalClassNames);
        return classNames;
    }
};
