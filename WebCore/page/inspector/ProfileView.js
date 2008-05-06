/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ProfileView = function(profile)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("profile-view");

    this.profile = profile;
    
    // Create the header
    var headerTable = document.createElement("table");
    headerTable.className = "data-grid";
    this.element.appendChild(headerTable);

    var headerTableTR = document.createElement("tr");
    headerTable.appendChild(headerTableTR);

    var headerTableSelf = document.createElement("th");
    headerTableSelf.className = "narrow sort-descending selected";
    headerTableSelf.addEventListener("click", this._toggleSortOrder.bind(this), false);
    headerTableSelf.textContent = WebInspector.UIString("Self");
    headerTableTR.appendChild(headerTableSelf);

    this.selectedHeaderColumn = headerTableSelf;

    var headerTableTotal = document.createElement("th");
    headerTableTotal.classname = "narrow";
    headerTableTotal.addEventListener("click", this._toggleSortOrder.bind(this), false);
    headerTableTotal.textContent = WebInspector.UIString("Total");
    headerTableTR.appendChild(headerTableTotal);

    var headerTableCalls = document.createElement("th");
    headerTableCalls.classname = "narrow";
    headerTableCalls.addEventListener("click", this._toggleSortOrder.bind(this), false);
    headerTableCalls.textContent = WebInspector.UIString("Calls");
    headerTableTR.appendChild(headerTableCalls);

    var headerTableSymbol = document.createElement("th");
    headerTableSymbol.addEventListener("click", this._toggleSortOrder.bind(this), false);
    headerTableSymbol.textContent = WebInspector.UIString("Function");
    headerTableTR.appendChild(headerTableSymbol);
}

WebInspector.ProfileView.prototype = {

    _toggleSortOrder: function(event)
    {
        var headerElement = event.target;
        if (headerElement.hasStyleClass("sort-descending")) {
            headerElement.removeStyleClass("sort-descending");
            headerElement.addStyleClass("sort-ascending");
        } else if (headerElement.hasStyleClass("sort-ascending")) {
            headerElement.removeStyleClass("sort-ascending");
            headerElement.addStyleClass("sort-descending");
        } else {
            this.selectedHeaderColumn.removeStyleClass("sort-ascending");
            this.selectedHeaderColumn.removeStyleClass("sort-descending");
            this.selectedHeaderColumn.removeStyleClass("selected");
            headerElement.addStyleClass("sort-descending");
            headerElement.addStyleClass("selected");
            this.selectedHeaderColumn = headerElement;
        }
    },

}

WebInspector.ProfileView.prototype.__proto__ = WebInspector.View.prototype;
