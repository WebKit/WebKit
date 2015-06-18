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

WebInspector.CSSStyleDetailsSidebarPanel = class CSSStyleDetailsSidebarPanel extends WebInspector.DOMDetailsSidebarPanel
{
    constructor()
    {
        super("css-style", WebInspector.UIString("Styles"), WebInspector.UIString("Style"));

        this._selectedPanel = null;

        this._navigationBar = new WebInspector.NavigationBar(null, null, "tablist");
        this._navigationBar.addEventListener(WebInspector.NavigationBar.Event.NavigationItemSelected, this._navigationItemSelected, this);
        this.element.insertBefore(this._navigationBar.element, this.contentElement);

        this._forcedPseudoClassCheckboxes = {};

        if (WebInspector.cssStyleManager.canForcePseudoClasses()) {
            this._forcedPseudoClassContainer = document.createElement("div");
            this._forcedPseudoClassContainer.className = "pseudo-classes";

            var groupElement = null;

            WebInspector.CSSStyleManager.ForceablePseudoClasses.forEach(function(pseudoClass) {
                // We don't localize the label since it is a CSS pseudo-class from the CSS standard.
                var label = pseudoClass.capitalize();

                var labelElement = document.createElement("label");

                var checkboxElement = document.createElement("input");
                checkboxElement.addEventListener("change", this._forcedPseudoClassCheckboxChanged.bind(this, pseudoClass));
                checkboxElement.type = "checkbox";

                this._forcedPseudoClassCheckboxes[pseudoClass] = checkboxElement;

                labelElement.appendChild(checkboxElement);
                labelElement.appendChild(document.createTextNode(label));

                if (!groupElement || groupElement.children.length === 2) {
                    groupElement = document.createElement("div");
                    groupElement.className = "group";
                    this._forcedPseudoClassContainer.appendChild(groupElement);
                }

                groupElement.appendChild(labelElement);
            }, this);

            this.contentElement.appendChild(this._forcedPseudoClassContainer);
        }

        this._computedStyleDetailsPanel = new WebInspector.ComputedStyleDetailsPanel(this);
        this._rulesStyleDetailsPanel = new WebInspector.RulesStyleDetailsPanel(this);
        this._metricsStyleDetailsPanel = new WebInspector.MetricsStyleDetailsPanel(this);

        this._computedStyleDetailsPanel.addEventListener(WebInspector.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);
        this._rulesStyleDetailsPanel.addEventListener(WebInspector.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);

        this._panels = [this._computedStyleDetailsPanel, this._rulesStyleDetailsPanel, this._metricsStyleDetailsPanel];

        this._navigationBar.addNavigationItem(this._computedStyleDetailsPanel.navigationItem);
        this._navigationBar.addNavigationItem(this._rulesStyleDetailsPanel.navigationItem);
        this._navigationBar.addNavigationItem(this._metricsStyleDetailsPanel.navigationItem);

        this._lastSelectedSectionSetting = new WebInspector.Setting("last-selected-style-details-panel", this._rulesStyleDetailsPanel.navigationItem.identifier);

        // This will cause the selected panel to be set in _navigationItemSelected.
        this._navigationBar.selectedNavigationItem = this._lastSelectedSectionSetting.value;

        this._filterBar = new WebInspector.FilterBar;
        this._filterBar.placeholder = WebInspector.UIString("Filter Styles");
        this._filterBar.addEventListener(WebInspector.FilterBar.Event.FilterDidChange, this._filterDidChange, this);
        this.element.appendChild(this._filterBar.element);
    }

    // Public

    supportsDOMNode(nodeToInspect)
    {
        return nodeToInspect.nodeType() === Node.ELEMENT_NODE;
    }

    refresh()
    {
        var domNode = this.domNode;
        if (!domNode)
            return;

        this.contentElement.scrollTop = this._initialScrollOffset;

        for (var panel of this._panels) {
            panel.element._savedScrollTop = undefined;
            panel.markAsNeedsRefresh(domNode);
        }

        this._updatePseudoClassCheckboxes();
    }

    visibilityDidChange()
    {
        super.visibilityDidChange();

        if (!this._selectedPanel)
            return;

        if (!this.visible) {
            this._selectedPanel.hidden();
            return;
        }

        this._navigationBar.updateLayout();

        this._updateNoForcedPseudoClassesScrollOffset();

        this._selectedPanel.shown();
        this._selectedPanel.markAsNeedsRefresh(this.domNode);
    }

    widthDidChange()
    {
        this._updateNoForcedPseudoClassesScrollOffset();

        if (this._selectedPanel)
            this._selectedPanel.widthDidChange();
    }

    computedStyleDetailsPanelShowProperty(property)
    {
        this._rulesStyleDetailsPanel.scrollToSectionAndHighlightProperty(property);
        this._switchPanels(this._rulesStyleDetailsPanel);

        this._navigationBar.selectedNavigationItem = this._lastSelectedSectionSetting.value;
    }

    // Protected

    addEventListeners()
    {
        this.domNode.addEventListener(WebInspector.DOMNode.Event.EnabledPseudoClassesChanged, this._updatePseudoClassCheckboxes, this);
    }

    removeEventListeners()
    {
        this.domNode.removeEventListener(null, null, this);
    }

    // Private

    get _initialScrollOffset()
    {
        if (!WebInspector.cssStyleManager.canForcePseudoClasses())
            return 0;
        return this.domNode && this.domNode.enabledPseudoClasses.length ? 0 : WebInspector.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset;
    }

    _updateNoForcedPseudoClassesScrollOffset()
    {
        if (this._forcedPseudoClassContainer)
            WebInspector.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = this._forcedPseudoClassContainer.offsetHeight;
    }

    _navigationItemSelected(event)
    {
        console.assert(event.target.selectedNavigationItem);
        if (!event.target.selectedNavigationItem)
            return;

        var selectedNavigationItem = event.target.selectedNavigationItem;

        var selectedPanel = null;
        for (var i = 0; i < this._panels.length; ++i) {
            if (this._panels[i].navigationItem !== selectedNavigationItem)
                continue;
            selectedPanel = this._panels[i];
            break;
        }

        this._switchPanels(selectedPanel);
    }

    _switchPanels(selectedPanel)
    {
        console.assert(selectedPanel);

        if (this._selectedPanel) {
            this._selectedPanel.hidden();
            this._selectedPanel.element._savedScrollTop = this.contentElement.scrollTop;
            this._selectedPanel.element.remove();
        }

        this._selectedPanel = selectedPanel;

        if (this._selectedPanel) {
            this.contentElement.appendChild(this._selectedPanel.element);

            if (typeof this._selectedPanel.element._savedScrollTop === "number")
                this.contentElement.scrollTop = this._selectedPanel.element._savedScrollTop;
            else
                this.contentElement.scrollTop = this._initialScrollOffset;

            var hasFilter = typeof this._selectedPanel.filterDidChange === "function";
            this.contentElement.classList.toggle("has-filter-bar", hasFilter);
            if (this._filterBar)
                this.contentElement.classList.toggle(WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, hasFilter && this._filterBar.hasActiveFilters());

            this._selectedPanel.shown();
        }

        this._lastSelectedSectionSetting.value = selectedPanel.navigationItem.identifier;
    }

    _forcedPseudoClassCheckboxChanged(pseudoClass, event)
    {
        if (!this.domNode)
            return;

        this.domNode.setPseudoClassEnabled(pseudoClass, event.target.checked);
    }

    _updatePseudoClassCheckboxes()
    {
        if (!this.domNode)
            return;

        var enabledPseudoClasses = this.domNode.enabledPseudoClasses;

        for (var pseudoClass in this._forcedPseudoClassCheckboxes) {
            var checkboxElement = this._forcedPseudoClassCheckboxes[pseudoClass];
            checkboxElement.checked = enabledPseudoClasses.includes(pseudoClass);
        }
    }

    _filterDidChange()
    {
        this.contentElement.classList.toggle(WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, this._filterBar.hasActiveFilters());

        this._selectedPanel.filterDidChange(this._filterBar);
    }
};

WebInspector.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = 38; // Default height of the forced pseudo classes container. Updated in widthDidChange.
WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName = "filter-in-progress";
WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName = "filter-section-has-label";
WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchSectionClassName = "filter-matching";
WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName = "filter-section-non-matching";
WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInPropertyClassName = "filter-property-non-matching";

