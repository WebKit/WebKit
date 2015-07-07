/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

WebInspector.DashboardView = function(representedObject, identifier)
{
    // Creating a new DashboardView directly returns an instance of the proper subclass.
    if (this.constructor === WebInspector.DashboardView) {
        console.assert(representedObject);

        if (representedObject instanceof WebInspector.DefaultDashboard)
            return new WebInspector.DefaultDashboardView(representedObject);

        if (representedObject instanceof WebInspector.DebuggerDashboard)
            return new WebInspector.DebuggerDashboardView(representedObject);

        throw "Can't make a DashboardView for an unknown representedObject.";
    }

    // Otherwise, a subclass is calling the base constructor.
    console.assert(this.constructor !== WebInspector.DashboardView && this instanceof WebInspector.DashboardView);
    console.assert(identifier);

    WebInspector.Object.call(this);

    this._representedObject = representedObject;

    this._element = document.createElement("div");
    this._element.classList.add(WebInspector.DashboardView.StyleClassName);
    this._element.classList.add(identifier);
};

WebInspector.DashboardView.StyleClassName = "dashboard";

WebInspector.DashboardView.prototype = {
    constructor: WebInspector.DashboardView,
    __proto__: WebInspector.Object.prototype,

    // Public

    get element()
    {
        return this._element;
    },

    get representedObject()
    {
        return this._representedObject;
    },

    shown: function()
    {
        // Implemented by subclasses.
    },

    hidden: function()
    {
        // Implemented by subclasses.
    },

    closed: function()
    {
        // Implemented by subclasses.
    }
};
