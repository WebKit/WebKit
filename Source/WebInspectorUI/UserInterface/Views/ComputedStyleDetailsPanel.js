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

WebInspector.ComputedStyleDetailsPanel = class ComputedStyleDetailsPanel extends WebInspector.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, WebInspector.ComputedStyleDetailsPanel.StyleClassName, "computed", WebInspector.UIString("Styles \u2014 Computed"));

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
        propertiesSection.addEventListener(WebInspector.DetailsSection.Event.CollapsedStateChanged, this._handleCollapsedStateChanged, this);

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

        this._boxModelDiagramRow = new WebInspector.BoxModelDetailsSectionRow;

        var boxModelGroup = new WebInspector.DetailsSectionGroup([this._boxModelDiagramRow]);
        var boxModelSection = new WebInspector.DetailsSection("style-box-model", WebInspector.UIString("Box Model"), [boxModelGroup]);

        this.element.appendChild(boxModelSection.element);
        
        this.cssStyleDeclarationTextEditorShouldAddPropertyGoToArrows = true;
    }

    // Public

    get regionFlow()
    {
        return this._regionFlow;
    }

    set regionFlow(regionFlow)
    {
        this._regionFlow = regionFlow;
        this._regionFlowNameLabelValue.textContent = regionFlow ? regionFlow.name : "";
        this._regionFlowNameRow.value = regionFlow ? this._regionFlowFragment : null;
        this._updateFlowNamesSectionVisibility();
    }

    get contentFlow()
    {
        return this._contentFlow;
    }

    set contentFlow(contentFlow)
    {
        this._contentFlow = contentFlow;
        this._contentFlowNameLabelValue.textContent = contentFlow ? contentFlow.name : "";
        this._contentFlowNameRow.value = contentFlow ? this._contentFlowFragment : null;
        this._updateFlowNamesSectionVisibility();
    }

    get containerRegions()
    {
        return this._containerRegions;
    }

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
    }

    cssStyleDeclarationTextEditorShowProperty(property, showSource)
    {
        function delegateShowProperty() {
            if (typeof this._delegate.computedStyleDetailsPanelShowProperty === "function")
                this._delegate.computedStyleDetailsPanelShowProperty(property);
        }

        if (!showSource) {
            delegateShowProperty.call(this);
            return;
        }

        let effectiveProperty = this._nodeStyles.effectivePropertyForName(property.name);
        if (!effectiveProperty || !effectiveProperty.styleSheetTextRange) {
            if (!effectiveProperty.relatedShorthandProperty) {
                delegateShowProperty.call(this);
                return;
            }
            effectiveProperty = effectiveProperty.relatedShorthandProperty;
        }

        let ownerRule = effectiveProperty.ownerStyle.ownerRule;
        if (!ownerRule) {
            delegateShowProperty.call(this);
            return;
        }

        let sourceCode = ownerRule.sourceCodeLocation.sourceCode;
        let {startLine, startColumn} = effectiveProperty.styleSheetTextRange;
        let sourceCodeLocation = sourceCode.createSourceCodeLocation(startLine, startColumn);
        WebInspector.showSourceCodeLocation(sourceCodeLocation);
    }

    refresh(significantChange)
    {
        // We only need to do a rebuild on significant changes. Other changes are handled
        // by the sections and text editors themselves.
        if (!significantChange) {
            super.refresh();
            return;
        }

        this._propertiesTextEditor.style = this.nodeStyles.computedStyle;
        this._refreshFlowDetails(this.nodeStyles.node);
        this._boxModelDiagramRow.nodeStyles = this.nodeStyles;

        super.refresh();
    }

    filterDidChange(filterBar)
    {
        this._propertiesTextEditor.removeNonMatchingProperties(filterBar.filters.text);
    }

    // Protected

    shown()
    {
        super.shown();

        this._propertiesTextEditor.updateLayout();
    }

    widthDidChange()
    {
        this._propertiesTextEditor.updateLayout();
    }

    // Private

    _computedStyleShowAllCheckboxValueChanged(event)
    {
        var checked = this._computedStyleShowAllCheckbox.checked;
        this._computedStyleShowAllSetting.value = checked;
        this._propertiesTextEditor.showsImplicitProperties = checked;
    }

    _handleCollapsedStateChanged(event)
    {
        if (event && event.data && !event.data.collapsed)
            this._propertiesTextEditor.refresh();
    }

    _updateFlowNamesSectionVisibility()
    {
        this._flowNamesSection.element.classList.toggle("hidden", !this._contentFlow && !this._regionFlow);
    }

    _resetFlowDetails ()
    {
        this.regionFlow = null;
        this.contentFlow = null;
        this.containerRegions = null;
    }

    _refreshFlowDetails(domNode)
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
    }

    _goToRegionFlowArrowWasClicked()
    {
        WebInspector.showContentFlowDOMTree(this._regionFlow);
    }

    _goToContentFlowArrowWasClicked()
    {
        WebInspector.showContentFlowDOMTree(this._contentFlow, this.nodeStyles.node);
    }
};

WebInspector.ComputedStyleDetailsPanel.StyleClassName = "computed";
