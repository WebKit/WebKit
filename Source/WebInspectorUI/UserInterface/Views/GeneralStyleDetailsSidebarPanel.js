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

WI.GeneralStyleDetailsSidebarPanel = class GeneralStyleDetailsSidebarPanel extends WI.DOMDetailsSidebarPanel
{
    constructor(identifier, displayName, panelConstructor)
    {
        super(identifier, displayName);

        this.element.classList.add("css-style");
        this._panel = new panelConstructor(this);

        this._classListContainerToggledSetting = new WI.Setting("class-list-container-toggled", false);
        this._forcedPseudoClassCheckboxes = {};
    }

    // Public

    get panel() { return this._panel; }

    supportsDOMNode(nodeToInspect)
    {
        return nodeToInspect.nodeType() === Node.ELEMENT_NODE;
    }

    visibilityDidChange()
    {
        super.visibilityDidChange();

        if (!this._panel)
            return;

        if (!this.visible) {
            this._panel.hidden();
            return;
        }

        this._updateNoForcedPseudoClassesScrollOffset();

        this._panel.shown();
        this._panel.markAsNeedsRefresh(this.domNode);
    }

    computedStyleDetailsPanelShowProperty(property)
    {
        this.parentSidebar.selectedSidebarPanel = "style-rules";

        let styleRulesPanel = null;
        for (let sidebarPanel of this.parentSidebar.sidebarPanels) {
            if (!(sidebarPanel instanceof WI.RulesStyleDetailsSidebarPanel))
                continue;

            styleRulesPanel = sidebarPanel;
            break;
        }

        console.assert(styleRulesPanel, "Styles panel is missing.");
        styleRulesPanel.panel.scrollToSectionAndHighlightProperty(property);
    }

    // StyleDetailsPanel delegate

    styleDetailsPanelFocusLastPseudoClassCheckbox(styleDetailsPanel)
    {
        this._forcedPseudoClassCheckboxes[WI.CSSManager.ForceablePseudoClasses.lastValue].focus();
    }

    styleDetailsPanelFocusFilterBar(styleDetailsPanel)
    {
        this._filterBar.inputField.focus();
    }

    // Protected

    layout()
    {
        let domNode = this.domNode;
        if (!domNode)
            return;

        this.contentView.element.scrollTop = this._initialScrollOffset;
        this._panel.markAsNeedsRefresh(domNode);

        this._updatePseudoClassCheckboxes();
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
        if (WI.cssManager.canForcePseudoClasses()) {
            this._forcedPseudoClassContainer = document.createElement("div");
            this._forcedPseudoClassContainer.className = "pseudo-classes";

            let groupElement = null;

            WI.CSSManager.ForceablePseudoClasses.forEach(function(pseudoClass) {
                // We don't localize the label since it is a CSS pseudo-class from the CSS standard.
                let label = pseudoClass.capitalize();

                let labelElement = document.createElement("label");

                let checkboxElement = document.createElement("input");
                checkboxElement.addEventListener("keydown", this._handleForcedPseudoClassCheckboxKeydown.bind(this, pseudoClass));
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

        this._showPanel(this._panel);

        let optionsContainer = this.element.createChild("div", "options-container");

        let newRuleButton = optionsContainer.createChild("img", "new-rule");
        newRuleButton.title = WI.UIString("Add new rule");
        newRuleButton.addEventListener("click", this._newRuleButtonClicked.bind(this));
        newRuleButton.addEventListener("contextmenu", this._newRuleButtonContextMenu.bind(this));

        this._filterBar = new WI.FilterBar;
        this._filterBar.addEventListener(WI.FilterBar.Event.FilterDidChange, this._filterDidChange, this);
        this._filterBar.inputField.addEventListener("keydown", this._handleFilterBarInputFieldKeyDown.bind(this));
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
        this._addClassInput.spellcheck = false;
        this._addClassInput.setAttribute("placeholder", WI.UIString("Add New Class"));
        this._addClassInput.addEventListener("keypress", this._addClassInputKeyPressed.bind(this));
        this._addClassInput.addEventListener("blur", this._addClassInputBlur.bind(this));

        WI.cssManager.addEventListener(WI.CSSManager.Event.StyleSheetAdded, this._styleSheetAddedOrRemoved, this);
        WI.cssManager.addEventListener(WI.CSSManager.Event.StyleSheetRemoved, this._styleSheetAddedOrRemoved, this);

        if (this._classListContainerToggledSetting.value)
            this._classToggleButtonClicked();
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        this._updateNoForcedPseudoClassesScrollOffset();

        if (this._panel)
            this._panel.sizeDidChange();
    }

    // Private

    get _initialScrollOffset()
    {
        if (!WI.cssManager.canForcePseudoClasses())
            return 0;
        return this.domNode && this.domNode.enabledPseudoClasses.length ? 0 : WI.GeneralStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset;
    }

    _updateNoForcedPseudoClassesScrollOffset()
    {
        if (this._forcedPseudoClassContainer)
            WI.GeneralStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = this._forcedPseudoClassContainer.offsetHeight;
    }

    _showPanel()
    {
        this.contentView.addSubview(this._panel);

        let hasFilter = typeof this._panel.filterDidChange === "function";
        this.contentView.element.classList.toggle("has-filter-bar", hasFilter);
        if (this._filterBar)
            this.contentView.element.classList.toggle(WI.GeneralStyleDetailsSidebarPanel.FilterInProgressClassName, hasFilter && this._filterBar.hasActiveFilters());

        this.contentView.element.classList.toggle("supports-new-rule", typeof this._panel.newRuleButtonClicked === "function");
        this._panel.shown();
    }

    _handleForcedPseudoClassCheckboxKeydown(pseudoClass, event)
    {
        if (event.key !== "Tab")
            return;

        let pseudoClasses = WI.CSSManager.ForceablePseudoClasses;
        let index = pseudoClasses.indexOf(pseudoClass);
        if (event.shiftKey) {
            if (index > 0) {
                this._forcedPseudoClassCheckboxes[pseudoClasses[index - 1]].focus();
                event.preventDefault();
            } else {
                this._filterBar.inputField.focus();
                event.preventDefault();
            }
        } else {
            if (index < pseudoClasses.length - 1) {
                this._forcedPseudoClassCheckboxes[pseudoClasses[index + 1]].focus();
                event.preventDefault();
            } else if (this._panel.focusFirstSection) {
                this._panel.focusFirstSection();
                event.preventDefault();
            }
        }
    }

    _forcedPseudoClassCheckboxChanged(pseudoClass, event)
    {
        if (!this.domNode)
            return;

        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

        effectiveDOMNode.setPseudoClassEnabled(pseudoClass, event.target.checked);

        this._forcedPseudoClassCheckboxes[pseudoClass].focus();
    }

    _updatePseudoClassCheckboxes()
    {
        if (!this.domNode)
            return;

        let effectiveDOMNode = this.domNode.isPseudoElement() ? this.domNode.parentNode : this.domNode;
        if (!effectiveDOMNode)
            return;

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
        if (this._panel && typeof this._panel.newRuleButtonClicked === "function")
            this._panel.newRuleButtonClicked();
    }

    _newRuleButtonContextMenu(event)
    {
        if (this._panel && typeof this._panel.newRuleButtonContextMenu === "function")
            this._panel.newRuleButtonContextMenu(event);
    }

    _classToggleButtonClicked(event)
    {
        this._classToggleButton.classList.toggle("selected");
        this._classListContainer.hidden = !this._classListContainer.hidden;
        this._classListContainerToggledSetting.value = !this._classListContainer.hidden;
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
        if (!this._classListContainer || this._classListContainer.hidden)
            return;

        // Ensure that _addClassContainer is the first child of _classListContainer.
        while (this._classListContainer.children.length > 1)
            this._classListContainer.children[1].remove();

        let classes = this.domNode.getAttribute("class") || [];
        let classToggledMap = this.domNode[WI.GeneralStyleDetailsSidebarPanel.ToggledClassesSymbol];
        if (!classToggledMap)
            classToggledMap = this.domNode[WI.GeneralStyleDetailsSidebarPanel.ToggledClassesSymbol] = new Map;

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

        let classToggledMap = this.domNode[WI.GeneralStyleDetailsSidebarPanel.ToggledClassesSymbol];
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
            event.dataTransfer.setData(WI.GeneralStyleDetailsSidebarPanel.ToggledClassesDragType, className);
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
        this.contentView.element.classList.toggle(WI.GeneralStyleDetailsSidebarPanel.FilterInProgressClassName, this._filterBar.hasActiveFilters());

        this._panel.filterDidChange(this._filterBar);
    }

    _handleFilterBarInputFieldKeyDown(event)
    {
        if (event.key !== "Tab")
            return;

        if (event.shiftKey) {
            if (this._panel.focusLastSection) {
                this._panel.focusLastSection();
                event.preventDefault();
            }
        } else {
            this._forcedPseudoClassCheckboxes[WI.CSSManager.ForceablePseudoClasses[0]].focus();
            event.preventDefault();
        }
    }

    _styleSheetAddedOrRemoved()
    {
        this.needsLayout();
    }
};

WI.GeneralStyleDetailsSidebarPanel.NoForcedPseudoClassesScrollOffset = 30; // Default height of the forced pseudo classes container. Updated in sizeDidChange.
WI.GeneralStyleDetailsSidebarPanel.FilterInProgressClassName = "filter-in-progress";
WI.GeneralStyleDetailsSidebarPanel.FilterMatchingSectionHasLabelClassName = "filter-section-has-label";
WI.GeneralStyleDetailsSidebarPanel.FilterMatchSectionClassName = "filter-matching";
WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInSectionClassName = "filter-section-non-matching";
WI.GeneralStyleDetailsSidebarPanel.NoFilterMatchInPropertyClassName = "filter-property-non-matching";

WI.GeneralStyleDetailsSidebarPanel.ToggledClassesSymbol = Symbol("css-style-details-sidebar-panel-toggled-classes-symbol");
WI.GeneralStyleDetailsSidebarPanel.ToggledClassesDragType = "text/classname";
