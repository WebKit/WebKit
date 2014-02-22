/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ProbeDetailsSidebarPanel = function()
{
    WebInspector.DetailsSidebarPanel.call(this, "probe", WebInspector.UIString("Probes"), WebInspector.UIString("Probes"), "Images/NavigationItemProbes.pdf", "6");

    WebInspector.probeManager.addEventListener(WebInspector.ProbeManager.Event.ProbeSetAdded, this._probeSetAdded, this);
    WebInspector.probeManager.addEventListener(WebInspector.ProbeManager.Event.ProbeSetRemoved, this._probeSetRemoved, this);

    this._probeSetSections = new Map;
    this._inspectedProbeSets = [];

    // Initialize sidebar sections for probe sets that already exist.
    for (var probeSet of WebInspector.probeManager.probeSets)
        this._probeSetAdded(probeSet);
};

WebInspector.ProbeDetailsSidebarPanel.OffsetSectionsStyleClassName  = "offset-sections";

WebInspector.ProbeDetailsSidebarPanel.prototype = {
    constructor: WebInspector.ProbeDetailsSidebarPanel,
    __proto__: WebInspector.DetailsSidebarPanel.prototype,

    // Public

    get inspectedProbeSets()
    {
        return this._inspectedProbeSets.slice();
    },

    set inspectedProbeSets(newProbeSets)
    {
        for (var probeSet of this._inspectedProbeSets) {
            var removedSection = this._probeSetSections.get(probeSet);
            this.element.removeChild(removedSection.element);
        }

        this._inspectedProbeSets = newProbeSets;

        for (var probeSet of newProbeSets) {
            var shownSection = this._probeSetSections.get(probeSet);
            this.element.appendChild(shownSection.element);
        }
    },

    inspect: function(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        this.inspectedProbeSets = objects.filter(function(object) {
            return object instanceof WebInspector.ProbeSet;
        });

        return !!this._inspectedProbeSets.length;
    },

    // Private

    _probeSetAdded: function(probeSetOrEvent)
    {
        var probeSet;
        if (probeSetOrEvent instanceof WebInspector.ProbeSet)
            probeSet = probeSetOrEvent;
        else
            probeSet = probeSetOrEvent.data.probeSet;
        console.assert(!this._probeSetSections.has(probeSet), "New probe group ", probeSet, " already has its own sidebar.");

        var newSection = new WebInspector.ProbeSetDetailsSection(probeSet);
        this._probeSetSections.set(probeSet, newSection);
    },


    _probeSetRemoved: function(event)
    {
        var probeSet = event.data.probeSet;
        console.assert(this._probeSetSections.has(probeSet), "Removed probe group ", probeSet, " doesn't have a sidebar.");

        // First remove probe set from inspected list, then from mapping.
        var inspectedProbeSets = this.inspectedProbeSets;
        var index = inspectedProbeSets.indexOf(probeSet);
        if (index !== -1) {
            inspectedProbeSets.splice(index, 1);
            this.inspectedProbeSets = inspectedProbeSets;
        }
        var removedSection = this._probeSetSections.get(probeSet);
        this._probeSetSections.delete(probeSet);
        removedSection.closed();
    }
};
