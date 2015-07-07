/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.ComputedStyleDetailsPanel = function()
{
    WebInspector.StyleDetailsPanel.call(this, WebInspector.ComputedStyleDetailsPanel.StyleClassName, "computed", WebInspector.UIString("Computed"));

    this._computedStyleShowAllSetting = new WebInspector.Setting("computed-style-show-all", false);

    var computedStyleShowAllLabel = document.createElement("label");
    computedStyleShowAllLabel.textContent = WebInspector.UIString("Show All");

    this._computedStyleShowAllCheckbox = document.createElement("input");
    this._computedStyleShowAllCheckbox.type = "checkbox";
    this._computedStyleShowAllCheckbox.checked = this._computedStyleShowAllSetting.value;
    this._computedStyleShowAllCheckbox.addEventListener("change", this._computedStyleShowAllCheckboxValueChanged.bind(this));
    computedStyleShowAllLabel.appendChild(this._computedStyleShowAllCheckbox);

    this._propertiesTextEditor = new WebInspector.CSSStyleDeclarationTextEditor(this);
    this._propertiesTextEditor.showsImplicitProperties = this._computedStyleShowAllSetting.value;
    this._propertiesTextEditor.alwaysShowPropertyNames = ["display", "width", "height"];
    this._propertiesTextEditor.sortProperties = true;

    var propertiesRow = new WebInspector.DetailsSectionRow;
    var propertiesGroup = new WebInspector.DetailsSectionGroup([propertiesRow]);
    var propertiesSection = new WebInspector.DetailsSection("computed-style-properties", WebInspector.UIString("Properties"), [propertiesGroup], computedStyleShowAllLabel);

    propertiesRow.element.appendChild(this._propertiesTextEditor.element);

    // Region flow name is used to display the "flow-from" property of the Region Containers.
    this._regionFlowFragment = document.createElement("span");
    this._regionFlowFragment.appendChild(document.createElement("img")).className = "icon";
    this._regionFlowNameLabelValue = this._regionFlowFragment.appendChild(document.createElement("span"));

    var goToRegionFlowButton = this._regionFlowFragment.appendChild(WebInspector.createGoToArrowButton());
    goToRegionFlowButton.addEventListener("click", this._goToRegionFlowArrowWasClicked.bind(this));

    this._regionFlowNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Region Flow"));
    this._regionFlowNameRow.element.classList.add("content-flow-link");

    // Content flow name is used to display the "flow-into" property of the Content nodes.
    this._contentFlowFragment = document.createElement("span");
    this._contentFlowFragment.appendChild(document.createElement("img")).className = "icon";
    this._contentFlowNameLabelValue = this._contentFlowFragment.appendChild(document.createElement("span"));

    var goToContentFlowButton = this._contentFlowFragment.appendChild(WebInspector.createGoToArrowButton());
    goToContentFlowButton.addEventListener("click", this._goToContentFlowArrowWasClicked.bind(this));

    this._contentFlowNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Content Flow"));
    this._contentFlowNameRow.element.classList.add("content-flow-link");

    var flowNamesGroup = new WebInspector.DetailsSectionGroup([this._regionFlowNameRow, this._contentFlowNameRow]);
    this._flowNamesSection = new WebInspector.DetailsSection("content-flow", WebInspector.UIString("Flows"), [flowNamesGroup]);

    this._containerRegionsDataGrid = new WebInspector.DOMTreeDataGrid;
    this._containerRegionsDataGrid.element.classList.add("no-header");

    var containerRegionsRow = new WebInspector.DetailsSectionDataGridRow(this._containerRegionsDataGrid);
    var containerRegionsGroup = new WebInspector.DetailsSectionGroup([containerRegionsRow]);
    this._containerRegionsFlowSection = new WebInspector.DetailsSection("container-regions", WebInspector.UIString("Container Regions"), [containerRegionsGroup]);

    this.element.appendChild(propertiesSection.element);
    this.element.appendChild(this._flowNamesSection.element);
    this.element.appendChild(this._containerRegionsFlowSection.element);

    this._resetFlowDetails();
};

