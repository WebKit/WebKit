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
        super("css-style", WebInspector.UIString("Styles"), WebInspector.UIString("Style"), null, true);

        this._selectedPanel = null;

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
                labelElement.append(label);

                if (!groupElement || groupElement.children.length === 2) {
                    groupElement = document.createElement("div");
                    groupElement.className = "group";
                    this._forcedPseudoClassContainer.appendChild(groupElement);
                }

                groupElement.appendChild(labelElement);
            }, this);

            this.contentView.element.appendChild(this._forcedPseudoClassContainer);
        }

        this._computedStyleDetailsPanel = new WebInspector.ComputedStyleDetailsPanel(this);
        this._rulesStyleDetailsPanel = new WebInspector.RulesStyleDetailsPanel(this);
        this._visualStyleDetailsPanel = new WebInspector.VisualStyleDetailsPanel(this);

        this._computedStyleDetailsPanel.addEventListener(WebInspector.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);
        this._rulesStyleDetailsPanel.addEventListener(WebInspector.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);

        this._panels = [this._computedStyleDetailsPanel, this._rulesStyleDetailsPanel, this._visualStyleDetailsPanel];
        this._panelNavigationInfo = [this._computedStyleDetailsPanel.navigationInfo, this._rulesStyleDetailsPanel.navigationInfo, this._visualStyleDetailsPanel.navigationInfo];

        this._lastSelectedSectionSetting = new WebInspector.Setting("last-selected-style-details-panel", this._rulesStyleDetailsPanel.navigationInfo.identifier);

        let selectedPanel = this._panelMatchingIdentifier(this._lastSelectedSectionSetting.value);
        if (!selectedPanel)
            selectedPanel = this._rulesStyleDetailsPanel;

        this._switchPanels(selectedPanel);

        this._navigationItem = new WebInspector.ScopeRadioButtonNavigationItem(this._identifier, this._displayName, this._panelNavigationInfo, selectedPanel.navigationInfo);
        this._navigationItem.addEventListener(WebInspector.ScopeRadioButtonNavigationItem.Event.SelectedItemChanged, this._handleSelectedItemChanged, this);

        var optionsContainer = document.createElement("div");
        optionsContainer.classList.add("options-container");

        var newRuleButton = document.createElement("img");
        newRuleButton.classList.add("new-rule");
        newRuleButton.title = WebInspector.UIString("New Rule");
        newRuleButton.addEventListener("click", this._newRuleButtonClicked.bind(this));
        optionsContainer.appendChild(newRuleButton);

        this._filterBar = new WebInspector.FilterBar;
        this._filterBar.placeholder = WebInspector.UIString("Filter Styles");
        this._filterBar.addEventListener(WebInspector.FilterBar.Event.FilterDidChange, this._filterDidChange, this);
        optionsContainer.appendChild(this._filterBar.element);

        WebInspector.cssStyleManager.addEventListener(WebInspector.CSSStyleManager.Event.StyleSheetAdded, this.refresh, this);
        WebInspector.cssStyleManager.addEventListener(WebInspector.CSSStyleManager.Event.StyleSheetRemoved, this.refresh, this);

        this.element.appendChild(optionsContainer);
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

        this.contentView.element.scrollTop = this._initialScrollOffset;

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

        this._navigationItem.selectedItemIdentifier = this._lastSelectedSectionSetting.value;
    }

    // Protected

    addEventListeners()
    {
        var effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

        effectiveDOMNode.addEventListener(WebInspector.DOMNode.Event.EnabledPseudoClassesChanged, this._updatePseudoClassCheckboxes, this);
    }

    removeEventListeners()
    {
        var effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

        effectiveDOMNode.removeEventListener(null, null, this);
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

    _panelMatchingIdentifier(identifier)
    {
        var selectedPanel

        for (var panel of this._panels) {
            if (panel.navigationInfo.identifier !== identifier)
                continue;

            selectedPanel = panel;
            break;
        }

        return selectedPanel;
    }

    _handleSelectedItemChanged()
    {
        var selectedIdentifier = this._navigationItem.selectedItemIdentifier;
        var selectedPanel = this._panelMatchingIdentifier(selectedIdentifier);
        this._switchPanels(selectedPanel);
    }

    _switchPanels(selectedPanel)
    {
        console.assert(selectedPanel);

        if (this._selectedPanel) {
            this._selectedPanel.hidden();
            this._selectedPanel.element._savedScrollTop = this.contentView.element.scrollTop;
            this.contentView.removeSubview(this._selectedPanel);
        }

        this._selectedPanel = selectedPanel;
        if (!this._selectedPanel)
            return;

        this.contentView.addSubview(this._selectedPanel);

        if (typeof this._selectedPanel.element._savedScrollTop === "number")
            this.contentView.element.scrollTop = this._selectedPanel.element._savedScrollTop;
        else
            this.contentView.element.scrollTop = this._initialScrollOffset;

        let hasFilter = typeof this._selectedPanel.filterDidChange === "function";
        this.contentView.element.classList.toggle("has-filter-bar", hasFilter);
        if (this._filterBar)
            this.contentView.element.classList.toggle(WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, hasFilter && this._filterBar.hasActiveFilters());

        this.contentView.element.classList.toggle("supports-new-rule", typeof this._selectedPanel.newRuleButtonClicked === "function");
        this._selectedPanel.shown();

        this._lastSelectedSectionSetting.value = selectedPanel.navigationInfo.identifier;
    }

    _forcedPseudoClassCheckboxChanged(pseudoClass, event)
    {
        if (!this.domNode)
            return;

        var effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;

        effectiveDOMNode.setPseudoClassEnabled(pseudoClass, event.target.checked);
    }

    _updatePseudoClassCheckboxes()
    {
        if (!this.domNode)
            return;

        var effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;

        var enabledPseudoClasses = effectiveDOMNode.enabledPseudoClasses;

        for (var pseudoClass in this._forcedPseudoClassCheckboxes) {
            var checkboxElement = this._forcedPseudoClassCheckboxes[pseudoClass];
            checkboxElement.checked = enabledPseudoClasses.includes(pseudoClass);
        }
    }

    _newRuleButtonClicked()
    {
        if (this._selectedPanel && typeof this._selectedPanel.newRuleButtonClicked === "function")
            this._selectedPanel.newRuleButtonClicked();
    }

    _filterDidChange()
    {
        this.contentView.element.classList.toggle(WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, this._filterBar.hasActiveFilters());

        this._selectedPanel.filterDidChange(this._filterBar);
    }
};

WebInspector.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = 30; // Default height of the forced pseudo classes container. Updated in widthDidChange.
WebInspector.CSSStyleDetailsSidebarPanel.FilterInProgressClassName = "filter-in-progress";
WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName = "filter-section-has-label";
WebInspector.CSSStyleDetailsSidebarPanel.FilterMatchSectionClassName = "filter-matching";
WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName = "filter-section-non-matching";
WebInspector.CSSStyleDetailsSidebarPanel.NoFilterMatchInPropertyClassName = "filter-property-non-matching";

