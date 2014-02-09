/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

WebInspector.ProbeSetDetailsSection = function(probeSet)
{
    console.assert(probeSet instanceof WebInspector.ProbeSet, "Invalid ProbeSet argument:", probeSet);

    this._listeners = new WebInspector.EventListenerSet(this, "ProbeSetDetailsSection UI listeners");
    this._probeSet = probeSet;

    var optionsElement = document.createElement("div");
    optionsElement.classList.add(WebInspector.ProbeSetDetailsSection.SectionOptionsStyleClassName);
    var titleElement = this._probeSetPositionTextOrLink();
    titleElement.classList.add(WebInspector.ProbeSetDetailsSection.DontFloatLinkStyleClassName);
    optionsElement.appendChild(titleElement);

    var removeProbeButton = optionsElement.createChild("img");
    removeProbeButton.classList.add(WebInspector.ProbeSetDetailsSection.ProbeRemoveStyleClassName);
    removeProbeButton.classList.add(WebInspector.ProbeSetDetailsSection.ProbeButtonEnabledStyleClassName);
    this._listeners.register(removeProbeButton, "click", this._removeButtonClicked);

    var clearSamplesButton = optionsElement.createChild("img");
    clearSamplesButton.classList.add(WebInspector.ProbeSetDetailsSection.ProbeClearSamplesStyleClassName);
    clearSamplesButton.classList.add(WebInspector.ProbeSetDetailsSection.ProbeButtonEnabledStyleClassName);
    this._listeners.register(clearSamplesButton, "click", this._clearSamplesButtonClicked);

    var addProbeButton = optionsElement.createChild("img");
    addProbeButton.classList.add(WebInspector.ProbeSetDetailsSection.AddProbeValueStyleClassName);
    this._listeners.register(addProbeButton, "click", this._addProbeButtonClicked);

    this._dataGrid = new WebInspector.ProbeSetDataGrid(probeSet);
    this._dataGrid.element.classList.add("inline");
    var singletonRow = new WebInspector.DetailsSectionRow;
    singletonRow.element.appendChild(this._dataGrid.element);
    var probeSectionGroup = new WebInspector.DetailsSectionGroup([singletonRow]);

    var dummyTitle = "";
    WebInspector.DetailsSection.call(this, "probe", dummyTitle, [probeSectionGroup], optionsElement);
    this.element.classList.add(WebInspector.ProbeSetDetailsSection.StyleClassName);

    this._listeners.install();
};

WebInspector.ProbeSetDetailsSection.AddProbeValueStyleClassName = "probe-add";
WebInspector.ProbeSetDetailsSection.DontFloatLinkStyleClassName = "dont-float";
WebInspector.ProbeSetDetailsSection.ProbeButtonEnabledStyleClassName = "enabled";
WebInspector.ProbeSetDetailsSection.ProbePopoverElementStyleClassName = "probe-popover";
WebInspector.ProbeSetDetailsSection.ProbeClearSamplesStyleClassName = "probe-clear-samples";
WebInspector.ProbeSetDetailsSection.ProbeRemoveStyleClassName = "probe-remove";
WebInspector.ProbeSetDetailsSection.SectionOptionsStyleClassName = "options";
WebInspector.ProbeSetDetailsSection.StyleClassName = "probe-set";

WebInspector.ProbeSetDetailsSection.prototype = {
    __proto__: WebInspector.DetailsSection.prototype,
    constructor: WebInspector.ProbeSetDetailsSection,

    // Public

    closed: function()
    {
        this._listeners.uninstall(true);
        this.element.remove();
    },

    // Private

    _probeSetPositionTextOrLink: function()
    {
        var breakpoint = this._probeSet.breakpoint;
        return WebInspector.createSourceCodeLocationLink(breakpoint.sourceCodeLocation);
    },

    _addProbeButtonClicked: function(event)
    {
        function createProbeFromEnteredExpression(visiblePopover, event)
        {
            if (event.keyCode !== 13)
                return;
            var expression = event.target.value;
            this._probeSet.createProbe(expression);
            visiblePopover.dismiss();
        }

        var popover = new WebInspector.Popover;
        var content = document.createElement("div");
        content.classList.add(WebInspector.ProbeSetDetailsSection.ProbePopoverElementStyleClassName);
        content.createChild("div").textContent = WebInspector.UIString("Add New Probe Expression");
        var textBox = content.createChild("input");
        textBox.addEventListener("keypress", createProbeFromEnteredExpression.bind(this, popover));
        textBox.addEventListener("click", function (event) {event.target.select()});
        textBox.type = "text";
        textBox.placeholder = WebInspector.UIString("Expression");
        popover.content = content;
        var target = WebInspector.Rect.rectFromClientRect(event.target.getBoundingClientRect());
        popover.present(target, [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MAX_X]);
        textBox.select();
    },

    _removeButtonClicked: function(event)
    {
        this._probeSet.clear();
    },

    _clearSamplesButtonClicked: function(event)
    {
        this._probeSet.clearSamples();
    }
};