WebInspector.ComputedStyleDetailsPanel.StyleClassName = "computed";

WebInspector.ComputedStyleDetailsPanel.prototype = {
    constructor: WebInspector.ComputedStyleDetailsPanel,

    // Public

    get regionFlow()
    {
        return this._regionFlow;
    },

    set regionFlow(regionFlow)
    {
        this._regionFlow = regionFlow;
        this._regionFlowNameLabelValue.textContent = regionFlow ? regionFlow.name : "";
        this._regionFlowNameRow.value = regionFlow ? this._regionFlowFragment : null;
        this._updateFlowNamesSectionVisibility();
    },

    get contentFlow()
    {
        return this._contentFlow;
    },

    set contentFlow(contentFlow)
    {
        this._contentFlow = contentFlow;
        this._contentFlowNameLabelValue.textContent = contentFlow ? contentFlow.name : "";
        this._contentFlowNameRow.value = contentFlow ? this._contentFlowFragment : null;
        this._updateFlowNamesSectionVisibility();
    },

    get containerRegions()
    {
        return this._containerRegions;
    },

    set containerRegions(regions)
    {
        this._containerRegions = regions;

        if (!regions || !regions.length) {
            this._containerRegionsFlowSection.element.classList.add("hidden");
            return;
        }

        this._containerRegionsDataGrid.removeChildren();
        for (var regionNode of regions)
            this._containerRegionsDataGrid.appendChild(new WebInspector.DOMTreeDataGridNode(regionNode));

        this._containerRegionsFlowSection.element.classList.remove("hidden");
    },

    refresh: function()
    {
        this._propertiesTextEditor.style = this.nodeStyles.computedStyle;
        this._refreshFlowDetails(this.nodeStyles.node);
    },

    // Protected

    shown: function()
    {
        WebInspector.StyleDetailsPanel.prototype.shown.call(this);

        this._propertiesTextEditor.updateLayout();
    },

    widthDidChange: function()
    {
        this._propertiesTextEditor.updateLayout();
    },

    // Private

    _computedStyleShowAllCheckboxValueChanged: function(event)
    {
        var checked = this._computedStyleShowAllCheckbox.checked;
        this._computedStyleShowAllSetting.value = checked;
        this._propertiesTextEditor.showsImplicitProperties = checked;
    },

    _updateFlowNamesSectionVisibility: function()
    {
        this._flowNamesSection.element.classList.toggle("hidden", !this._contentFlow && !this._regionFlow);
    },

    _resetFlowDetails : function()
    {
        this.regionFlow = null;
        this.contentFlow = null;
        this.containerRegions = null;
    },

    _refreshFlowDetails: function(domNode)
    {
        this._resetFlowDetails();
        if (!domNode)
            return;

        function contentFlowInfoReady(error, flowData)
        {
            // Element is not part of any flow.
            if (error || !flowData) {
                this._resetFlowDetails();
                return;
            }

            this.regionFlow = flowData.regionFlow;
            this.contentFlow = flowData.contentFlow;
            this.containerRegions = flowData.regions;
        }

        WebInspector.domTreeManager.getNodeContentFlowInfo(domNode, contentFlowInfoReady.bind(this));
    },

    _goToRegionFlowArrowWasClicked: function()
    {
        WebInspector.resourceSidebarPanel.showContentFlowDOMTree(this._regionFlow);
    },

    _goToContentFlowArrowWasClicked: function()
    {
        WebInspector.resourceSidebarPanel.showContentFlowDOMTree(this._contentFlow, this.nodeStyles.node, true);
    }
};

WebInspector.ComputedStyleDetailsPanel.prototype.__proto__ = WebInspector.StyleDetailsPanel.prototype;
