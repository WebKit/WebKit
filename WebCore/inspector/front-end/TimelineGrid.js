/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.TimelineGrid = function()
{
    this.element = document.createElement("div");

    this._itemsGraphsElement = document.createElement("div");
    this._itemsGraphsElement.id = "resources-graphs";
    this.element.appendChild(this._itemsGraphsElement);

    this._dividersElement = document.createElement("div");
    this._dividersElement.id = "resources-dividers";
    this.element.appendChild(this._dividersElement);

    this._eventDividersElement = document.createElement("div");
    this._eventDividersElement.id = "resources-event-dividers";
    this.element.appendChild(this._eventDividersElement);

    this._dividersLabelBarElement = document.createElement("div");
    this._dividersLabelBarElement.id = "resources-dividers-label-bar";
    this.element.appendChild(this._dividersLabelBarElement);
}

WebInspector.TimelineGrid.prototype = {
    get itemsGraphsElement()
    {
        return this._itemsGraphsElement;
    },

    updateDividers: function(force, calculator)
    {
        var dividerCount = Math.round(this._dividersElement.offsetWidth / 64);
        var slice = calculator.boundarySpan / dividerCount;
        if (!force && this._currentDividerSlice === slice)
            return false;

        this._currentDividerSlice = slice;

        this._dividersElement.removeChildren();
        this._eventDividersElement.removeChildren();
        this._dividersLabelBarElement.removeChildren();

        for (var i = 1; i <= dividerCount; ++i) {
            var divider = document.createElement("div");
            divider.className = "resources-divider";
            if (i === dividerCount)
                divider.addStyleClass("last");
            divider.style.left = ((i / dividerCount) * 100) + "%";

            this._dividersElement.appendChild(divider.cloneNode());

            var label = document.createElement("div");
            label.className = "resources-divider-label";
            if (!isNaN(slice))
                label.textContent = calculator.formatValue(slice * i);
            divider.appendChild(label);

            this._dividersLabelBarElement.appendChild(divider);
        }
        return true;
    },

    addEventDivider: function(divider)
    {
        this._eventDividersElement.appendChild(divider);
    },

    setScrollAndDividerTop: function(scrollTop, dividersTop)
    {
        this._dividersElement.style.top = scrollTop + "px";
        this._eventDividersElement.style.top = scrollTop + "px";
        this._dividersLabelBarElement.style.top = dividersTop + "px";
    }
}
