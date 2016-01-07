/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.VisualStyleDetailsPanel = class VisualStyleDetailsPanel extends WebInspector.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, "visual", "visual", WebInspector.UIString("Styles \u2014 Visual"));

        this._currentStyle = null;
        this._forceNextStyleUpdate = false;

        this._sections = {};
        this._groups = {};
        this._keywords = {};
        this._units = {};
        this._autocompletionPropertyEditors = {};

        // These keywords, as well as the values below, are not localized as they must match the CSS spec.
        this._keywords.defaults = ["Inherit", "Initial", "Unset", "Revert"];
        this._keywords.boxModel = this._keywords.defaults.concat(["Auto"]);
        this._keywords.borderStyle = {
            basic: this._keywords.defaults.concat(["None", "Hidden", "Solid"]),
            advanced: ["Dashed", "Dotted", "Double", "Groove", "Hidden", "Inset", "None", "Outset", "Ridge"]
        };
        this._keywords.borderWidth = this._keywords.defaults.concat(["Medium", "Thick", "Thin"]);

        this._units.defaultsSansPercent = {
            basic: ["px", "em"],
            advanced: ["ch", "cm", "ex", "in", "mm", "pc", "pt", "rem", "vh", "vw", "vmax", "vmin"]
        };
        this._units.defaults = {
            basic: ["%"].concat(this._units.defaultsSansPercent.basic),
            advanced: this._units.defaultsSansPercent.advanced
        };

        // Selector Section
        this._selectorSection = new WebInspector.VisualStyleSelectorSection(this);
        this._selectorSection.addEventListener(WebInspector.VisualStyleSelectorSection.Event.SelectorChanged, this._updateSections, this);
        this._selectorSection.addEventListener(WebInspector.VisualStyleSelectorSection.Event.StyleTextChanged, this._prepareForChange, this);
        this.element.appendChild(this._selectorSection.element);

        // Layout Section
        this._generateSection("display", WebInspector.UIString("Display"));
        this._generateSection("position", WebInspector.UIString("Position"));
        this._generateSection("float", WebInspector.UIString("Float and Clear"));
        this._generateSection("dimensions", WebInspector.UIString("Dimensions"));
        this._generateSection("margin", WebInspector.UIString("Margin"));
        this._generateSection("padding", WebInspector.UIString("Padding"));
        this._generateSection("flexbox", WebInspector.UIString("Flexbox"));
        this._generateSection("alignment", WebInspector.UIString("Alignment"));

        this._sections.layout = new WebInspector.DetailsSection("layout", WebInspector.UIString("Layout"), [this._groups.display.section, this._groups.position.section, this._groups.float.section, this._groups.dimensions.section, this._groups.margin.section, this._groups.padding.section, this._groups.flexbox.section, this._groups.alignment.section]);
        this.element.appendChild(this._sections.layout.element);

        // Text Section
        this._generateSection("text-style", WebInspector.UIString("Style"));
        this._generateSection("font", WebInspector.UIString("Font"));
        this._generateSection("text-spacing", WebInspector.UIString("Spacing"));
        this._generateSection("text-shadow", WebInspector.UIString("Shadow"));

        this._sections.text = new WebInspector.DetailsSection("text", WebInspector.UIString("Text"), [this._groups.textStyle.section, this._groups.font.section, this._groups.textSpacing.section, this._groups.textShadow.section]);
        this.element.appendChild(this._sections.text.element);

        // Background Section
        this._generateSection("background-style", WebInspector.UIString("Style"));
        this._generateSection("border", WebInspector.UIString("Border"));
        this._generateSection("outline", WebInspector.UIString("Outline"));
        this._generateSection("box-shadow", WebInspector.UIString("Box Shadow"));
        this._generateSection("list-style", WebInspector.UIString("List Styles"));

        this._sections.background = new WebInspector.DetailsSection("background", WebInspector.UIString("Background"), [this._groups.backgroundStyle.section, this._groups.border.section, this._groups.outline.section, this._groups.boxShadow.section, this._groups.listStyle.section]);
        this.element.appendChild(this._sections.background.element);

        // Effects Section
        this._generateSection("transition", WebInspector.UIString("Transition"));
        this._generateSection("animation", WebInspector.UIString("Animation"));

        this._sections.effects = new WebInspector.DetailsSection("effects", WebInspector.UIString("Effects"), [this._groups.transition.section, this._groups.animation.section]);
        this.element.appendChild(this._sections.effects.element);
    }

    // Public

    refresh(significantChange)
    {
        if (significantChange || this._forceNextStyleUpdate) {
            this._selectorSection.update(this._nodeStyles);
            this._updateSections();
            this._forceNextStyleUpdate = false;
        }

        super.refresh();
    }

    // Private

    _generateSection(id, displayName)
    {
        if (!id || !displayName)
            return;

        function replaceDashWithCapital(match) {
            return match.replace("-", "").toUpperCase();
        }

        let camelCaseId = id.replace(/-\b\w/g, replaceDashWithCapital);

        function createOptionsElement() {
            let container = document.createElement("div");
            container.classList.add("visual-style-section-clear");
            container.title = WebInspector.UIString("Click to clear modified properties");
            container.addEventListener("click", this._clearModifiedSection.bind(this, camelCaseId));
            return container;
        }

        this._groups[camelCaseId] = {
            section: new WebInspector.DetailsSection(id, displayName, [], createOptionsElement.call(this)),
            properties: {}
        };

        let populateFunction = this["_populate" + camelCaseId.capitalize() + "Section"];
        populateFunction.call(this);
    }

    _prepareForChange(event)
    {
        this._forceNextStyleUpdate = true;
    }

    _updateSections(event)
    {
        this._currentStyle = this._selectorSection.currentStyle();
        if (!this._currentStyle)
            return;

        let disabled = this._currentStyle[WebInspector.VisualStyleDetailsPanel.StyleDisabledSymbol];
        this.element.classList.toggle("disabled", !!disabled);
        if (disabled)
            return;

        for (let key in this._groups)
            this._updateProperties(this._groups[key], !!event);

        for (let key in this._sections) {
            let section = this._sections[key];
            let oneSectionExpanded = false;
            for (let group of section.groups) {
                if (!group.collapsed) {
                    oneSectionExpanded = true;
                    break;
                }
            }

            section.collapsed = !oneSectionExpanded;
        }
    }

    _updateProperties(group, forceStyleUpdate)
    {
        if (!group.section)
            return;

        if (group.links) {
            for (let key in group.links)
                group.links[key].linked = false;
        }

        let initialTextList = this._initialTextList;
        if (!initialTextList)
            this._currentStyle[WebInspector.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol] = initialTextList = new WeakMap;

        let initialPropertyText = {};
        let initialPropertyTextMissing = !initialTextList.has(group);
        for (let key in group.properties) {
            let propertyEditor = group.properties[key];
            propertyEditor.update(!propertyEditor.style || forceStyleUpdate ? this._currentStyle : null);

            let value = propertyEditor.synthesizedValue;
            if (value && !propertyEditor.propertyMissing && initialPropertyTextMissing)
                initialPropertyText[key] = value;
        }

        if (initialPropertyTextMissing)
            initialTextList.set(group, initialPropertyText);

        let groupHasSetProperty = this._groupHasSetProperty(group);
        group.section.collapsed = !groupHasSetProperty && !group.section.expandedByUser;
        group.section.element.classList.toggle("has-set-property", groupHasSetProperty);
        this._sectionModified(group);

        let autocompleteCompatibleProperties = group.autocompleteCompatibleProperties;
        if (!autocompleteCompatibleProperties || !autocompleteCompatibleProperties.length)
            return;

        for (let editor of autocompleteCompatibleProperties)
            this._updateAutocompleteCompatiblePropertyEditor(editor, forceStyleUpdate);
    }

    _updateAutocompleteCompatiblePropertyEditor(editor, force)
    {
        if (!editor || (editor.hasCompletions && !force))
            return;

        editor.completions = WebInspector.CSSKeywordCompletions.forProperty(editor.propertyReferenceName);
    }

    _sectionModified(group)
    {
        group.section.element.classList.toggle("modified", this._initialPropertyTextModified(group));
        group.section.element.classList.toggle("has-set-property", this._groupHasSetProperty(group));
    }

    _clearModifiedSection(groupId)
    {
        let group = this._groups[groupId];
        group.section.element.classList.remove("modified");
        let initialPropertyTextList = this._currentStyle[WebInspector.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol].get(group);
        if (!initialPropertyTextList)
            return;

        let newStyleText = this._currentStyle.text;
        for (let key in group.properties) {
            let propertyEditor = group.properties[key];
            let initialValue = initialPropertyTextList[key] || null;
            newStyleText = propertyEditor.modifyPropertyText(newStyleText, initialValue);
            propertyEditor.resetEditorValues(initialValue);
        }

        this._currentStyle.text = newStyleText;
        group.section.element.classList.toggle("has-set-property", this._groupHasSetProperty(group));
    }

    get _initialTextList()
    {
        return this._currentStyle[WebInspector.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol];
    }

    _initialPropertyTextModified(group)
    {
        if (!group.properties)
            return false;

        let initialPropertyTextList = this._initialTextList.get(group);
        if (!initialPropertyTextList)
            return false;

        for (let key in group.properties) {
            let propertyEditor = group.properties[key];
            if (propertyEditor.propertyMissing)
                continue;

            let value = propertyEditor.synthesizedValue;
            if (value && initialPropertyTextList[key] !== value)
                return true;
        }

        return false;
    }

    _groupHasSetProperty(group)
    {
        for (let key in group.properties) {
            let propertyEditor = group.properties[key];
            let value = propertyEditor.synthesizedValue;
            if (value && !propertyEditor.propertyMissing)
                return true;
        }
        return false;
    }

    _populateSection(group, groups)
    {
        if (!group || !groups)
            return;

        group.section.groups = groups;
        for (let key in group.properties)
            group.properties[key].addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, this._sectionModified.bind(this, group));
    }

    _populateDisplaySection()
    {
        let group = this._groups.display;
        let properties = group.properties;

        let displayRow = new WebInspector.DetailsSectionRow;

        properties.display = new WebInspector.VisualStyleKeywordPicker("display", WebInspector.UIString("Type"), {
            basic: ["None", "Block", "Flex", "Inline", "Inline Block"],
            advanced: ["Compact", "Inline Flex", "Inline Table", "List Item", "Table", "Table Caption", "Table Cell", "Table Column", "Table Column Group", "Table Footer Group", "Table Header Group", "Table Row", "Table Row Group", " WAP Marquee", " WebKit Box", " WebKit Grid", " WebKit Inline Box", " WebKit Inline Grid"]
        });
        properties.visibility = new WebInspector.VisualStyleKeywordPicker("visibility", WebInspector.UIString("Visibility"), {
            basic: ["Hidden", "Visible"],
            advanced: ["Collapse"]
        });

        displayRow.element.appendChild(properties.display.element);
        displayRow.element.appendChild(properties.visibility.element);

        let sizingRow = new WebInspector.DetailsSectionRow;

        properties.boxSizing = new WebInspector.VisualStyleKeywordPicker("box-sizing", WebInspector.UIString("Sizing"), this._keywords.defaults.concat(["Border Box", "Content Box"]));
        properties.cursor = new WebInspector.VisualStyleKeywordPicker("cursor", WebInspector.UIString("Cursor"), {
            basic: ["Auto", "Default", "None", "Pointer", "Crosshair", "Text"],
            advanced: ["Context Menu", "Help", "Progress", "Wait", "Cell", "Vertical Text", "Alias", "Copy", "Move", "No Drop", "Not Allowed", "All Scroll", "Col Resize", "Row Resize", "N Resize", "E Resize", "S Resize", "W Resize", "NS Resize", "EW Resize", "NE Resize", "NW Resize", "SE Resize", "sW Resize", "NESW Resize", "NWSE Resize"]
        });

        sizingRow.element.appendChild(properties.boxSizing.element);
        sizingRow.element.appendChild(properties.cursor.element);

        let overflowRow = new WebInspector.DetailsSectionRow;

        properties.opacity = new WebInspector.VisualStyleUnitSlider("opacity", WebInspector.UIString("Opacity"));
        properties.overflow = new WebInspector.VisualStyleKeywordPicker(["overflow-x", "overflow-y"], WebInspector.UIString("Overflow"), {
            basic: ["Initial", "Unset", "Revert", "Auto", "Hidden", "Scroll", "Visible"],
            advanced: ["Marquee", "Overlay", " WebKit Paged X", " WebKit Paged Y"]
        });

        overflowRow.element.appendChild(properties.opacity.element);
        overflowRow.element.appendChild(properties.overflow.element);

        let displayGroup = new WebInspector.DetailsSectionGroup([displayRow, sizingRow, overflowRow]);
        this._populateSection(group, [displayGroup]);
    }

    _addMetricsMouseListeners(editor, mode)
    {
        function editorMouseover() {
            if (!this._currentStyle)
                return;

            if (!this._currentStyle.ownerRule) {
                WebInspector.domTreeManager.highlightDOMNode(this._currentStyle.node.id, mode);
                return;
            }

            WebInspector.domTreeManager.highlightSelector(this._currentStyle.ownerRule.selectorText, this._currentStyle.node.ownerDocument.frameIdentifier, mode);
        }

        function editorMouseout() {
            WebInspector.domTreeManager.hideDOMNodeHighlight();
        }

        editor.element.addEventListener("mouseover", editorMouseover.bind(this));
        editor.element.addEventListener("mouseout", editorMouseout);
    }

    _generateMetricSectionRows(group, prefix, allowNegatives, highlightOnHover)
    {
        let properties = group.properties;
        let links = group.links = {};

        let hasPrefix = prefix && prefix.length;
        let propertyNamePrefix = hasPrefix ? prefix + "-" : "";

        let top = hasPrefix ? prefix + "Top" : "top";
        let bottom = hasPrefix ? prefix + "Bottom" : "bottom";
        let left = hasPrefix ? prefix + "Left" : "left";
        let right = hasPrefix ? prefix + "Right" : "right";

        let vertical = new WebInspector.DetailsSectionRow;

        properties[top] = new WebInspector.VisualStyleNumberInputBox(propertyNamePrefix + "top", WebInspector.UIString("Top"), this._keywords.boxModel, this._units.defaults, allowNegatives);
        properties[bottom] = new WebInspector.VisualStyleNumberInputBox(propertyNamePrefix + "bottom", WebInspector.UIString("Bottom"), this._keywords.boxModel, this._units.defaults, allowNegatives, true);
        links["vertical"] = new WebInspector.VisualStylePropertyEditorLink([properties[top], properties[bottom]], "link-vertical");

        vertical.element.appendChild(properties[top].element);
        vertical.element.appendChild(links["vertical"].element);
        vertical.element.appendChild(properties[bottom].element);

        let horizontal = new WebInspector.DetailsSectionRow;

        properties[left] = new WebInspector.VisualStyleNumberInputBox(propertyNamePrefix + "left", WebInspector.UIString("Left"), this._keywords.boxModel, this._units.defaults, allowNegatives);
        properties[right] = new WebInspector.VisualStyleNumberInputBox(propertyNamePrefix + "right", WebInspector.UIString("Right"), this._keywords.boxModel, this._units.defaults, allowNegatives, true);
        links["horizontal"] = new WebInspector.VisualStylePropertyEditorLink([properties[left], properties[right]], "link-horizontal");

        horizontal.element.appendChild(properties[left].element);
        horizontal.element.appendChild(links["horizontal"].element);
        horizontal.element.appendChild(properties[right].element);

        let allLinkRow = new WebInspector.DetailsSectionRow;
        links["all"] = new WebInspector.VisualStylePropertyEditorLink([properties[top], properties[bottom], properties[left], properties[right]], "link-all", [links["vertical"], links["horizontal"]]);
        allLinkRow.element.appendChild(links["all"].element);

        if (highlightOnHover) {
            this._addMetricsMouseListeners(properties[top], prefix);
            this._addMetricsMouseListeners(links["vertical"], prefix);
            this._addMetricsMouseListeners(properties[bottom], prefix);
            this._addMetricsMouseListeners(links["all"], prefix);
            this._addMetricsMouseListeners(properties[left], prefix);
            this._addMetricsMouseListeners(links["horizontal"], prefix);
            this._addMetricsMouseListeners(properties[right], prefix);
        }

        return [vertical, allLinkRow, horizontal];
    }

    _populatePositionSection()
    {
        let group = this._groups.position;
        let rows = this._generateMetricSectionRows(group, null, true);
        let properties = group.properties;

        let positionType = new WebInspector.DetailsSectionRow;

        properties.position = new WebInspector.VisualStyleKeywordPicker("position", WebInspector.UIString("Type"), {
            basic: ["Static", "Relative", "Absolute", "Fixed"],
            advanced: [" WebKit Sticky"]
        });
        properties.zIndex = new WebInspector.VisualStyleNumberInputBox("z-index", WebInspector.UIString("Z-Index"), this._keywords.boxModel, null, true);

        positionType.element.appendChild(properties.position.element);
        positionType.element.appendChild(properties.zIndex.element);
        positionType.element.classList.add("visual-style-separated-row");

        rows.unshift(positionType)

        let positionGroup = new WebInspector.DetailsSectionGroup(rows);
        this._populateSection(group, [positionGroup]);
    }

    _populateFloatSection()
    {
        let group = this._groups.float;
        let properties = group.properties;

        let floatRow = new WebInspector.DetailsSectionRow;

        properties.float = new WebInspector.VisualStyleKeywordIconList("float", WebInspector.UIString("Float"), ["Left", "Right", "None"]);
        floatRow.element.appendChild(properties.float.element);

        let clearRow = new WebInspector.DetailsSectionRow;

        properties.clear = new WebInspector.VisualStyleKeywordIconList("clear", WebInspector.UIString("Clear"), ["Left", "Right", "Both", "None"]);
        clearRow.element.appendChild(properties.clear.element);

        let floatGroup = new WebInspector.DetailsSectionGroup([floatRow, clearRow]);
        this._populateSection(group, [floatGroup]);
    }

    _populateDimensionsSection()
    {
        let group = this._groups.dimensions;
        let properties = group.properties;

        let dimensionsWidth = new WebInspector.DetailsSectionRow;

        properties.width = new WebInspector.VisualStyleRelativeNumberSlider("width", WebInspector.UIString("Width"), this._keywords.boxModel, this._units.defaults);

        dimensionsWidth.element.appendChild(properties.width.element);

        let dimensionsHeight = new WebInspector.DetailsSectionRow;

        properties.height = new WebInspector.VisualStyleRelativeNumberSlider("height", WebInspector.UIString("Height"), this._keywords.boxModel, this._units.defaults, true);

        dimensionsHeight.element.appendChild(properties.height.element);

        let dimensionsProperties = [properties.width, properties.height];
        let dimensionsRegularGroup = new WebInspector.DetailsSectionGroup([dimensionsWidth, dimensionsHeight]);

        let dimensionsMinWidth = new WebInspector.DetailsSectionRow;

        properties.minWidth = new WebInspector.VisualStyleRelativeNumberSlider("min-width", WebInspector.UIString("Width"), this._keywords.boxModel, this._units.defaults);

        dimensionsMinWidth.element.appendChild(properties.minWidth.element);

        let dimensionsMinHeight = new WebInspector.DetailsSectionRow;

        properties.minHeight = new WebInspector.VisualStyleRelativeNumberSlider("min-height", WebInspector.UIString("Height"), this._keywords.boxModel, this._units.defaults);

        dimensionsMinHeight.element.appendChild(properties.minHeight.element);

        let dimensionsMinProperties = [properties.minWidth, properties.minHeight];
        let dimensionsMinGroup = new WebInspector.DetailsSectionGroup([dimensionsMinWidth, dimensionsMinHeight]);

        let dimensionsMaxKeywords = this._keywords.defaults.concat("None");
        let dimensionsMaxWidth = new WebInspector.DetailsSectionRow;

        properties.maxWidth = new WebInspector.VisualStyleRelativeNumberSlider("max-width", WebInspector.UIString("Width"), dimensionsMaxKeywords, this._units.defaults);

        dimensionsMaxWidth.element.appendChild(properties.maxWidth.element);

        let dimensionsMaxHeight = new WebInspector.DetailsSectionRow;

        properties.maxHeight = new WebInspector.VisualStyleRelativeNumberSlider("max-height", WebInspector.UIString("Height"), dimensionsMaxKeywords, this._units.defaults);

        dimensionsMaxHeight.element.appendChild(properties.maxHeight.element);

        let dimensionsMaxProperties = [properties.maxWidth, properties.maxHeight];
        let dimensionsMaxGroup = new WebInspector.DetailsSectionGroup([dimensionsMaxWidth, dimensionsMaxHeight]);

        let dimensionsTabController = new WebInspector.VisualStyleTabbedPropertiesRow({
            "default": {title: WebInspector.UIString("Default"), element: dimensionsRegularGroup.element, properties: dimensionsProperties},
            "min": {title: WebInspector.UIString("Min"), element: dimensionsMinGroup.element, properties: dimensionsMinProperties},
            "max": {title: WebInspector.UIString("Max"), element: dimensionsMaxGroup.element, properties: dimensionsMaxProperties}
        });

        let highlightMode = "content";
        this._addMetricsMouseListeners(group.properties.width, highlightMode);
        this._addMetricsMouseListeners(group.properties.height, highlightMode);
        this._addMetricsMouseListeners(group.properties.minWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.minHeight, highlightMode);
        this._addMetricsMouseListeners(group.properties.maxWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.maxHeight, highlightMode);

        let dimensionsGroup = new WebInspector.DetailsSectionGroup([dimensionsTabController, dimensionsRegularGroup, dimensionsMinGroup, dimensionsMaxGroup]);
        this._populateSection(group, [dimensionsGroup]);
    }

    _populateMarginSection()
    {
        let group = this._groups.margin;
        let rows = this._generateMetricSectionRows(group, "margin", true, true);
        let marginGroup = new WebInspector.DetailsSectionGroup(rows);
        this._populateSection(group, [marginGroup]);
    }

    _populatePaddingSection()
    {
        let group = this._groups.padding;
        let rows = this._generateMetricSectionRows(group, "padding", false, true);
        let paddingGroup = new WebInspector.DetailsSectionGroup(rows);
        this._populateSection(group, [paddingGroup]);
    }

    _populateFlexboxSection()
    {
        let group = this._groups.flexbox;
        let properties = group.properties;

        let flexOrderRow = new WebInspector.DetailsSectionRow;

        properties.order = new WebInspector.VisualStyleNumberInputBox("order", WebInspector.UIString("Order"), this._keywords.defaults);
        properties.flexBasis = new WebInspector.VisualStyleNumberInputBox("flex-basis", WebInspector.UIString("Basis"), this._keywords.boxModel, this._units.defaults, true);

        flexOrderRow.element.appendChild(properties.order.element);
        flexOrderRow.element.appendChild(properties.flexBasis.element);

        let flexSizeRow = new WebInspector.DetailsSectionRow;

        properties.flexGrow = new WebInspector.VisualStyleNumberInputBox("flex-grow", WebInspector.UIString("Grow"), this._keywords.defaults);
        properties.flexShrink = new WebInspector.VisualStyleNumberInputBox("flex-shrink", WebInspector.UIString("Shrink"), this._keywords.defaults);

        flexSizeRow.element.appendChild(properties.flexGrow.element);
        flexSizeRow.element.appendChild(properties.flexShrink.element);

        let flexFlowRow = new WebInspector.DetailsSectionRow;

        properties.flexDirection = new WebInspector.VisualStyleKeywordPicker("flex-direction", WebInspector.UIString("Direction"), this._keywords.defaults.concat(["Row", "Row Reverse", "Column", "Column Reverse"]));
        properties.flexWrap = new WebInspector.VisualStyleKeywordPicker("flex-wrap", WebInspector.UIString("Wrap"), this._keywords.defaults.concat(["Wrap", "Wrap Reverse", "Nowrap"]));

        flexFlowRow.element.appendChild(properties.flexDirection.element);
        flexFlowRow.element.appendChild(properties.flexWrap.element);

        let flexboxGroup = new WebInspector.DetailsSectionGroup([flexOrderRow, flexSizeRow, flexFlowRow]);
        this._populateSection(group, [flexboxGroup]);
    }

    _populateAlignmentSection()
    {
        let group = this._groups.alignment;
        let properties = group.properties;
        let alignmentKeywords = ["Initial", "Unset", "Revert", "Auto", "Flex Start", "Flex End", "Center", "Stretch"];
        let advancedAlignmentKeywords = ["Start", "End", "Left", "Right", "Baseline", "Last Baseline"];

        let contentRow = new WebInspector.DetailsSectionRow;
        let contentKeywords = {
            basic: alignmentKeywords.concat(["Space Between", "Space Around"]),
            advanced: advancedAlignmentKeywords.concat(["Space Evenly"])
        };

        properties.justifyContent = new WebInspector.VisualStyleKeywordPicker("justify-content", WebInspector.UIString("Horizontal"), contentKeywords);
        properties.alignContent = new WebInspector.VisualStyleKeywordPicker("align-content", WebInspector.UIString("Vertical"), contentKeywords);

        contentRow.element.appendChild(properties.justifyContent.element);
        contentRow.element.appendChild(properties.alignContent.element);

        let itemsRow = new WebInspector.DetailsSectionRow;
        let itemKeywords = {
            basic: alignmentKeywords,
            advanced: ["Self Start", "Self End"].concat(advancedAlignmentKeywords)
        };

        properties.alignItems = new WebInspector.VisualStyleKeywordPicker("align-items", WebInspector.UIString("Children"), itemKeywords);
        properties.alignSelf = new WebInspector.VisualStyleKeywordPicker("align-self", WebInspector.UIString("Self"), itemKeywords);

        itemsRow.element.appendChild(properties.alignItems.element);
        itemsRow.element.appendChild(properties.alignSelf.element);

        let alignmentGroup = new WebInspector.DetailsSectionGroup([contentRow, itemsRow]);
        this._populateSection(group, [alignmentGroup]);
    }

    _populateTextStyleSection()
    {
        let group = this._groups.textStyle;
        let properties = group.properties;

        let textAppearanceRow = new WebInspector.DetailsSectionRow;

        properties.color = new WebInspector.VisualStyleColorPicker("color", WebInspector.UIString("Color"));
        properties.textDirection = new WebInspector.VisualStyleKeywordPicker("direction", WebInspector.UIString("Direction"), this._keywords.defaults.concat(["LTR", "RTL"]));

        textAppearanceRow.element.appendChild(properties.color.element);
        textAppearanceRow.element.appendChild(properties.textDirection.element);

        let textAlignRow = new WebInspector.DetailsSectionRow;

        properties.textAlign = new WebInspector.VisualStyleKeywordIconList("text-align", WebInspector.UIString("Align"), ["Left", "Center", "Right", "Justify"]);

        textAlignRow.element.appendChild(properties.textAlign.element);

        let textTransformRow = new WebInspector.DetailsSectionRow;

        properties.textTransform = new WebInspector.VisualStyleKeywordIconList("text-transform", WebInspector.UIString("Transform"), ["Capitalize", "Uppercase", "Lowercase", "None"]);

        textTransformRow.element.appendChild(properties.textTransform.element);

        let textDecorationRow = new WebInspector.DetailsSectionRow;

        properties.textDecoration = new WebInspector.VisualStyleKeywordIconList("text-decoration", WebInspector.UIString("Decoration"), ["Underline", "Line Through", "Overline", "None"]);

        textDecorationRow.element.appendChild(properties.textDecoration.element);

        group.autocompleteCompatibleProperties = [properties.color];

        let textStyleGroup = new WebInspector.DetailsSectionGroup([textAppearanceRow, textAlignRow, textTransformRow, textDecorationRow]);
        this._populateSection(group, [textStyleGroup]);
    }

    _populateFontSection()
    {
        let group = this._groups.font;
        let properties = group.properties;

        let fontFamilyRow = new WebInspector.DetailsSectionRow;

        properties.fontFamily = new WebInspector.VisualStyleFontFamilyListEditor("font-family", WebInspector.UIString("Family"));

        fontFamilyRow.element.appendChild(properties.fontFamily.element);

        let fontSizeRow = new WebInspector.DetailsSectionRow;

        properties.fontSize = new WebInspector.VisualStyleNumberInputBox("font-size", WebInspector.UIString("Size"), this._keywords.defaults.concat(["Larger", "XX Large", "X Large", "Large", "Medium", "Small", "X Small", "XX Small", "Smaller"]), this._units.defaults);

        properties.fontWeight = new WebInspector.VisualStyleKeywordPicker("font-weight", WebInspector.UIString("Weight"), {
            basic: this._keywords.defaults.concat(["Bolder", "Bold", "Normal", "Lighter"]),
            advanced: ["100", "200", "300", "400", "500", "600", "700", "800", "900"]
        });

        fontSizeRow.element.appendChild(properties.fontSize.element);
        fontSizeRow.element.appendChild(properties.fontWeight.element);

        let fontStyleRow = new WebInspector.DetailsSectionRow;

        properties.fontStyle = new WebInspector.VisualStyleKeywordIconList("font-style", WebInspector.UIString("Style"), ["Italic", "Normal"]);
        properties.fontVariant = new WebInspector.VisualStyleKeywordCheckbox("font-variant", WebInspector.UIString("Variant"), "Small Caps")

        fontStyleRow.element.appendChild(properties.fontStyle.element);
        fontStyleRow.element.appendChild(properties.fontVariant.element);

        group.autocompleteCompatibleProperties = [properties.fontFamily];

        let fontGroup = new WebInspector.DetailsSectionGroup([fontFamilyRow, fontSizeRow, fontStyleRow]);
        this._populateSection(group, [fontGroup]);
    }

    _populateTextSpacingSection()
    {
        let group = this._groups.textSpacing;
        let properties = group.properties;

        let defaultTextKeywords = this._keywords.defaults.concat(["Normal"]);

        let textLayoutRow = new WebInspector.DetailsSectionRow;

        properties.lineHeight = new WebInspector.VisualStyleNumberInputBox("line-height", WebInspector.UIString("Height"), defaultTextKeywords, this._units.defaults);
        properties.verticalAlign = new WebInspector.VisualStyleNumberInputBox("vertical-align", WebInspector.UIString("Align"), ["Baseline", "Bottom"].concat(this._keywords.defaults, ["Middle", "Sub", "Super", "Text Bottom", "Text Top", "Top"]), this._units.defaults);

        textLayoutRow.element.appendChild(properties.lineHeight.element);
        textLayoutRow.element.appendChild(properties.verticalAlign.element);

        let textSpacingRow = new WebInspector.DetailsSectionRow;

        properties.letterSpacing = new WebInspector.VisualStyleNumberInputBox("letter-spacing", WebInspector.UIString("Letter"), defaultTextKeywords, this._units.defaults);
        properties.wordSpacing = new WebInspector.VisualStyleNumberInputBox("word-spacing", WebInspector.UIString("Word"), defaultTextKeywords, this._units.defaults);

        textSpacingRow.element.appendChild(properties.letterSpacing.element);
        textSpacingRow.element.appendChild(properties.wordSpacing.element);

        let textWhitespaceRow = new WebInspector.DetailsSectionRow;

        properties.textIndent = new WebInspector.VisualStyleNumberInputBox("text-indent", WebInspector.UIString("Indent"), this._keywords.defaults, this._units.defaults);
        properties.whiteSpace = new WebInspector.VisualStyleKeywordPicker("white-space", WebInspector.UIString("Whitespace"), this._keywords.defaults.concat(["Normal", "Nowrap", "Pre", "Pre Line", "Pre Wrap"]));

        textWhitespaceRow.element.appendChild(properties.textIndent.element);
        textWhitespaceRow.element.appendChild(properties.whiteSpace.element);

        let textSpacingGroup = new WebInspector.DetailsSectionGroup([textLayoutRow, textSpacingRow, textWhitespaceRow]);
        this._populateSection(group, [textSpacingGroup]);
    }

    _populateTextShadowSection()
    {
        let group = this._groups.textShadow;
        let properties = group.properties;

        let textShadowSizing = new WebInspector.DetailsSectionRow;

        let textShadowH = new WebInspector.VisualStyleNumberInputBox("text-shadow", WebInspector.UIString("Horizontal"), null, this._units.defaultsSansPercent);
        let textShadowV = new WebInspector.VisualStyleNumberInputBox("text-shadow", WebInspector.UIString("Vertical"), null, this._units.defaultsSansPercent);

        textShadowSizing.element.appendChild(textShadowH.element);
        textShadowSizing.element.appendChild(textShadowV.element);

        let textShadowStyle = new WebInspector.DetailsSectionRow;

        let textShadowColor = new WebInspector.VisualStyleColorPicker("text-shadow", WebInspector.UIString("Color"));
        let textShadowBlur = new WebInspector.VisualStyleNumberInputBox("text-shadow", WebInspector.UIString("Blur"), null, this._units.defaultsSansPercent);
        textShadowBlur.optionalProperty = true;

        textShadowStyle.element.appendChild(textShadowColor.element);
        textShadowStyle.element.appendChild(textShadowBlur.element);

        properties.textShadow = new WebInspector.VisualStylePropertyCombiner("text-shadow", [textShadowH, textShadowV, textShadowBlur, textShadowColor]);

        group.autocompleteCompatibleProperties = [textShadowColor];

        let textShadowGroup = new WebInspector.DetailsSectionGroup([textShadowSizing, textShadowStyle]);
        this._populateSection(group, [textShadowGroup]);
    }

    _populateBackgroundStyleSection()
    {
        let group = this._groups.backgroundStyle;
        let properties = group.properties;

        let backgroundStyleRow = new WebInspector.DetailsSectionRow;

        properties.backgroundColor = new WebInspector.VisualStyleColorPicker("background-color", WebInspector.UIString("Color"));
        properties.backgroundBlendMode = new WebInspector.VisualStyleKeywordPicker("background-blend-mode", WebInspector.UIString("Blend"), ["Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color", "Color Dodge", "Saturation", "Luminosity"]);

        backgroundStyleRow.element.appendChild(properties.backgroundColor.element);
        backgroundStyleRow.element.appendChild(properties.backgroundBlendMode.element);

        let backgroundClipRow = new WebInspector.DetailsSectionRow;

        let backgroundClipKeywords = ["Initial", "Border Box", "Padding Box", "Content Box"];
        properties.backgroundClip = new WebInspector.VisualStyleKeywordPicker("background-clip", WebInspector.UIString("Clip"), backgroundClipKeywords);
        properties.backgroundOrigin = new WebInspector.VisualStyleKeywordPicker("background-origin", WebInspector.UIString("Origin"), backgroundClipKeywords);

        backgroundClipRow.element.appendChild(properties.backgroundClip.element);
        backgroundClipRow.element.appendChild(properties.backgroundOrigin.element);

        let backgroundSizeRow = new WebInspector.DetailsSectionRow;

        let backgroundSizeKeywords = this._keywords.boxModel.concat(["Contain", "Cover"]);
        let backgroundSizeX = new WebInspector.VisualStyleNumberInputBox("background-size", WebInspector.UIString("Width"), backgroundSizeKeywords, this._units.defaults);
        backgroundSizeX.masterProperty = true;
        let backgroundSizeY = new WebInspector.VisualStyleNumberInputBox("background-size", WebInspector.UIString("Height"), backgroundSizeKeywords, this._units.defaults);
        backgroundSizeY.masterProperty = true;

        properties.backgroundSize = new WebInspector.VisualStylePropertyCombiner("background-size", [backgroundSizeX, backgroundSizeY]);

        backgroundSizeRow.element.appendChild(backgroundSizeX.element);
        backgroundSizeRow.element.appendChild(backgroundSizeY.element);

        let backgroundRow = new WebInspector.DetailsSectionRow;

        properties.background = new WebInspector.VisualStyleCommaSeparatedKeywordEditor("background", null, {
            "background-image": "none",
            "background-position": "0% 0%",
            "background-repeat": "repeat",
            "background-attachment": "scroll",
        });

        backgroundRow.element.appendChild(properties.background.element);

        let backgroundImageRow = new WebInspector.DetailsSectionRow;

        let backgroundImage = new WebInspector.VisualStyleBackgroundPicker("background-image", WebInspector.UIString("Type"), this._keywords.defaults.concat(["None"]));

        backgroundImageRow.element.appendChild(backgroundImage.element);

        let backgroundPositionRow = new WebInspector.DetailsSectionRow;

        let backgroundPositionX = new WebInspector.VisualStyleNumberInputBox("background-position", WebInspector.UIString("Position X"), ["Center", "Left", "Right"], this._units.defaults);
        backgroundPositionX.optionalProperty = true;
        let backgroundPositionY = new WebInspector.VisualStyleNumberInputBox("background-position", WebInspector.UIString("Position Y"), ["Bottom", "Center", "Top"], this._units.defaults);
        backgroundPositionY.optionalProperty = true;

        backgroundPositionRow.element.appendChild(backgroundPositionX.element);
        backgroundPositionRow.element.appendChild(backgroundPositionY.element);

        let backgroundRepeatRow = new WebInspector.DetailsSectionRow;

        let backgroundRepeat = new WebInspector.VisualStyleKeywordPicker("background-repeat", WebInspector.UIString("Repeat"), this._keywords.defaults.concat(["No Repeat", "Repeat", "Repeat X", "Repeat Y"]));
        backgroundRepeat.optionalProperty = true;
        let backgroundAttachment = new WebInspector.VisualStyleKeywordPicker("background-attachment", WebInspector.UIString("Attach"), this._keywords.defaults.concat(["Fixed", "Local", "Scroll"]));
        backgroundAttachment.optionalProperty = true;

        backgroundRepeatRow.element.appendChild(backgroundRepeat.element);
        backgroundRepeatRow.element.appendChild(backgroundAttachment.element);

        let backgroundProperties = [backgroundImage, backgroundPositionX, backgroundPositionY, backgroundRepeat, backgroundAttachment];
        let backgroundPropertyCombiner = new WebInspector.VisualStylePropertyCombiner("background", backgroundProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, backgroundPropertyCombiner, backgroundProperties);
        properties.background.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.background, backgroundPropertyCombiner);
        backgroundPropertyCombiner.addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(backgroundPropertyCombiner);
        properties.background.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [properties.backgroundColor];

        let backgroundStyleGroup = new WebInspector.DetailsSectionGroup([backgroundStyleRow, backgroundClipRow, backgroundSizeRow, backgroundRow, backgroundImageRow, backgroundPositionRow, backgroundRepeatRow]);
        this._populateSection(group, [backgroundStyleGroup]);
    }

    _populateBorderSection()
    {
        let group = this._groups.border;
        let properties = group.properties;

        let borderAllSize = new WebInspector.DetailsSectionRow;

        properties.borderStyle = new WebInspector.VisualStyleKeywordPicker(["border-top-style", "border-right-style", "border-bottom-style", "border-left-style"] , WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.borderStyle.propertyReferenceName = "border-style";
        properties.borderWidth = new WebInspector.VisualStyleNumberInputBox(["border-top-width", "border-right-width", "border-bottom-width", "border-left-width"], WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);
        properties.borderWidth.propertyReferenceName = "border-width";

        borderAllSize.element.appendChild(properties.borderStyle.element);
        borderAllSize.element.appendChild(properties.borderWidth.element);

        let borderAllStyle = new WebInspector.DetailsSectionRow;

        properties.borderColor = new WebInspector.VisualStyleColorPicker(["border-top-color", "border-right-color", "border-bottom-color", "border-left-color"], WebInspector.UIString("Color"));
        properties.borderColor.propertyReferenceName = "border-color";
        properties.borderRadius = new WebInspector.VisualStyleNumberInputBox(["border-top-left-radius", "border-top-right-radius", "border-bottom-left-radius", "border-bottom-right-radius"], WebInspector.UIString("Radius"), this._keywords.defaults, this._units.defaults);
        properties.borderRadius.propertyReferenceName = "border-radius";

        borderAllStyle.element.appendChild(properties.borderColor.element);
        borderAllStyle.element.appendChild(properties.borderRadius.element);

        let borderAllProperties = [properties.borderStyle, properties.borderWidth, properties.borderColor, properties.borderRadius];
        let borderAllGroup = new WebInspector.DetailsSectionGroup([borderAllSize, borderAllStyle]);

        let borderTopSize = new WebInspector.DetailsSectionRow;

        properties.borderTopStyle = new WebInspector.VisualStyleKeywordPicker("border-top-style", WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.borderTopWidth = new WebInspector.VisualStyleNumberInputBox("border-top-width", WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderTopSize.element.appendChild(properties.borderTopStyle.element);
        borderTopSize.element.appendChild(properties.borderTopWidth.element);

        let borderTopStyle = new WebInspector.DetailsSectionRow;

        properties.borderTopColor = new WebInspector.VisualStyleColorPicker("border-top-color", WebInspector.UIString("Color"));
        properties.borderTopRadius = new WebInspector.VisualStyleNumberInputBox(["border-top-left-radius", "border-top-right-radius"], WebInspector.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderTopStyle.element.appendChild(properties.borderTopColor.element);
        borderTopStyle.element.appendChild(properties.borderTopRadius.element);

        let borderTopProperties = [properties.borderTopStyle, properties.borderTopWidth, properties.borderTopColor, properties.borderTopRadius];
        let borderTopGroup = new WebInspector.DetailsSectionGroup([borderTopSize, borderTopStyle]);

        let borderRightSize = new WebInspector.DetailsSectionRow;

        properties.borderRightStyle = new WebInspector.VisualStyleKeywordPicker("border-right-style", WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.borderRightWidth = new WebInspector.VisualStyleNumberInputBox("border-right-width", WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderRightSize.element.appendChild(properties.borderRightStyle.element);
        borderRightSize.element.appendChild(properties.borderRightWidth.element);

        let borderRightStyle = new WebInspector.DetailsSectionRow;

        properties.borderRightColor = new WebInspector.VisualStyleColorPicker("border-right-color", WebInspector.UIString("Color"));
        properties.borderRightRadius = new WebInspector.VisualStyleNumberInputBox(["border-top-right-radius", "border-bottom-right-radius"], WebInspector.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderRightStyle.element.appendChild(properties.borderRightColor.element);
        borderRightStyle.element.appendChild(properties.borderRightRadius.element);

        let borderRightProperties = [properties.borderRightStyle, properties.borderRightWidth, properties.borderRightColor, properties.borderRightRadius];
        let borderRightGroup = new WebInspector.DetailsSectionGroup([borderRightSize, borderRightStyle]);

        let borderBottomSize = new WebInspector.DetailsSectionRow;

        properties.borderBottomStyle = new WebInspector.VisualStyleKeywordPicker("border-bottom-style", WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.borderBottomWidth = new WebInspector.VisualStyleNumberInputBox("border-bottom-width", WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderBottomSize.element.appendChild(properties.borderBottomStyle.element);
        borderBottomSize.element.appendChild(properties.borderBottomWidth.element);

        let borderBottomStyle = new WebInspector.DetailsSectionRow;

        properties.borderBottomColor = new WebInspector.VisualStyleColorPicker("border-bottom-color", WebInspector.UIString("Color"));
        properties.borderBottomRadius = new WebInspector.VisualStyleNumberInputBox(["border-bottom-left-radius", "border-bottom-right-radius"], WebInspector.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderBottomStyle.element.appendChild(properties.borderBottomColor.element);
        borderBottomStyle.element.appendChild(properties.borderBottomRadius.element);

        let borderBottomProperties = [properties.borderBottomStyle, properties.borderBottomWidth, properties.borderBottomColor, properties.borderBottomRadius];
        let borderBottomGroup = new WebInspector.DetailsSectionGroup([borderBottomSize, borderBottomStyle]);

        let borderLeftSize = new WebInspector.DetailsSectionRow;

        properties.borderLeftStyle = new WebInspector.VisualStyleKeywordPicker("border-left-style", WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.borderLeftWidth = new WebInspector.VisualStyleNumberInputBox("border-left-width", WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderLeftSize.element.appendChild(properties.borderLeftStyle.element);
        borderLeftSize.element.appendChild(properties.borderLeftWidth.element);

        let borderLeftStyle = new WebInspector.DetailsSectionRow;

        properties.borderLeftColor = new WebInspector.VisualStyleColorPicker("border-left-color", WebInspector.UIString("Color"));
        properties.borderLeftRadius = new WebInspector.VisualStyleNumberInputBox(["border-top-left-radius", "border-bottom-left-radius"], WebInspector.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderLeftStyle.element.appendChild(properties.borderLeftColor.element);
        borderLeftStyle.element.appendChild(properties.borderLeftRadius.element);

        let borderLeftProperties = [properties.borderLeftStyle, properties.borderLeftWidth, properties.borderLeftColor, properties.borderLeftRadius];
        let borderLeftGroup = new WebInspector.DetailsSectionGroup([borderLeftSize, borderLeftStyle]);

        let borderTabController = new WebInspector.VisualStyleTabbedPropertiesRow({
            "all": {title: WebInspector.UIString("All"), element: borderAllGroup.element, properties: borderAllProperties},
            "top": {title: WebInspector.UIString("Top"), element: borderTopGroup.element, properties: borderTopProperties},
            "right": {title: WebInspector.UIString("Right"), element: borderRightGroup.element, properties: borderRightProperties},
            "bottom": {title: WebInspector.UIString("Bottom"), element: borderBottomGroup.element, properties: borderBottomProperties},
            "left": {title: WebInspector.UIString("Left"), element: borderLeftGroup.element, properties: borderLeftProperties}
        });

        let highlightMode = "border";
        this._addMetricsMouseListeners(group.properties.borderWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderTopWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderBottomWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderLeftWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderRightWidth, highlightMode);

        let borderGroup = new WebInspector.DetailsSectionGroup([borderTabController, borderAllGroup, borderTopGroup, borderRightGroup, borderBottomGroup, borderLeftGroup]);

        let borderImageSourceRow = new WebInspector.DetailsSectionRow;

        properties.borderImageSource = new WebInspector.VisualStyleURLInput("border-image-source", WebInspector.UIString("Image"), this._keywords.defaults.concat(["None"]));

        borderImageSourceRow.element.appendChild(properties.borderImageSource.element);

        let borderImageRepeatRow = new WebInspector.DetailsSectionRow;

        let borderImageSliceFill = new WebInspector.VisualStyleKeywordCheckbox("border-image-slice", WebInspector.UIString("Fill"), "Fill");
        borderImageSliceFill.optionalProperty = true;
        properties.borderImageRepeat = new WebInspector.VisualStyleKeywordPicker("border-image-repeat", WebInspector.UIString("Repeat"), this._keywords.defaults.concat(["Stretch", "Repeat", "Round", "Space"]));

        borderImageRepeatRow.element.appendChild(borderImageSliceFill.element);
        borderImageRepeatRow.element.appendChild(properties.borderImageRepeat.element);

        function generateBorderImagePropertyEditors(propertyName, keywords, units) {
            let vertical = new WebInspector.DetailsSectionRow;

            let top = new WebInspector.VisualStyleNumberInputBox(propertyName, WebInspector.UIString("Top"), keywords, units);
            top.masterProperty = true;
            let bottom = new WebInspector.VisualStyleNumberInputBox(propertyName, WebInspector.UIString("Bottom"), keywords, units);
            bottom.masterProperty = true;

            vertical.element.appendChild(top.element);
            vertical.element.appendChild(bottom.element);

            let horizontal = new WebInspector.DetailsSectionRow;

            let left = new WebInspector.VisualStyleNumberInputBox(propertyName, WebInspector.UIString("Left"), keywords, units);
            left.masterProperty = true;
            let right = new WebInspector.VisualStyleNumberInputBox(propertyName, WebInspector.UIString("Right"), keywords, units);
            right.masterProperty = true;

            horizontal.element.appendChild(left.element);
            horizontal.element.appendChild(right.element);

            return {group: new WebInspector.DetailsSectionGroup([vertical, horizontal]), properties: [top, bottom, left, right]};
        }

        let nonKeywordUnits = [WebInspector.UIString("Number")];

        let borderImageUnits = this._units.defaults;
        borderImageUnits.basic = nonKeywordUnits.concat(borderImageUnits.basic);
        let borderImageWidth = generateBorderImagePropertyEditors("border-image-width", this._keywords.boxModel, borderImageUnits);
        properties.borderImageWidth = new WebInspector.VisualStylePropertyCombiner("border-image-width", borderImageWidth.properties, true);

        let borderOutsetUnits = this._units.defaultsSansPercent;
        borderOutsetUnits.basic = nonKeywordUnits.concat(borderOutsetUnits.basic);
        let borderImageOutset = generateBorderImagePropertyEditors("border-image-outset", this._keywords.defaults, borderOutsetUnits);
        properties.borderImageOutset = new WebInspector.VisualStylePropertyCombiner("border-image-outset", borderImageOutset.properties, true);

        let borderImageSlice = generateBorderImagePropertyEditors("border-image-slice", this._keywords.defaults, ["%"].concat(nonKeywordUnits));
        borderImageSlice.properties.push(borderImageSliceFill);
        properties.borderImageSlice = new WebInspector.VisualStylePropertyCombiner("border-image-slice", borderImageSlice.properties, true);

        let borderImagePropertiesTabController = new WebInspector.VisualStyleTabbedPropertiesRow({
            "width": {title: WebInspector.UIString("Width"), element: borderImageWidth.group.element, properties: [properties.borderImageWidth]},
            "outset": {title: WebInspector.UIString("Outset"), element: borderImageOutset.group.element, properties: [properties.borderImageOutset]},
            "slice": {title: WebInspector.UIString("Slice"), element: borderImageSlice.group.element, properties: [properties.borderImageSlice]}
        });

        let borderImageGroup = new WebInspector.DetailsSectionGroup([borderImageSourceRow, borderImageRepeatRow, borderImagePropertiesTabController, borderImageWidth.group, borderImageOutset.group, borderImageSlice.group]);

        group.autocompleteCompatibleProperties = [properties.borderColor, properties.borderTopColor, properties.borderBottomColor, properties.borderLeftColor, properties.borderRightColor];

        this._populateSection(group, [borderGroup, borderImageGroup]);
    }

    _populateOutlineSection()
    {
        let group = this._groups.outline;
        let properties = group.properties;

        let outlineSizeRow = new WebInspector.DetailsSectionRow;

        properties.outlineWidth = new WebInspector.VisualStyleNumberInputBox("outline-width", WebInspector.UIString("Width"), this._keywords.borderWidth, this._units.defaults);
        properties.outlineOffset = new WebInspector.VisualStyleNumberInputBox("outline-offset", WebInspector.UIString("Offset"), this._keywords.defaults, this._units.defaults, true);

        outlineSizeRow.element.appendChild(properties.outlineWidth.element);
        outlineSizeRow.element.appendChild(properties.outlineOffset.element);

        let outlineStyleRow = new WebInspector.DetailsSectionRow;

        properties.outlineStyle = new WebInspector.VisualStyleKeywordPicker("outline-style" , WebInspector.UIString("Style"), this._keywords.borderStyle);
        properties.outlineColor = new WebInspector.VisualStyleColorPicker("outline-color", WebInspector.UIString("Color"));

        outlineStyleRow.element.appendChild(properties.outlineStyle.element);
        outlineStyleRow.element.appendChild(properties.outlineColor.element);

        group.autocompleteCompatibleProperties = [properties.outlineColor];

        let outlineGroup = new WebInspector.DetailsSectionGroup([outlineSizeRow, outlineStyleRow]);
        this._populateSection(group, [outlineGroup]);
    }

    _populateBoxShadowSection()
    {
        let group = this._groups.boxShadow;
        let properties = group.properties;

        let boxShadowRow = new WebInspector.DetailsSectionRow;

        properties.boxShadow = new WebInspector.VisualStyleCommaSeparatedKeywordEditor("box-shadow");

        boxShadowRow.element.appendChild(properties.boxShadow.element);

        let boxShadowHRow = new WebInspector.DetailsSectionRow;

        let boxShadowH = new WebInspector.VisualStyleRelativeNumberSlider("box-shadow", WebInspector.UIString("Left"), null, this._units.defaultsSansPercent, true);

        boxShadowHRow.element.appendChild(boxShadowH.element);

        let boxShadowVRow = new WebInspector.DetailsSectionRow;

        let boxShadowV = new WebInspector.VisualStyleRelativeNumberSlider("box-shadow", WebInspector.UIString("Top"), null, this._units.defaultsSansPercent, true);

        boxShadowVRow.element.appendChild(boxShadowV.element);

        let boxShadowBlurRow = new WebInspector.DetailsSectionRow;

        let boxShadowBlur = new WebInspector.VisualStyleNumberInputBox("box-shadow", WebInspector.UIString("Blur"), null, this._units.defaultsSansPercent);
        boxShadowBlur.optionalProperty = true;
        let boxShadowSpread = new WebInspector.VisualStyleNumberInputBox("box-shadow", WebInspector.UIString("Spread"), null, this._units.defaultsSansPercent, true);
        boxShadowSpread.optionalProperty = true;

        boxShadowBlurRow.element.appendChild(boxShadowBlur.element);
        boxShadowBlurRow.element.appendChild(boxShadowSpread.element);

        let boxShadowColorRow = new WebInspector.DetailsSectionRow;

        let boxShadowColor = new WebInspector.VisualStyleColorPicker("box-shadow", WebInspector.UIString("Color"));
        let boxShadowInset = new WebInspector.VisualStyleKeywordCheckbox("box-shadow", WebInspector.UIString("Inset"), "Inset");
        boxShadowInset.optionalProperty = true;

        boxShadowColorRow.element.appendChild(boxShadowColor.element);
        boxShadowColorRow.element.appendChild(boxShadowInset.element);

        let boxShadowProperties = [boxShadowH, boxShadowV, boxShadowBlur, boxShadowSpread, boxShadowColor, boxShadowInset];
        let boxShadowPropertyCombiner = new WebInspector.VisualStylePropertyCombiner("box-shadow", boxShadowProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, boxShadowPropertyCombiner, boxShadowProperties);
        properties.boxShadow.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.boxShadow, boxShadowPropertyCombiner);
        boxShadowPropertyCombiner.addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(boxShadowPropertyCombiner);
        properties.boxShadow.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [boxShadowColor];

        let boxShadow = new WebInspector.DetailsSectionGroup([boxShadowRow, boxShadowHRow, boxShadowVRow, boxShadowBlurRow, boxShadowColorRow]);
        this._populateSection(group, [boxShadow]);
    }

    _populateListStyleSection()
    {
        let group = this._groups.listStyle;
        let properties = group.properties;

        let listStyleTypeRow = new WebInspector.DetailsSectionRow;

        properties.listStyleType = new WebInspector.VisualStyleKeywordPicker("list-style-type", WebInspector.UIString("Type"), {
            basic: this._keywords.defaults.concat(["None", "Circle", "Disc", "Square", "Decimal", "Lower Alpha", "Upper Alpha", "Lower Roman", "Upper Roman"]),
            advanced: ["Decimal Leading Zero", "Asterisks", "Footnotes", "Binary", "Octal", "Lower Hexadecimal", "Upper Hexadecimal", "Lower Latin", "Upper Latin", "Lower Greek", "Upper Greek", "Arabic Indic", "Hebrew", "Hiragana", "Katakana", "Hiragana Iroha", "Katakana Iroha", "CJK Earthly Branch", "CJK Heavenly Stem", "CJK Ideographic", "Bengali", "Cambodian", "Khmer", "Devanagari", "Gujarati", "Gurmukhi", "Kannada", "Lao", "Malayalam", "Mongolian", "Myanmar", "Oriya", "Persian", "Urdu", "Telugu", "Armenian", "Lower Armenian", "Upper Armenian", "Georgian", "Tibetan", "Thai", "Afar", "Hangul Consonant", "Hangul", "Lower Norwegian", "Upper Norwegian", "Ethiopic", "Ethiopic Halehame Gez", "Ethiopic Halehame Aa Et", "Ethiopic Halehame Aa Er", "Oromo", "Ethiopic Halehame Om Et", "Sidama", "Ethiopic Halehame Sid Et", "Somali", "Ethiopic Halehame So Et", "Amharic", "Ethiopic Halehame Am Et", "Tigre", "Ethiopic Halehame Tig", "Tigrinya Er", "Ethiopic Halehame Ti Er", "Tigrinya Et", "Ethiopic Halehame Ti Et", "Ethiopic Abegede", "Ethiopic Abegede Gez", "Amharic Abegede", "Ethiopic Abegede Am Et", "Tigrinya Er Abegede", "Ethiopic Abegede Ti Er", "Tigrinya Et Abegede", "Ethiopic Abegede Ti Et"]
        });

        properties.listStylePosition = new WebInspector.VisualStyleKeywordIconList("list-style-position", WebInspector.UIString("Position"), ["Outside", "Inside", "Initial"]);

        listStyleTypeRow.element.appendChild(properties.listStyleType.element);
        listStyleTypeRow.element.appendChild(properties.listStylePosition.element);

        let listStyleImageRow = new WebInspector.DetailsSectionRow;

        properties.listStyleImage = new WebInspector.VisualStyleURLInput("list-style-image", WebInspector.UIString("Image"), this._keywords.defaults.concat(["None"]));

        listStyleImageRow.element.appendChild(properties.listStyleImage.element);

        let listStyle = new WebInspector.DetailsSectionGroup([listStyleTypeRow, listStyleImageRow]);
        this._populateSection(group, [listStyle]);
    }

    _populateTransitionSection()
    {
        let group = this._groups.transition;
        let properties = group.properties;

        let transitionRow = new WebInspector.DetailsSectionRow;

        properties.transition = new WebInspector.VisualStyleCommaSeparatedKeywordEditor("transition", null, {
            "transition-property": "all",
            "transition-timing-function": "ease",
            "transition-duration": "0",
            "transition-delay": "0"
        });

        transitionRow.element.appendChild(properties.transition.element);

        let transitionPropertyRow = new WebInspector.DetailsSectionRow;

        let transitionProperty = new WebInspector.VisualStylePropertyNameInput("transition-property", WebInspector.UIString("Property"));
        let transitionTiming = new WebInspector.VisualStyleTimingEditor("transition-timing-function", WebInspector.UIString("Timing"), ["Linear", "Ease", "Ease In", "Ease Out", "Ease In Out"]);

        transitionPropertyRow.element.appendChild(transitionProperty.element);
        transitionPropertyRow.element.appendChild(transitionTiming.element);

        let transitionDurationRow = new WebInspector.DetailsSectionRow;

        let transitionTimeKeywords = ["s", "ms"];
        let transitionDuration = new WebInspector.VisualStyleNumberInputBox("transition-duration", WebInspector.UIString("Duration"), null, transitionTimeKeywords);
        let transitionDelay = new WebInspector.VisualStyleNumberInputBox("transition-delay", WebInspector.UIString("Delay"), null, transitionTimeKeywords);
        transitionDelay.optionalProperty = true;

        transitionDurationRow.element.appendChild(transitionDuration.element);
        transitionDurationRow.element.appendChild(transitionDelay.element);

        let transitionProperties = [transitionProperty, transitionTiming, transitionDuration, transitionDelay];
        let transitionPropertyCombiner = new WebInspector.VisualStylePropertyCombiner("transition", transitionProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, transitionPropertyCombiner, transitionProperties);
        properties.transition.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.transition, transitionPropertyCombiner);
        transitionPropertyCombiner.addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(transitionPropertyCombiner);
        properties.transition.addEventListener(WebInspector.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [transitionProperty];

        let transitionGroup = new WebInspector.DetailsSectionGroup([transitionRow, transitionPropertyRow, transitionDurationRow]);
        this._populateSection(group, [transitionGroup]);
    }

    _populateAnimationSection()
    {
        let group = this._groups.animation;
        let properties = group.properties;

        let animationNameRow = new WebInspector.DetailsSectionRow;

        properties.animationName = new WebInspector.VisualStyleBasicInput("animation-name", WebInspector.UIString("Name"), WebInspector.UIString("Enter the name of a Keyframe"));

        animationNameRow.element.appendChild(properties.animationName.element);

        let animationTimingRow = new WebInspector.DetailsSectionRow;

        properties.animationTiming = new WebInspector.VisualStyleTimingEditor("animation-timing-function", WebInspector.UIString("Timing"), ["Linear", "Ease", "Ease In", "Ease Out", "Ease In Out"]);
        properties.animationIterationCount = new WebInspector.VisualStyleNumberInputBox("animation-iteration-count", WebInspector.UIString("Iterations"), this._keywords.defaults.concat(["Infinite"]), null);

        animationTimingRow.element.appendChild(properties.animationTiming.element);
        animationTimingRow.element.appendChild(properties.animationIterationCount.element);

        let animationDurationRow = new WebInspector.DetailsSectionRow;

        let animationTimeKeywords = ["s", "ms"];
        properties.animationDuration = new WebInspector.VisualStyleNumberInputBox("animation-duration", WebInspector.UIString("Duration"), null, animationTimeKeywords);
        properties.animationDelay = new WebInspector.VisualStyleNumberInputBox("animation-delay", WebInspector.UIString("Delay"), null, animationTimeKeywords);

        animationDurationRow.element.appendChild(properties.animationDuration.element);
        animationDurationRow.element.appendChild(properties.animationDelay.element);

        let animationDirectionRow = new WebInspector.DetailsSectionRow;

        properties.animationDirection = new WebInspector.VisualStyleKeywordPicker("animation-direction", WebInspector.UIString("Direction"), {
            basic: this._keywords.defaults.concat(["Normal", "Reverse"]),
            advanced: ["Alternate", "Alternate Reverse"]
        });
        properties.animationFillMode = new WebInspector.VisualStyleKeywordPicker("animation-fill-mode", WebInspector.UIString("Fill Mode"), this._keywords.defaults.concat(["None", "Forwards", "Backwards", "Both"]));

        animationDirectionRow.element.appendChild(properties.animationDirection.element);
        animationDirectionRow.element.appendChild(properties.animationFillMode.element);

        let animationStateRow = new WebInspector.DetailsSectionRow;

        properties.animationPlayState = new WebInspector.VisualStyleKeywordIconList("animation-play-state", WebInspector.UIString("State"), ["Running", "Paused", "Initial"]);

        animationStateRow.element.appendChild(properties.animationPlayState.element);

        let animationGroup = new WebInspector.DetailsSectionGroup([animationNameRow, animationTimingRow, animationDurationRow, animationDirectionRow, animationStateRow]);
        this._populateSection(group, [animationGroup]);
    }

    _noRemainingCommaSeparatedEditorItems(propertyCombiner, propertyEditors)
    {
        if (!propertyCombiner || !propertyEditors)
            return;

        propertyCombiner.updateValuesFromText("");
        for (let editor of propertyEditors)
            editor.disabled = true;
    }

    _selectedCommaSeparatedEditorItemValueChanged(propertyEditor, propertyCombiner)
    {
        propertyEditor.selectedTreeElementValue = propertyCombiner.synthesizedValue;
    }

    _commaSeparatedEditorTreeItemSelected(event)
    {
        if (typeof this.updateValuesFromText === "function")
            this.updateValuesFromText(event.data.text || "");
    }
};

WebInspector.VisualStyleDetailsPanel.StyleDisabledSymbol = Symbol("visual-style-style-disabled");
WebInspector.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol = Symbol("visual-style-initial-property-section-text");

// FIXME: Add information about each property as a form of documentation.  This is currently empty as
// we do not have permission to use the info on sites like MDN and have not written anything ourselves.
WebInspector.VisualStyleDetailsPanel.propertyReferenceInfo = {};
