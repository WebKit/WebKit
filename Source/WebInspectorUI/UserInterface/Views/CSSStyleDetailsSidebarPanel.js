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

WI.CSSStyleDetailsSidebarPanel = class CSSStyleDetailsSidebarPanel extends WI.DOMDetailsSidebarPanel
{
    constructor()
    {
        const dontCreateNavigationItem = true;
        super("css-style", WI.UIString("Styles"), dontCreateNavigationItem);

        this._selectedPanel = null;
        this._computedStyleDetailsPanel = new WI.ComputedStyleDetailsPanel(this);
        this._rulesStyleDetailsPanel = new WI.RulesStyleDetailsPanel(this);
        this._visualStyleDetailsPanel = new WI.VisualStyleDetailsPanel(this);

        if (WI.settings.experimentalSpreadsheetStyleEditor.value)
            this._activeRulesStyleDetailsPanel = new WI.RulesStyleSpreadsheetDetailsPanel(this);
        else
            this._activeRulesStyleDetailsPanel = this._rulesStyleDetailsPanel;

        this._panels = [this._computedStyleDetailsPanel, this._activeRulesStyleDetailsPanel, this._visualStyleDetailsPanel];
        this._panelNavigationInfo = [this._computedStyleDetailsPanel.navigationInfo, this._activeRulesStyleDetailsPanel.navigationInfo, this._visualStyleDetailsPanel.navigationInfo];

        this._lastSelectedPanelSetting = new WI.Setting("last-selected-style-details-panel", this._activeRulesStyleDetailsPanel.navigationInfo.identifier);
        this._classListContainerToggledSetting = new WI.Setting("class-list-container-toggled", false);

        this._initiallySelectedPanel = this._panelMatchingIdentifier(this._lastSelectedPanelSetting.value) || this._activeRulesStyleDetailsPanel;

        this._navigationItem = new WI.ScopeRadioButtonNavigationItem(this.identifier, this.displayName, this._panelNavigationInfo, this._initiallySelectedPanel.navigationInfo);
        this._navigationItem.addEventListener(WI.ScopeRadioButtonNavigationItem.Event.SelectedItemChanged, this._handleSelectedItemChanged, this);

        this._forcedPseudoClassCheckboxes = {};
    }

    // Public

    supportsDOMNode(nodeToInspect)
    {
        return nodeToInspect.nodeType() === Node.ELEMENT_NODE;
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

    computedStyleDetailsPanelShowProperty(property)
    {
        this._activeRulesStyleDetailsPanel.scrollToSectionAndHighlightProperty(property);
        this._switchPanels(this._activeRulesStyleDetailsPanel);

        this._navigationItem.selectedItemIdentifier = this._lastSelectedPanelSetting.value;
    }

    // Protected

    layout()
    {
        let domNode = this.domNode;
        if (!domNode)
            return;

        this.contentView.element.scrollTop = this._initialScrollOffset;

        for (let panel of this._panels) {
            panel.element._savedScrollTop = undefined;
            panel.markAsNeedsRefresh(domNode);
        }

        this._updatePseudoClassCheckboxes();

        if (!this._classListContainer.hidden)
            this._populateClassToggles();
    }

    addEventListeners()
    {
        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

        effectiveDOMNode.addEventListener(WI.DOMNode.Event.EnabledPseudoClassesChanged, this._updatePseudoClassCheckboxes, this);
        effectiveDOMNode.addEventListener(WI.DOMNode.Event.AttributeModified, this._handleNodeAttributeModified, this);
        effectiveDOMNode.addEventListener(WI.DOMNode.Event.AttributeRemoved, this._handleNodeAttributeRemoved, this);
    }

    removeEventListeners()
    {
        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

        effectiveDOMNode.removeEventListener(null, null, this);
    }

    initialLayout()
    {
        if (WI.cssStyleManager.canForcePseudoClasses()) {
            this._forcedPseudoClassContainer = document.createElement("div");
            this._forcedPseudoClassContainer.className = "pseudo-classes";

            let groupElement = null;

            WI.CSSStyleManager.ForceablePseudoClasses.forEach(function(pseudoClass) {
                // We don't localize the label since it is a CSS pseudo-class from the CSS standard.
                let label = pseudoClass.capitalize();

                let labelElement = document.createElement("label");

                let checkboxElement = document.createElement("input");
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

        this._computedStyleDetailsPanel.addEventListener(WI.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);
        this._rulesStyleDetailsPanel.addEventListener(WI.StyleDetailsPanel.Event.Refreshed, this._filterDidChange, this);

        console.assert(this._initiallySelectedPanel, "Should have an initially selected panel.");

        this._switchPanels(this._initiallySelectedPanel);
        this._initiallySelectedPanel = null;

        let optionsContainer = this.element.createChild("div", "options-container");

        let newRuleButton = optionsContainer.createChild("img", "new-rule");
        newRuleButton.title = WI.UIString("Add new rule");
        newRuleButton.addEventListener("click", this._newRuleButtonClicked.bind(this));
        newRuleButton.addEventListener("contextmenu", this._newRuleButtonContextMenu.bind(this));

        this._filterBar = new WI.FilterBar;
        this._filterBar.placeholder = WI.UIString("Filter Styles");
        this._filterBar.addEventListener(WI.FilterBar.Event.FilterDidChange, this._filterDidChange, this);
        optionsContainer.appendChild(this._filterBar.element);

        this._classToggleButton = optionsContainer.createChild("button", "toggle-class-toggle");
        this._classToggleButton.textContent = WI.UIString("Classes");
        this._classToggleButton.title = WI.UIString("Toggle Classes");
        this._classToggleButton.addEventListener("click", this._classToggleButtonClicked.bind(this));

        this._classListContainer = this.element.createChild("div", "class-list-container");
        this._classListContainer.hidden = true;

        this._addClassContainer = this._classListContainer.createChild("div", "new-class");
        this._addClassContainer.title = WI.UIString("Add a Class");
        this._addClassContainer.addEventListener("click", this._addClassContainerClicked.bind(this));

        this._addClassInput = this._addClassContainer.createChild("input", "class-name-input");
        this._addClassInput.setAttribute("placeholder", WI.UIString("Enter Class Name"));
        this._addClassInput.addEventListener("keypress", this._addClassInputKeyPressed.bind(this));
        this._addClassInput.addEventListener("blur", this._addClassInputBlur.bind(this));

        WI.cssStyleManager.addEventListener(WI.CSSStyleManager.Event.StyleSheetAdded, this._styleSheetAddedOrRemoved, this);
        WI.cssStyleManager.addEventListener(WI.CSSStyleManager.Event.StyleSheetRemoved, this._styleSheetAddedOrRemoved, this);

        if (this._classListContainerToggledSetting.value)
            this._classToggleButtonClicked();
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        this._updateNoForcedPseudoClassesScrollOffset();

        if (this._selectedPanel)
            this._selectedPanel.sizeDidChange();
    }

    // Private

    get _initialScrollOffset()
    {
        if (!WI.cssStyleManager.canForcePseudoClasses())
            return 0;
        return this.domNode && this.domNode.enabledPseudoClasses.length ? 0 : WI.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset;
    }

    _updateNoForcedPseudoClassesScrollOffset()
    {
        if (this._forcedPseudoClassContainer)
            WI.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = this._forcedPseudoClassContainer.offsetHeight;
    }

    _panelMatchingIdentifier(identifier)
    {
        let selectedPanel = null;
        for (let panel of this._panels) {
            if (panel.navigationInfo.identifier !== identifier)
                continue;

            selectedPanel = panel;
            break;
        }

        return selectedPanel;
    }

    _handleSelectedItemChanged()
    {
        let selectedIdentifier = this._navigationItem.selectedItemIdentifier;
        let selectedPanel = this._panelMatchingIdentifier(selectedIdentifier);
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
            this.contentView.element.classList.toggle(WI.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, hasFilter && this._filterBar.hasActiveFilters());

        this.contentView.element.classList.toggle("supports-new-rule", typeof this._selectedPanel.newRuleButtonClicked === "function");
        this._selectedPanel.shown();

        this._lastSelectedPanelSetting.value = selectedPanel.navigationInfo.identifier;
    }

    _forcedPseudoClassCheckboxChanged(pseudoClass, event)
    {
        if (!this.domNode)
            return;

        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;

        effectiveDOMNode.setPseudoClassEnabled(pseudoClass, event.target.checked);
    }

    _updatePseudoClassCheckboxes()
    {
        if (!this.domNode)
            return;

        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;

        let enabledPseudoClasses = effectiveDOMNode.enabledPseudoClasses;

        for (let pseudoClass in this._forcedPseudoClassCheckboxes) {
            let checkboxElement = this._forcedPseudoClassCheckboxes[pseudoClass];
            checkboxElement.checked = enabledPseudoClasses.includes(pseudoClass);
        }
    }

    _handleNodeAttributeModified(event)
    {
        if (event && event.data && event.data.name === "class")
            this._populateClassToggles();
    }

    _handleNodeAttributeRemoved(event)
    {
        if (event && event.data && event.data.name === "class")
            this._populateClassToggles();
    }


    _newRuleButtonClicked()
    {
        if (this._selectedPanel && typeof this._selectedPanel.newRuleButtonClicked === "function")
            this._selectedPanel.newRuleButtonClicked();
    }

    _newRuleButtonContextMenu(event)
    {
        if (this._selectedPanel && typeof this._selectedPanel.newRuleButtonContextMenu === "function")
            this._selectedPanel.newRuleButtonContextMenu(event);
    }

    _classToggleButtonClicked(event)
    {
        this._classToggleButton.classList.toggle("selected");
        this._classListContainer.hidden = !this._classListContainer.hidden;
        this._classListContainerToggledSetting.value = !this._classListContainer.hidden;
        if (this._classListContainer.hidden)
            return;

        this._populateClassToggles();
    }

    _addClassContainerClicked(event)
    {
        this._addClassContainer.classList.add("active");
        this._addClassInput.focus();
    }

    _addClassInputKeyPressed(event)
    {
        if (event.keyCode !== WI.KeyboardShortcut.Key.Enter.keyCode)
            return;

        this._addClassInput.blur();
    }

    _addClassInputBlur(event)
    {
        this.domNode.toggleClass(this._addClassInput.value, true);
        this._addClassContainer.classList.remove("active");
        this._addClassInput.value = null;
    }

    _populateClassToggles()
    {
        // Ensure that _addClassContainer is the first child of _classListContainer.
        while (this._classListContainer.children.length > 1)
            this._classListContainer.children[1].remove();

        let classes = this.domNode.getAttribute("class");
        let classToggledMap = this.domNode[WI.CSSStyleDetailsSidebarPanel.ToggledClassesSymbol];
        if (!classToggledMap)
            classToggledMap = this.domNode[WI.CSSStyleDetailsSidebarPanel.ToggledClassesSymbol] = new Map;

        if (classes && classes.length) {
            for (let className of classes.split(/\s+/))
                classToggledMap.set(className, true);
        }

        for (let [className, toggled] of classToggledMap) {
            if ((toggled && !classes.includes(className)) || (!toggled && classes.includes(className))) {
                toggled = !toggled;
                classToggledMap.set(className, toggled);
            }

            this._createToggleForClassName(className);
        }
    }

    _createToggleForClassName(className)
    {
        if (!className || !className.length)
            return;

        let classToggledMap = this.domNode[WI.CSSStyleDetailsSidebarPanel.ToggledClassesSymbol];
        if (!classToggledMap)
            return;

        if (!classToggledMap.has(className))
            classToggledMap.set(className, true);

        let toggled = classToggledMap.get(className);

        let classNameContainer = document.createElement("div");
        classNameContainer.classList.add("class-toggle");

        let classNameToggle = classNameContainer.createChild("input");
        classNameToggle.type = "checkbox";
        classNameToggle.checked = toggled;

        let classNameTitle = classNameContainer.createChild("span");
        classNameTitle.textContent = className;
        classNameTitle.draggable = true;
        classNameTitle.addEventListener("dragstart", (event) => {
            event.dataTransfer.setData(WI.CSSStyleDetailsSidebarPanel.ToggledClassesDragType, className);
            event.dataTransfer.effectAllowed = "copy";
        });

        let classNameToggleChanged = (event) => {
            this.domNode.toggleClass(className, classNameToggle.checked);
            classToggledMap.set(className, classNameToggle.checked);
        };

        classNameToggle.addEventListener("click", classNameToggleChanged);
        classNameTitle.addEventListener("click", (event) => {
            classNameToggle.checked = !classNameToggle.checked;
            classNameToggleChanged();
        });

        this._classListContainer.appendChild(classNameContainer);
    }

    _filterDidChange()
    {
        this.contentView.element.classList.toggle(WI.CSSStyleDetailsSidebarPanel.FilterInProgressClassName, this._filterBar.hasActiveFilters());

        this._selectedPanel.filterDidChange(this._filterBar);
    }

    _styleSheetAddedOrRemoved()
    {
        this.needsLayout();
    }
};

WI.CSSStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = 30; // Default height of the forced pseudo classes container. Updated in sizeDidChange.
WI.CSSStyleDetailsSidebarPanel.FilterInProgressClassName = "filter-in-progress";
WI.CSSStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName = "filter-section-has-label";
WI.CSSStyleDetailsSidebarPanel.FilterMatchSectionClassName = "filter-matching";
WI.CSSStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName = "filter-section-non-matching";
WI.CSSStyleDetailsSidebarPanel.NoFilterMatchInPropertyClassName = "filter-property-non-matching";

WI.CSSStyleDetailsSidebarPanel.ToggledClassesSymbol = Symbol("css-style-details-sidebar-panel-toggled-classes-symbol");
WI.CSSStyleDetailsSidebarPanel.ToggledClassesDragType = "text/classname";
