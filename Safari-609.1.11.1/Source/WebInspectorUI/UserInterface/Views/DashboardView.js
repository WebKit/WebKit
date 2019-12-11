/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

WI.DashboardView = class DashboardView extends WI.Object
{
    constructor(representedObject, identifier)
    {
        console.assert(identifier);

        super();

        this._representedObject = representedObject;

        this._element = document.createElement("div");
        this._element.classList.add("dashboard");
        this._element.classList.add(identifier);
    }

    // Static

    static create(representedObject)
    {
        console.assert(representedObject);

        if (representedObject instanceof WI.DefaultDashboard)
            return new WI.DefaultDashboardView(representedObject);

        if (representedObject instanceof WI.DebuggerDashboard)
            return new WI.DebuggerDashboardView(representedObject);

        throw "Can't make a DashboardView for an unknown representedObject.";
    }

    // Public

    get element()
    {
        return this._element;
    }

    get representedObject()
    {
        return this._representedObject;
    }

    shown()
    {
        // Implemented by subclasses.
    }

    hidden()
    {
        // Implemented by subclasses.
    }

    closed()
    {
        // Implemented by subclasses.
    }
};
