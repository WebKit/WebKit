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

WI.VisualStyleDetailsPanel = class VisualStyleDetailsPanel extends WI.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, "visual", "visual", WI.UIString("Styles \u2014 Visual"));

        this._currentStyle = null;

        this._sections = {};
        this._groups = {};
        this._keywords = {};
        this._units = {};

        // These keywords, as well as the values below, are not localized as they must match the CSS spec.
        this._keywords.defaults = ["Inherit", "Initial", "Unset", "Revert"];
        this._keywords.normal = this._keywords.defaults.concat(["Normal"]);
        this._keywords.boxModel = this._keywords.defaults.concat(["Auto"]);
        this._keywords.borderStyle = {
            basic: this._keywords.defaults.concat(["None", "Hidden", "Solid"]),
            advanced: ["Dashed", "Dotted", "Double", "Groove", "Inset", "Outset", "Ridge"]
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
    }

    // Public

    refresh(significantChange)
    {
        if (significantChange)
            this._selectorSection.update(this._nodeStyles);
        else
            this._updateSections();

        super.refresh();
    }

    // Protected

    initialLayout()
    {
        // Selector Section
        this._selectorSection = new WI.VisualStyleSelectorSection;
        this._selectorSection.addEventListener(WI.VisualStyleSelectorSection.Event.SelectorChanged, this._updateSections, this);
        this.element.appendChild(this._selectorSection.element);

        // Layout Section
        this._generateSection("display", WI.UIString("Display"));
        this._generateSection("position", WI.UIString("Position"));
        this._generateSection("float", WI.UIString("Float and Clear"));
        this._generateSection("dimensions", WI.UIString("Dimensions"));
        this._generateSection("margin", WI.UIString("Margin"));
        this._generateSection("padding", WI.UIString("Padding"));
        this._generateSection("flexbox", WI.UIString("Flexbox"));
        this._generateSection("alignment", WI.UIString("Alignment"));

        this._sections.layout = new WI.DetailsSection("layout", WI.UIString("Layout"), [this._groups.display.section, this._groups.position.section, this._groups.float.section, this._groups.dimensions.section, this._groups.margin.section, this._groups.padding.section, this._groups.flexbox.section, this._groups.alignment.section]);
        this.element.appendChild(this._sections.layout.element);

        // Text Section
        this._generateSection("content", WI.UIString("Content"));
        this._generateSection("text-style", WI.UIString("Style"));
        this._generateSection("font", WI.UIString("Font"));
        this._generateSection("font-variants", WI.UIString("Variants"));
        this._generateSection("text-spacing", WI.UIString("Spacing"));
        this._generateSection("text-shadow", WI.UIString("Shadow"));

        this._sections.text = new WI.DetailsSection("text", WI.UIString("Text"), [this._groups.content.section, this._groups.textStyle.section, this._groups.font.section, this._groups.fontVariants.section, this._groups.textSpacing.section, this._groups.textShadow.section]);
        this.element.appendChild(this._sections.text.element);

        // Background Section
        this._generateSection("fill", WI.UIString("Fill"));
        this._generateSection("stroke", WI.UIString("Stroke"));
        this._generateSection("background-style", WI.UIString("Style"));
        this._generateSection("border", WI.UIString("Border"));
        this._generateSection("outline", WI.UIString("Outline"));
        this._generateSection("box-shadow", WI.UIString("Box Shadow"));
        this._generateSection("list-style", WI.UIString("List Styles"));

        this._sections.background = new WI.DetailsSection("background", WI.UIString("Background"), [this._groups.fill.section, this._groups.stroke.section, this._groups.backgroundStyle.section, this._groups.border.section, this._groups.outline.section, this._groups.boxShadow.section, this._groups.listStyle.section]);
        this.element.appendChild(this._sections.background.element);

        // Effects Section
        this._generateSection("transition", WI.UIString("Transition"));
        this._generateSection("animation", WI.UIString("Animation"));

        this._sections.effects = new WI.DetailsSection("effects", WI.UIString("Effects"), [this._groups.transition.section, this._groups.animation.section]);
        this.element.appendChild(this._sections.effects.element);
    }

    sizeDidChange()
    {
        super.sizeDidChange();

        let sidebarWidth = this.element.realOffsetWidth;
        for (let key in this._groups) {
            let group = this._groups[key];
            if (!group.specifiedWidthProperties)
                continue;

            for (let editor of group.specifiedWidthProperties)
                editor.recalculateWidth(sidebarWidth);
        }
    }

    // Private

    _generateSection(id, displayName)
    {
        if (!id || !displayName)
            return;

        let camelCaseId = id.toCamelCase();

        function createOptionsElement() {
            let container = document.createElement("div");
            container.title = WI.UIString("Clear modified properties");
            container.addEventListener("click", this._clearModifiedSection.bind(this, camelCaseId));
            return container;
        }

        this._groups[camelCaseId] = {
            section: new WI.DetailsSection(id, displayName, [], createOptionsElement.call(this)),
            properties: {}
        };

        let populateFunction = this["_populate" + camelCaseId.capitalize() + "Section"];
        populateFunction.call(this);
    }

    _updateSections(event)
    {
        this._currentStyle = this._selectorSection.currentStyle();
        if (!this._currentStyle)
            return;

        let disabled = this._currentStyle[WI.VisualStyleDetailsPanel.StyleDisabledSymbol];
        this.element.classList.toggle("disabled", !!disabled);
        if (disabled)
            return;

        for (let key in this._groups)
            this._updateProperties(this._groups[key], !!event);

        if (event) {
            for (let key in this._sections) {
                let section = this._sections[key];
                let oneSectionExpanded = false;
                for (let group of section.groups) {
                    let camelCaseId = group.identifier.toCamelCase();
                    group.collapsed = !group.expandedByUser && !this._groupHasSetProperty(this._groups[camelCaseId]);
                    if (!group.collapsed)
                        oneSectionExpanded = true;
                }
                if (oneSectionExpanded)
                    section.collapsed = false;
            }
        }

        let hasMatchedElementPseudoSelector = this._currentStyle.ownerRule && this._currentStyle.ownerRule.hasMatchedPseudoElementSelector();
        this._groups.content.section.element.classList.toggle("inactive", !hasMatchedElementPseudoSelector);
        this._groups.listStyle.section.element.classList.toggle("inactive", hasMatchedElementPseudoSelector);

        let node = this._nodeStyles.node;
        let isSVGElement = node.isSVGElement();

        this._groups.float.section.element.classList.toggle("inactive", isSVGElement);
        this._groups.border.section.element.classList.toggle("inactive", isSVGElement);
        this._groups.boxShadow.section.element.classList.toggle("inactive", isSVGElement);
        this._groups.listStyle.section.element.classList.toggle("inactive", isSVGElement);

        this._groups.fill.section.element.classList.toggle("inactive", !isSVGElement);
        this._groups.stroke.section.element.classList.toggle("inactive", !isSVGElement);

        let isSVGCircle = node.nodeName() === "circle";
        let isSVGEllipse = node.nodeName() === "ellipse";
        let isSVGRadialGradient = node.nodeName() === "radialGradient";
        let isSVGRect = node.nodeName() === "rect";
        let isSVGLine = node.nodeName() === "line";
        let isSVGLinearGradient = node.nodeName() === "linearGradient";

        // Only show the dimensions section if the current element is not an SVG element or is <ellipse>, <rect>, <circle>, or <radialGradient>.
        this._groups.dimensions.section.element.classList.toggle("inactive", !(!isSVGElement || isSVGEllipse || isSVGRect || isSVGCircle || isSVGRadialGradient));

        // Only show the non-SVG dimensions group if the current element is not an SVG element or is <rect>.
        this._groups.dimensions.defaultGroup.element.classList.toggle("inactive", !(!isSVGElement || isSVGRect));

        // Only show the SVG dimensions group if the current element is an SVG element, <rect>, <circle>, or <radialGradient>.
        this._groups.dimensions.svgGroup.element.classList.toggle("inactive", !(isSVGEllipse || isSVGRect || isSVGCircle || isSVGRadialGradient));

        // Only show editor for "r" if the current element is <circle> or <radialGradient>.
        this._groups.dimensions.properties.r.element.classList.toggle("inactive", !(isSVGCircle || isSVGRadialGradient));

        // Only show editors for "rx" and "ry" if the current element is <ellipse> or <rect>.
        this._groups.dimensions.properties.rx.element.classList.toggle("inactive", !(isSVGEllipse || isSVGRect));
        this._groups.dimensions.properties.ry.element.classList.toggle("inactive", !(isSVGEllipse || isSVGRect));

        // Only show the SVG position group if the current element is <rect>, <circle>, <ellipse>, <line>, <radialGradient>, or <linearGradient>.
        this._groups.position.svgGroup.element.classList.toggle("inactive", !(isSVGRect || isSVGCircle || isSVGEllipse || isSVGLine || isSVGRadialGradient || isSVGLinearGradient));

        // Only show editors for "x" and "y" if the current element is <rect>.
        this._groups.position.properties.x.element.classList.toggle("inactive", !isSVGRect);
        this._groups.position.properties.y.element.classList.toggle("inactive", !isSVGRect);

        // Only show editors for "x1", "y1", "x2", and "y2" if the current element is <line> or <linearGradient>.
        this._groups.position.properties.x1.element.classList.toggle("inactive", !(isSVGLine || isSVGLinearGradient));
        this._groups.position.properties.y1.element.classList.toggle("inactive", !(isSVGLine || isSVGLinearGradient));
        this._groups.position.properties.x2.element.classList.toggle("inactive", !(isSVGLine || isSVGLinearGradient));
        this._groups.position.properties.y2.element.classList.toggle("inactive", !(isSVGLine || isSVGLinearGradient));

        // Only show editors for "cx" and "cy" if the current element is <circle>, <ellipse>, or <radialGradient>.
        this._groups.position.properties.cx.element.classList.toggle("inactive", !(isSVGCircle || isSVGEllipse || isSVGRadialGradient));
        this._groups.position.properties.cy.element.classList.toggle("inactive", !(isSVGCircle || isSVGEllipse || isSVGRadialGradient));
    }

    _updateProperties(group, forceStyleUpdate)
    {
        if (!group.section)
            return;

        if (forceStyleUpdate && group.links) {
            for (let key in group.links)
                group.links[key].linked = false;
        }

        let initialTextList = this._initialTextList;
        if (!initialTextList)
            this._currentStyle[WI.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol] = initialTextList = new WeakMap;

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

        this._sectionModified(group);

        if (group.autocompleteCompatibleProperties) {
            for (let editor of group.autocompleteCompatibleProperties)
                this._updateAutocompleteCompatiblePropertyEditor(editor, forceStyleUpdate);
        }

        if (group.specifiedWidthProperties) {
            let sidebarWidth = this.element.realOffsetWidth;
            for (let editor of group.specifiedWidthProperties)
                editor.recalculateWidth(sidebarWidth);
        }
    }

    _updateAutocompleteCompatiblePropertyEditor(editor, force)
    {
        if (!editor || (editor.hasCompletions && !force))
            return;

        editor.completions = WI.CSSKeywordCompletions.forProperty(editor.propertyReferenceName);
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
        let initialPropertyTextList = this._currentStyle[WI.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol].get(group);
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
        return this._currentStyle[WI.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol];
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
            group.properties[key].addEventListener(WI.VisualStylePropertyEditor.Event.ValueDidChange, this._sectionModified.bind(this, group));
    }

    _populateDisplaySection()
    {
        let group = this._groups.display;
        let properties = group.properties;

        let displayRow = new WI.DetailsSectionRow;

        properties.display = new WI.VisualStyleKeywordPicker("display", WI.UIString("Type"), {
            basic: ["None", "Block", "Flex", "Inline", "Inline Block"],
            advanced: ["Compact", "Inline Flex", "Inline Table", "List Item", "Table", "Table Caption", "Table Cell", "Table Column", "Table Column Group", "Table Footer Group", "Table Header Group", "Table Row", "Table Row Group", " WAP Marquee", " WebKit Box", " WebKit Grid", " WebKit Inline Box", " WebKit Inline Grid"]
        });
        properties.visibility = new WI.VisualStyleKeywordPicker("visibility", WI.UIString("Visibility"), {
            basic: ["Hidden", "Visible"],
            advanced: ["Collapse"]
        });

        displayRow.element.appendChild(properties.display.element);
        displayRow.element.appendChild(properties.visibility.element);

        let sizingRow = new WI.DetailsSectionRow;

        properties.boxSizing = new WI.VisualStyleKeywordPicker("box-sizing", WI.UIString("Sizing"), this._keywords.defaults.concat(["Border Box", "Content Box"]));
        properties.cursor = new WI.VisualStyleKeywordPicker("cursor", WI.UIString("Cursor"), {
            basic: ["Auto", "Default", "None", "Pointer", "Crosshair", "Text"],
            advanced: ["Context Menu", "Help", "Progress", "Wait", "Cell", "Vertical Text", "Alias", "Copy", "Move", "No Drop", "Not Allowed", "All Scroll", "Col Resize", "Row Resize", "N Resize", "E Resize", "S Resize", "W Resize", "NS Resize", "EW Resize", "NE Resize", "NW Resize", "SE Resize", "sW Resize", "NESW Resize", "NWSE Resize"]
        });

        sizingRow.element.appendChild(properties.boxSizing.element);
        sizingRow.element.appendChild(properties.cursor.element);

        let overflowRow = new WI.DetailsSectionRow;

        properties.opacity = new WI.VisualStyleUnitSlider("opacity", WI.UIString("Opacity"));
        properties.overflow = new WI.VisualStyleKeywordPicker(["overflow-x", "overflow-y"], WI.UIString("Overflow"), {
            basic: ["Initial", "Unset", "Revert", "Auto", "Hidden", "Scroll", "Visible"],
            advanced: ["Marquee", "Overlay", " WebKit Paged X", " WebKit Paged Y"]
        });

        overflowRow.element.appendChild(properties.opacity.element);
        overflowRow.element.appendChild(properties.overflow.element);

        group.specifiedWidthProperties = [properties.opacity];

        let displayGroup = new WI.DetailsSectionGroup([displayRow, sizingRow, overflowRow]);
        this._populateSection(group, [displayGroup]);
    }

    _addMetricsMouseListeners(editor, mode)
    {
        function editorMouseover() {
            if (!this._currentStyle)
                return;

            if (!this._currentStyle.ownerRule) {
                WI.domTreeManager.highlightDOMNode(this._currentStyle.node.id, mode);
                return;
            }

            WI.domTreeManager.highlightSelector(this._currentStyle.ownerRule.selectorText, this._currentStyle.node.ownerDocument.frameIdentifier, mode);
        }

        function editorMouseout() {
            WI.domTreeManager.hideDOMNodeHighlight();
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

        let vertical = new WI.DetailsSectionRow;

        properties[top] = new WI.VisualStyleNumberInputBox(propertyNamePrefix + "top", WI.UIString("Top"), this._keywords.boxModel, this._units.defaults, allowNegatives);
        properties[bottom] = new WI.VisualStyleNumberInputBox(propertyNamePrefix + "bottom", WI.UIString("Bottom"), this._keywords.boxModel, this._units.defaults, allowNegatives, true);
        links["vertical"] = new WI.VisualStylePropertyEditorLink([properties[top], properties[bottom]], "link-vertical");

        vertical.element.appendChild(properties[top].element);
        vertical.element.appendChild(links["vertical"].element);
        vertical.element.appendChild(properties[bottom].element);

        let horizontal = new WI.DetailsSectionRow;

        properties[left] = new WI.VisualStyleNumberInputBox(propertyNamePrefix + "left", WI.UIString("Left"), this._keywords.boxModel, this._units.defaults, allowNegatives);
        properties[right] = new WI.VisualStyleNumberInputBox(propertyNamePrefix + "right", WI.UIString("Right"), this._keywords.boxModel, this._units.defaults, allowNegatives, true);
        links["horizontal"] = new WI.VisualStylePropertyEditorLink([properties[left], properties[right]], "link-horizontal");

        horizontal.element.appendChild(properties[left].element);
        horizontal.element.appendChild(links["horizontal"].element);
        horizontal.element.appendChild(properties[right].element);

        let allLinkRow = new WI.DetailsSectionRow;
        links["all"] = new WI.VisualStylePropertyEditorLink([properties[top], properties[bottom], properties[left], properties[right]], "link-all", [links["vertical"], links["horizontal"]]);
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

        vertical.element.classList.add("metric-section-row");
        horizontal.element.classList.add("metric-section-row");
        allLinkRow.element.classList.add("metric-section-row");

        return [vertical, allLinkRow, horizontal];
    }

    _populatePositionSection()
    {
        let group = this._groups.position;
        let rows = this._generateMetricSectionRows(group, null, true);
        let properties = group.properties;

        let positionType = new WI.DetailsSectionRow;

        properties.position = new WI.VisualStyleKeywordPicker("position", WI.UIString("Type"), {
            basic: ["Static", "Relative", "Absolute", "Fixed"],
            advanced: [" WebKit Sticky"]
        });
        properties.zIndex = new WI.VisualStyleNumberInputBox("z-index", WI.UIString("Z-Index"), this._keywords.boxModel, null, true);

        positionType.element.appendChild(properties.position.element);
        positionType.element.appendChild(properties.zIndex.element);
        positionType.element.classList.add("visual-style-separated-row");

        rows.unshift(positionType);

        group.defaultGroup = new WI.DetailsSectionGroup(rows);

        let xyRow = new WI.DetailsSectionRow;

        properties.x = new WI.VisualStyleNumberInputBox("x", WI.UIString("X"), this._keywords.boxModel, this._units.defaults, true);
        properties.y = new WI.VisualStyleNumberInputBox("y", WI.UIString("Y"), this._keywords.boxModel, this._units.defaults, true);

        xyRow.element.appendChild(properties.x.element);
        xyRow.element.appendChild(properties.y.element);

        let x1y1Row = new WI.DetailsSectionRow;

        properties.x1 = new WI.VisualStyleNumberInputBox("x1", WI.UIString("X1"), this._keywords.boxModel, this._units.defaults, true);
        properties.y1 = new WI.VisualStyleNumberInputBox("y1", WI.UIString("Y1"), this._keywords.boxModel, this._units.defaults, true);

        x1y1Row.element.appendChild(properties.x1.element);
        x1y1Row.element.appendChild(properties.y1.element);

        let x2y2Row = new WI.DetailsSectionRow;

        properties.x2 = new WI.VisualStyleNumberInputBox("x2", WI.UIString("X2"), this._keywords.boxModel, this._units.defaults, true);
        properties.y2 = new WI.VisualStyleNumberInputBox("y2", WI.UIString("Y2"), this._keywords.boxModel, this._units.defaults, true);

        x2y2Row.element.appendChild(properties.x2.element);
        x2y2Row.element.appendChild(properties.y2.element);

        let cxcyRow = new WI.DetailsSectionRow;

        properties.cx = new WI.VisualStyleNumberInputBox("cx", WI.UIString("Center X"), this._keywords.boxModel, this._units.defaults, true);
        properties.cy = new WI.VisualStyleNumberInputBox("cy", WI.UIString("Center Y"), this._keywords.boxModel, this._units.defaults, true);

        cxcyRow.element.appendChild(properties.cx.element);
        cxcyRow.element.appendChild(properties.cy.element);

        group.svgGroup = new WI.DetailsSectionGroup([xyRow, x1y1Row, x2y2Row, cxcyRow]);

        this._populateSection(group, [group.defaultGroup, group.svgGroup]);

        let allowedPositionValues = ["relative", "absolute", "fixed", "-webkit-sticky"];
        properties.zIndex.addDependency("position", allowedPositionValues);
        properties.top.addDependency("position", allowedPositionValues);
        properties.right.addDependency("position", allowedPositionValues);
        properties.bottom.addDependency("position", allowedPositionValues);
        properties.left.addDependency("position", allowedPositionValues);
    }

    _populateFloatSection()
    {
        let group = this._groups.float;
        let properties = group.properties;

        let floatRow = new WI.DetailsSectionRow;

        properties.float = new WI.VisualStyleKeywordIconList("float", WI.UIString("Float"), ["Left", "Right", "None"]);
        floatRow.element.appendChild(properties.float.element);

        let clearRow = new WI.DetailsSectionRow;

        properties.clear = new WI.VisualStyleKeywordIconList("clear", WI.UIString("Clear"), ["Left", "Right", "Both", "None"]);
        clearRow.element.appendChild(properties.clear.element);

        let floatGroup = new WI.DetailsSectionGroup([floatRow, clearRow]);
        this._populateSection(group, [floatGroup]);
    }

    _populateDimensionsSection()
    {
        let group = this._groups.dimensions;
        let properties = group.properties;

        let dimensionsWidth = new WI.DetailsSectionRow;

        properties.width = new WI.VisualStyleRelativeNumberSlider("width", WI.UIString("Width"), this._keywords.boxModel, this._units.defaults);

        dimensionsWidth.element.appendChild(properties.width.element);

        let dimensionsHeight = new WI.DetailsSectionRow;

        properties.height = new WI.VisualStyleRelativeNumberSlider("height", WI.UIString("Height"), this._keywords.boxModel, this._units.defaults, true);

        dimensionsHeight.element.appendChild(properties.height.element);

        let dimensionsProperties = [properties.width, properties.height];
        let dimensionsRegularGroup = new WI.DetailsSectionGroup([dimensionsWidth, dimensionsHeight]);

        let dimensionsMinWidth = new WI.DetailsSectionRow;

        properties.minWidth = new WI.VisualStyleRelativeNumberSlider("min-width", WI.UIString("Width"), this._keywords.boxModel, this._units.defaults);

        dimensionsMinWidth.element.appendChild(properties.minWidth.element);

        let dimensionsMinHeight = new WI.DetailsSectionRow;

        properties.minHeight = new WI.VisualStyleRelativeNumberSlider("min-height", WI.UIString("Height"), this._keywords.boxModel, this._units.defaults);

        dimensionsMinHeight.element.appendChild(properties.minHeight.element);

        let dimensionsMinProperties = [properties.minWidth, properties.minHeight];
        let dimensionsMinGroup = new WI.DetailsSectionGroup([dimensionsMinWidth, dimensionsMinHeight]);

        let dimensionsMaxKeywords = this._keywords.defaults.concat("None");
        let dimensionsMaxWidth = new WI.DetailsSectionRow;

        properties.maxWidth = new WI.VisualStyleRelativeNumberSlider("max-width", WI.UIString("Width"), dimensionsMaxKeywords, this._units.defaults);

        dimensionsMaxWidth.element.appendChild(properties.maxWidth.element);

        let dimensionsMaxHeight = new WI.DetailsSectionRow;

        properties.maxHeight = new WI.VisualStyleRelativeNumberSlider("max-height", WI.UIString("Height"), dimensionsMaxKeywords, this._units.defaults);

        dimensionsMaxHeight.element.appendChild(properties.maxHeight.element);

        let dimensionsMaxProperties = [properties.maxWidth, properties.maxHeight];
        let dimensionsMaxGroup = new WI.DetailsSectionGroup([dimensionsMaxWidth, dimensionsMaxHeight]);

        let dimensionsTabController = new WI.VisualStyleTabbedPropertiesRow({
            "default": {title: WI.UIString("Default"), element: dimensionsRegularGroup.element, properties: dimensionsProperties},
            "min": {title: WI.UIString("Min"), element: dimensionsMinGroup.element, properties: dimensionsMinProperties},
            "max": {title: WI.UIString("Max"), element: dimensionsMaxGroup.element, properties: dimensionsMaxProperties}
        });

        let highlightMode = "content";
        this._addMetricsMouseListeners(group.properties.width, highlightMode);
        this._addMetricsMouseListeners(group.properties.height, highlightMode);
        this._addMetricsMouseListeners(group.properties.minWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.minHeight, highlightMode);
        this._addMetricsMouseListeners(group.properties.maxWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.maxHeight, highlightMode);

        group.defaultGroup = new WI.DetailsSectionGroup([dimensionsTabController, dimensionsRegularGroup, dimensionsMinGroup, dimensionsMaxGroup]);

        let rRow = new WI.DetailsSectionRow;

        properties.r = new WI.VisualStyleRelativeNumberSlider("r", WI.UIString("Radius"), this._keywords.boxModel, this._units.defaults);

        rRow.element.appendChild(properties.r.element);

        let rxRow = new WI.DetailsSectionRow;

        properties.rx = new WI.VisualStyleRelativeNumberSlider("rx", WI.UIString("Radius X"), this._keywords.boxModel, this._units.defaults);

        rxRow.element.appendChild(properties.rx.element);

        let ryRow = new WI.DetailsSectionRow;

        properties.ry = new WI.VisualStyleRelativeNumberSlider("ry", WI.UIString("Radius Y"), this._keywords.boxModel, this._units.defaults);

        ryRow.element.appendChild(properties.ry.element);

        group.svgGroup = new WI.DetailsSectionGroup([rRow, rxRow, ryRow]);

        this._populateSection(group, [group.defaultGroup, group.svgGroup]);
    }

    _populateMarginSection()
    {
        let group = this._groups.margin;
        let rows = this._generateMetricSectionRows(group, "margin", true, true);
        let marginGroup = new WI.DetailsSectionGroup(rows);
        this._populateSection(group, [marginGroup]);
    }

    _populatePaddingSection()
    {
        let group = this._groups.padding;
        let rows = this._generateMetricSectionRows(group, "padding", false, true);
        let paddingGroup = new WI.DetailsSectionGroup(rows);
        this._populateSection(group, [paddingGroup]);
    }

    _populateFlexboxSection()
    {
        let group = this._groups.flexbox;
        let properties = group.properties;

        let flexOrderRow = new WI.DetailsSectionRow;

        properties.order = new WI.VisualStyleNumberInputBox("order", WI.UIString("Order"), this._keywords.defaults);
        properties.flexBasis = new WI.VisualStyleNumberInputBox("flex-basis", WI.UIString("Basis"), this._keywords.boxModel, this._units.defaults, true);

        flexOrderRow.element.appendChild(properties.order.element);
        flexOrderRow.element.appendChild(properties.flexBasis.element);

        let flexSizeRow = new WI.DetailsSectionRow;

        properties.flexGrow = new WI.VisualStyleNumberInputBox("flex-grow", WI.UIString("Grow"), this._keywords.defaults);
        properties.flexShrink = new WI.VisualStyleNumberInputBox("flex-shrink", WI.UIString("Shrink"), this._keywords.defaults);

        flexSizeRow.element.appendChild(properties.flexGrow.element);
        flexSizeRow.element.appendChild(properties.flexShrink.element);

        let flexFlowRow = new WI.DetailsSectionRow;

        properties.flexDirection = new WI.VisualStyleKeywordPicker("flex-direction", WI.UIString("Direction"), this._keywords.defaults.concat(["Row", "Row Reverse", "Column", "Column Reverse"]));
        properties.flexWrap = new WI.VisualStyleKeywordPicker("flex-wrap", WI.UIString("Wrap"), this._keywords.defaults.concat(["Wrap", "Wrap Reverse", "Nowrap"]));

        flexFlowRow.element.appendChild(properties.flexDirection.element);
        flexFlowRow.element.appendChild(properties.flexWrap.element);

        let flexboxGroup = new WI.DetailsSectionGroup([flexOrderRow, flexSizeRow, flexFlowRow]);
        this._populateSection(group, [flexboxGroup]);

        let allowedDisplayValues = ["flex", "inline-flex", "-webkit-box", "-webkit-inline-box"];
        properties.order.addDependency("display", allowedDisplayValues);
        properties.flexBasis.addDependency("display", allowedDisplayValues);
        properties.flexGrow.addDependency("display", allowedDisplayValues);
        properties.flexShrink.addDependency("display", allowedDisplayValues);
        properties.flexDirection.addDependency("display", allowedDisplayValues);
        properties.flexWrap.addDependency("display", allowedDisplayValues);
    }

    _populateAlignmentSection()
    {
        let group = this._groups.alignment;
        let properties = group.properties;
        let alignmentKeywords = ["Initial", "Unset", "Revert", "Auto", "Flex Start", "Flex End", "Center", "Stretch"];
        let advancedAlignmentKeywords = ["Start", "End", "Left", "Right", "Baseline", "Last Baseline"];

        let contentRow = new WI.DetailsSectionRow;
        let contentKeywords = {
            basic: alignmentKeywords.concat(["Space Between", "Space Around"]),
            advanced: advancedAlignmentKeywords.concat(["Space Evenly"])
        };

        properties.justifyContent = new WI.VisualStyleKeywordPicker("justify-content", WI.UIString("Horizontal"), contentKeywords);
        properties.alignContent = new WI.VisualStyleKeywordPicker("align-content", WI.UIString("Vertical"), contentKeywords);

        contentRow.element.appendChild(properties.justifyContent.element);
        contentRow.element.appendChild(properties.alignContent.element);

        let itemsRow = new WI.DetailsSectionRow;
        let itemKeywords = {
            basic: alignmentKeywords,
            advanced: ["Self Start", "Self End"].concat(advancedAlignmentKeywords)
        };

        properties.alignItems = new WI.VisualStyleKeywordPicker("align-items", WI.UIString("Children"), itemKeywords);
        properties.alignSelf = new WI.VisualStyleKeywordPicker("align-self", WI.UIString("Self"), itemKeywords);

        itemsRow.element.appendChild(properties.alignItems.element);
        itemsRow.element.appendChild(properties.alignSelf.element);

        let alignmentGroup = new WI.DetailsSectionGroup([contentRow, itemsRow]);
        this._populateSection(group, [alignmentGroup]);

        let allowedDisplayValues = ["flex", "inline-flex", "-webkit-box", "-webkit-inline-box"];
        properties.justifyContent.addDependency("display", allowedDisplayValues);
        properties.alignContent.addDependency("display", allowedDisplayValues);
        properties.alignItems.addDependency("display", allowedDisplayValues);
        properties.alignSelf.addDependency("display", allowedDisplayValues);
    }

    _populateContentSection()
    {
        let group = this._groups.content;
        let properties = group.properties;

        let contentRow = new WI.DetailsSectionRow;

        properties.content = new WI.VisualStyleBasicInput("content", null, WI.UIString("Enter value"));

        contentRow.element.appendChild(properties.content.element);

        let contentGroup = new WI.DetailsSectionGroup([contentRow]);
        this._populateSection(group, [contentGroup]);
    }

    _populateTextStyleSection()
    {
        let group = this._groups.textStyle;
        let properties = group.properties;

        let textAppearanceRow = new WI.DetailsSectionRow;

        properties.color = new WI.VisualStyleColorPicker("color", WI.UIString("Color"));
        properties.textDirection = new WI.VisualStyleKeywordPicker("direction", WI.UIString("Direction"), this._keywords.defaults.concat(["LTR", "RTL"]));

        textAppearanceRow.element.appendChild(properties.color.element);
        textAppearanceRow.element.appendChild(properties.textDirection.element);

        let textAlignRow = new WI.DetailsSectionRow;

        properties.textAlign = new WI.VisualStyleKeywordIconList("text-align", WI.UIString("Align"), ["Left", "Center", "Right", "Justify"]);

        textAlignRow.element.appendChild(properties.textAlign.element);

        let textTransformRow = new WI.DetailsSectionRow;

        properties.textTransform = new WI.VisualStyleKeywordIconList("text-transform", WI.UIString("Transform"), ["Capitalize", "Uppercase", "Lowercase", "None"]);

        textTransformRow.element.appendChild(properties.textTransform.element);

        let textDecorationRow = new WI.DetailsSectionRow;

        properties.textDecoration = new WI.VisualStyleKeywordIconList("text-decoration", WI.UIString("Decoration"), ["Underline", "Line Through", "Overline", "None"]);

        textDecorationRow.element.appendChild(properties.textDecoration.element);

        group.autocompleteCompatibleProperties = [properties.color];

        let textStyleGroup = new WI.DetailsSectionGroup([textAppearanceRow, textAlignRow, textTransformRow, textDecorationRow]);
        this._populateSection(group, [textStyleGroup]);
    }

    _populateFontSection()
    {
        let group = this._groups.font;
        let properties = group.properties;

        let fontFamilyRow = new WI.DetailsSectionRow;

        properties.fontFamily = new WI.VisualStyleFontFamilyListEditor("font-family", WI.UIString("Family"));

        fontFamilyRow.element.appendChild(properties.fontFamily.element);

        let fontSizeRow = new WI.DetailsSectionRow;

        properties.fontSize = new WI.VisualStyleNumberInputBox("font-size", WI.UIString("Size"), this._keywords.defaults.concat(["Larger", "XX Large", "X Large", "Large", "Medium", "Small", "X Small", "XX Small", "Smaller"]), this._units.defaults);

        properties.fontWeight = new WI.VisualStyleKeywordPicker("font-weight", WI.UIString("Weight"), {
            basic: this._keywords.defaults.concat(["Bolder", "Bold", "Normal", "Lighter"]),
            advanced: ["100", "200", "300", "400", "500", "600", "700", "800", "900"]
        });

        fontSizeRow.element.appendChild(properties.fontSize.element);
        fontSizeRow.element.appendChild(properties.fontWeight.element);

        let fontStyleRow = new WI.DetailsSectionRow;

        properties.fontStyle = new WI.VisualStyleKeywordIconList("font-style", WI.UIString("Style"), ["Italic", "Normal"]);
        properties.fontFeatureSettings = new WI.VisualStyleBasicInput("font-feature-settings", WI.UIString("Features"), WI.UIString("Enter Tag"));

        fontStyleRow.element.appendChild(properties.fontStyle.element);
        fontStyleRow.element.appendChild(properties.fontFeatureSettings.element);

        group.autocompleteCompatibleProperties = [properties.fontFamily];
        group.specifiedWidthProperties = [properties.fontFamily];

        let fontGroup = new WI.DetailsSectionGroup([fontFamilyRow, fontSizeRow, fontStyleRow]);
        this._populateSection(group, [fontGroup]);
    }

    _populateFontVariantsSection()
    {
        let group = this._groups.fontVariants;
        let properties = group.properties;

        let alternatesRow = new WI.DetailsSectionRow;

        properties.fontVariantAlternates = new WI.VisualStyleBasicInput("font-variant-alternates", WI.UIString("Alternates"), WI.UIString("Enter Value"));

        alternatesRow.element.appendChild(properties.fontVariantAlternates.element);

        let positionRow = new WI.DetailsSectionRow;

        properties.fontVariantPosition = new WI.VisualStyleKeywordPicker("font-variant-position", WI.UIString("Position"), this._keywords.normal.concat(["Sub", "Super"]));

        positionRow.element.appendChild(properties.fontVariantPosition.element);

        properties.fontVariantCaps = new WI.VisualStyleKeywordPicker("font-variant-caps", WI.UIString("Caps"), this._keywords.normal.concat(["None", "Small Caps", "All Small Caps", "Petite Caps", "All Petite Caps", "Unicase", "Titling Caps"]));

        positionRow.element.appendChild(properties.fontVariantCaps.element);

        let ligaturesRow = new WI.DetailsSectionRow;

        // FIXME <http://webkit.org/b/153645> Add token based editor for Visual Sidebar
        properties.fontVariantLigatures = new WI.VisualStyleKeywordPicker("font-variant-ligatures", WI.UIString("Ligatures"), this._keywords.normal.concat(["None", "Common Ligatures", "No Common Ligatures", "Discretionary Ligatures", "No Discretionary Ligatures", "Historical Ligatures", "No Historical Ligatures", "Contextual", "No Contextual"]));

        ligaturesRow.element.appendChild(properties.fontVariantLigatures.element);

        properties.fontVariantNumeric = new WI.VisualStyleKeywordPicker("font-variant-numeric", WI.UIString("Numeric"), this._keywords.normal.concat(["None", "Ordinal", "Slashed Zero", "Lining Nums", "Oldstyle Nums", "Proportional Nums", "Tabular Nums", "Diagonal Fractions", "Stacked Fractions"]));

        ligaturesRow.element.appendChild(properties.fontVariantNumeric.element);

        let variantsGroup = new WI.DetailsSectionGroup([alternatesRow, positionRow, ligaturesRow]);
        this._populateSection(group, [variantsGroup]);
    }

    _populateTextSpacingSection()
    {
        let group = this._groups.textSpacing;
        let properties = group.properties;

        let textLayoutRow = new WI.DetailsSectionRow;

        properties.lineHeight = new WI.VisualStyleNumberInputBox("line-height", WI.UIString("Height"), this._keywords.normal, this._units.defaults);
        properties.verticalAlign = new WI.VisualStyleNumberInputBox("vertical-align", WI.UIString("Align"), ["Baseline", "Bottom"].concat(this._keywords.defaults, ["Middle", "Sub", "Super", "Text Bottom", "Text Top", "Top"]), this._units.defaults);

        textLayoutRow.element.appendChild(properties.lineHeight.element);
        textLayoutRow.element.appendChild(properties.verticalAlign.element);

        let textSpacingRow = new WI.DetailsSectionRow;

        properties.letterSpacing = new WI.VisualStyleNumberInputBox("letter-spacing", WI.UIString("Letter"), this._keywords.normal, this._units.defaults);
        properties.wordSpacing = new WI.VisualStyleNumberInputBox("word-spacing", WI.UIString("Word"), this._keywords.normal, this._units.defaults);

        textSpacingRow.element.appendChild(properties.letterSpacing.element);
        textSpacingRow.element.appendChild(properties.wordSpacing.element);

        let textWhitespaceRow = new WI.DetailsSectionRow;

        properties.textIndent = new WI.VisualStyleNumberInputBox("text-indent", WI.UIString("Indent"), this._keywords.defaults, this._units.defaults);
        properties.whiteSpace = new WI.VisualStyleKeywordPicker("white-space", WI.UIString("Whitespace"), this._keywords.defaults.concat(["Normal", "Nowrap", "Pre", "Pre Line", "Pre Wrap"]));

        textWhitespaceRow.element.appendChild(properties.textIndent.element);
        textWhitespaceRow.element.appendChild(properties.whiteSpace.element);

        let textSpacingGroup = new WI.DetailsSectionGroup([textLayoutRow, textSpacingRow, textWhitespaceRow]);
        this._populateSection(group, [textSpacingGroup]);
    }

    _populateTextShadowSection()
    {
        let group = this._groups.textShadow;
        let properties = group.properties;

        let textShadowSizing = new WI.DetailsSectionRow;

        let textShadowH = new WI.VisualStyleNumberInputBox("text-shadow", WI.UIString("Horizontal"), null, this._units.defaultsSansPercent);
        let textShadowV = new WI.VisualStyleNumberInputBox("text-shadow", WI.UIString("Vertical"), null, this._units.defaultsSansPercent);

        textShadowSizing.element.appendChild(textShadowH.element);
        textShadowSizing.element.appendChild(textShadowV.element);

        let textShadowStyle = new WI.DetailsSectionRow;

        let textShadowColor = new WI.VisualStyleColorPicker("text-shadow", WI.UIString("Color"));
        let textShadowBlur = new WI.VisualStyleNumberInputBox("text-shadow", WI.UIString("Blur"), null, this._units.defaultsSansPercent);
        textShadowBlur.optionalProperty = true;

        textShadowStyle.element.appendChild(textShadowColor.element);
        textShadowStyle.element.appendChild(textShadowBlur.element);

        properties.textShadow = new WI.VisualStylePropertyCombiner("text-shadow", [textShadowH, textShadowV, textShadowBlur, textShadowColor]);

        group.autocompleteCompatibleProperties = [textShadowColor];

        let textShadowGroup = new WI.DetailsSectionGroup([textShadowSizing, textShadowStyle]);
        this._populateSection(group, [textShadowGroup]);
    }

    _populateFillSection()
    {
        let group = this._groups.fill;
        let properties = group.properties;

        let fillRow = new WI.DetailsSectionRow;

        properties.fill = new WI.VisualStyleColorPicker("fill", WI.UIString("Color"));
        properties.fillRule = new WI.VisualStyleKeywordPicker("fill-rule", WI.UIString("Rule"), this._keywords.defaults.concat(["Nonzero", "Evenodd"]));

        fillRow.element.appendChild(properties.fill.element);
        fillRow.element.appendChild(properties.fillRule.element);

        let fillOpacityRow = new WI.DetailsSectionRow;

        properties.fillOpacity = new WI.VisualStyleUnitSlider("fill-opacity", WI.UIString("Opacity"));

        fillOpacityRow.element.appendChild(properties.fillOpacity.element);

        group.specifiedWidthProperties = [properties.fillOpacity];

        let fillGroup = new WI.DetailsSectionGroup([fillRow, fillOpacityRow]);
        this._populateSection(group, [fillGroup]);
    }

    _populateStrokeSection()
    {
        let group = this._groups.stroke;
        let properties = group.properties;

        let strokeRow = new WI.DetailsSectionRow;

        properties.stroke = new WI.VisualStyleColorPicker("stroke", WI.UIString("Color"));
        properties.strokeWidth = new WI.VisualStyleNumberInputBox("stroke-width", WI.UIString("Width"), this._keywords.defaults, this._units.defaults);

        strokeRow.element.appendChild(properties.stroke.element);
        strokeRow.element.appendChild(properties.strokeWidth.element);

        let strokeOpacity = new WI.DetailsSectionRow;

        properties.strokeOpacity = new WI.VisualStyleUnitSlider("stroke-opacity", WI.UIString("Opacity"));

        strokeOpacity.element.appendChild(properties.strokeOpacity.element);

        let strokeDasharrayRow = new WI.DetailsSectionRow;

        properties.strokeDasharray = new WI.VisualStyleBasicInput("stroke-dasharray", WI.UIString("Dash Array"), WI.UIString("Enter an array value"));

        strokeDasharrayRow.element.appendChild(properties.strokeDasharray.element);

        let strokeDasharrayOptionsRow = new WI.DetailsSectionRow;

        properties.strokeDashoffset = new WI.VisualStyleNumberInputBox("stroke-dashoffset", WI.UIString("Offset"), this._keywords.defaults, this._units.defaults);
        properties.strokeMiterlimit = new WI.VisualStyleNumberInputBox("stroke-miterlimit", WI.UIString("Miter"), this._keywords.defaults);

        strokeDasharrayOptionsRow.element.appendChild(properties.strokeDashoffset.element);
        strokeDasharrayOptionsRow.element.appendChild(properties.strokeMiterlimit.element);

        let strokeLineOptionsRow = new WI.DetailsSectionRow;

        properties.strokeLinecap = new WI.VisualStyleKeywordPicker("stroke-linecap", WI.UIString("Cap"), this._keywords.defaults.concat(["Butt", "Round", "Square"]));
        properties.strokeLinejoin = new WI.VisualStyleKeywordPicker("stroke-linejoin", WI.UIString("Join"), this._keywords.defaults.concat(["Miter", "Round", "Bevel"]));

        strokeLineOptionsRow.element.appendChild(properties.strokeLinecap.element);
        strokeLineOptionsRow.element.appendChild(properties.strokeLinejoin.element);

        group.specifiedWidthProperties = [properties.strokeOpacity];

        let strokeGroup = new WI.DetailsSectionGroup([strokeRow, strokeOpacity, strokeDasharrayRow, strokeDasharrayOptionsRow, strokeLineOptionsRow]);
        this._populateSection(group, [strokeGroup]);
    }

    _populateBackgroundStyleSection()
    {
        let group = this._groups.backgroundStyle;
        let properties = group.properties;

        let backgroundStyleRow = new WI.DetailsSectionRow;

        properties.backgroundColor = new WI.VisualStyleColorPicker("background-color", WI.UIString("Color"));
        properties.backgroundBlendMode = new WI.VisualStyleKeywordPicker("background-blend-mode", WI.UIString("Blend"), ["Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color", "Color Dodge", "Saturation", "Luminosity"]);

        backgroundStyleRow.element.appendChild(properties.backgroundColor.element);
        backgroundStyleRow.element.appendChild(properties.backgroundBlendMode.element);

        let backgroundClipRow = new WI.DetailsSectionRow;

        let backgroundClipKeywords = ["Initial", "Border Box", "Padding Box", "Content Box"];
        properties.backgroundClip = new WI.VisualStyleKeywordPicker("background-clip", WI.UIString("Clip"), backgroundClipKeywords);
        properties.backgroundOrigin = new WI.VisualStyleKeywordPicker("background-origin", WI.UIString("Origin"), backgroundClipKeywords);

        backgroundClipRow.element.appendChild(properties.backgroundClip.element);
        backgroundClipRow.element.appendChild(properties.backgroundOrigin.element);

        let backgroundSizeRow = new WI.DetailsSectionRow;

        let backgroundSizeKeywords = this._keywords.boxModel.concat(["Contain", "Cover"]);
        let backgroundSizeX = new WI.VisualStyleNumberInputBox("background-size", WI.UIString("Width"), backgroundSizeKeywords, this._units.defaults);
        backgroundSizeX.masterProperty = true;
        let backgroundSizeY = new WI.VisualStyleNumberInputBox("background-size", WI.UIString("Height"), backgroundSizeKeywords, this._units.defaults);
        backgroundSizeY.masterProperty = true;

        properties.backgroundSize = new WI.VisualStylePropertyCombiner("background-size", [backgroundSizeX, backgroundSizeY]);

        backgroundSizeRow.element.appendChild(backgroundSizeX.element);
        backgroundSizeRow.element.appendChild(backgroundSizeY.element);

        let backgroundRow = new WI.DetailsSectionRow;

        properties.background = new WI.VisualStyleCommaSeparatedKeywordEditor("background", null, {
            "background-image": "none",
            "background-position": "0% 0%",
            "background-repeat": "repeat",
            "background-attachment": "scroll",
        });

        backgroundRow.element.appendChild(properties.background.element);

        let backgroundImageRow = new WI.DetailsSectionRow;

        let backgroundImage = new WI.VisualStyleBackgroundPicker("background-image", WI.UIString("Type"), this._keywords.defaults.concat(["None"]));

        backgroundImageRow.element.appendChild(backgroundImage.element);

        let backgroundPositionRow = new WI.DetailsSectionRow;

        let backgroundPositionX = new WI.VisualStyleNumberInputBox("background-position", WI.UIString("Position X"), ["Center", "Left", "Right"], this._units.defaults);
        backgroundPositionX.optionalProperty = true;
        let backgroundPositionY = new WI.VisualStyleNumberInputBox("background-position", WI.UIString("Position Y"), ["Bottom", "Center", "Top"], this._units.defaults);
        backgroundPositionY.optionalProperty = true;

        backgroundPositionRow.element.appendChild(backgroundPositionX.element);
        backgroundPositionRow.element.appendChild(backgroundPositionY.element);

        let backgroundRepeatRow = new WI.DetailsSectionRow;

        let backgroundRepeat = new WI.VisualStyleKeywordPicker("background-repeat", WI.UIString("Repeat"), this._keywords.defaults.concat(["No Repeat", "Repeat", "Repeat X", "Repeat Y"]));
        backgroundRepeat.optionalProperty = true;
        let backgroundAttachment = new WI.VisualStyleKeywordPicker("background-attachment", WI.UIString("Attach"), this._keywords.defaults.concat(["Fixed", "Local", "Scroll"]));
        backgroundAttachment.optionalProperty = true;

        backgroundRepeatRow.element.appendChild(backgroundRepeat.element);
        backgroundRepeatRow.element.appendChild(backgroundAttachment.element);

        let backgroundProperties = [backgroundImage, backgroundPositionX, backgroundPositionY, backgroundRepeat, backgroundAttachment];
        let backgroundPropertyCombiner = new WI.VisualStylePropertyCombiner("background", backgroundProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, backgroundPropertyCombiner, backgroundProperties);
        properties.background.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.background, backgroundPropertyCombiner);
        backgroundPropertyCombiner.addEventListener(WI.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(backgroundPropertyCombiner);
        properties.background.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [properties.backgroundColor];
        group.specifiedWidthProperties = [properties.background];

        let backgroundStyleGroup = new WI.DetailsSectionGroup([backgroundStyleRow, backgroundClipRow, backgroundSizeRow, backgroundRow, backgroundImageRow, backgroundPositionRow, backgroundRepeatRow]);
        this._populateSection(group, [backgroundStyleGroup]);
    }

    _populateBorderSection()
    {
        let group = this._groups.border;
        let properties = group.properties;

        let borderAllSize = new WI.DetailsSectionRow;

        properties.borderStyle = new WI.VisualStyleKeywordPicker(["border-top-style", "border-right-style", "border-bottom-style", "border-left-style"], WI.UIString("Style"), this._keywords.borderStyle);
        properties.borderStyle.propertyReferenceName = "border-style";
        properties.borderWidth = new WI.VisualStyleNumberInputBox(["border-top-width", "border-right-width", "border-bottom-width", "border-left-width"], WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);
        properties.borderWidth.propertyReferenceName = "border-width";

        borderAllSize.element.appendChild(properties.borderStyle.element);
        borderAllSize.element.appendChild(properties.borderWidth.element);

        let borderAllStyle = new WI.DetailsSectionRow;

        properties.borderColor = new WI.VisualStyleColorPicker(["border-top-color", "border-right-color", "border-bottom-color", "border-left-color"], WI.UIString("Color"));
        properties.borderColor.propertyReferenceName = "border-color";
        properties.borderRadius = new WI.VisualStyleNumberInputBox(["border-top-left-radius", "border-top-right-radius", "border-bottom-left-radius", "border-bottom-right-radius"], WI.UIString("Radius"), this._keywords.defaults, this._units.defaults);
        properties.borderRadius.propertyReferenceName = "border-radius";

        borderAllStyle.element.appendChild(properties.borderColor.element);
        borderAllStyle.element.appendChild(properties.borderRadius.element);

        let borderAllProperties = [properties.borderStyle, properties.borderWidth, properties.borderColor, properties.borderRadius];
        let borderAllGroup = new WI.DetailsSectionGroup([borderAllSize, borderAllStyle]);

        let borderTopSize = new WI.DetailsSectionRow;

        properties.borderTopStyle = new WI.VisualStyleKeywordPicker("border-top-style", WI.UIString("Style"), this._keywords.borderStyle);
        properties.borderTopWidth = new WI.VisualStyleNumberInputBox("border-top-width", WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderTopSize.element.appendChild(properties.borderTopStyle.element);
        borderTopSize.element.appendChild(properties.borderTopWidth.element);

        let borderTopStyle = new WI.DetailsSectionRow;

        properties.borderTopColor = new WI.VisualStyleColorPicker("border-top-color", WI.UIString("Color"));
        properties.borderTopRadius = new WI.VisualStyleNumberInputBox(["border-top-left-radius", "border-top-right-radius"], WI.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderTopStyle.element.appendChild(properties.borderTopColor.element);
        borderTopStyle.element.appendChild(properties.borderTopRadius.element);

        let borderTopProperties = [properties.borderTopStyle, properties.borderTopWidth, properties.borderTopColor, properties.borderTopRadius];
        let borderTopGroup = new WI.DetailsSectionGroup([borderTopSize, borderTopStyle]);

        let borderRightSize = new WI.DetailsSectionRow;

        properties.borderRightStyle = new WI.VisualStyleKeywordPicker("border-right-style", WI.UIString("Style"), this._keywords.borderStyle);
        properties.borderRightWidth = new WI.VisualStyleNumberInputBox("border-right-width", WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderRightSize.element.appendChild(properties.borderRightStyle.element);
        borderRightSize.element.appendChild(properties.borderRightWidth.element);

        let borderRightStyle = new WI.DetailsSectionRow;

        properties.borderRightColor = new WI.VisualStyleColorPicker("border-right-color", WI.UIString("Color"));
        properties.borderRightRadius = new WI.VisualStyleNumberInputBox(["border-top-right-radius", "border-bottom-right-radius"], WI.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderRightStyle.element.appendChild(properties.borderRightColor.element);
        borderRightStyle.element.appendChild(properties.borderRightRadius.element);

        let borderRightProperties = [properties.borderRightStyle, properties.borderRightWidth, properties.borderRightColor, properties.borderRightRadius];
        let borderRightGroup = new WI.DetailsSectionGroup([borderRightSize, borderRightStyle]);

        let borderBottomSize = new WI.DetailsSectionRow;

        properties.borderBottomStyle = new WI.VisualStyleKeywordPicker("border-bottom-style", WI.UIString("Style"), this._keywords.borderStyle);
        properties.borderBottomWidth = new WI.VisualStyleNumberInputBox("border-bottom-width", WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderBottomSize.element.appendChild(properties.borderBottomStyle.element);
        borderBottomSize.element.appendChild(properties.borderBottomWidth.element);

        let borderBottomStyle = new WI.DetailsSectionRow;

        properties.borderBottomColor = new WI.VisualStyleColorPicker("border-bottom-color", WI.UIString("Color"));
        properties.borderBottomRadius = new WI.VisualStyleNumberInputBox(["border-bottom-left-radius", "border-bottom-right-radius"], WI.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderBottomStyle.element.appendChild(properties.borderBottomColor.element);
        borderBottomStyle.element.appendChild(properties.borderBottomRadius.element);

        let borderBottomProperties = [properties.borderBottomStyle, properties.borderBottomWidth, properties.borderBottomColor, properties.borderBottomRadius];
        let borderBottomGroup = new WI.DetailsSectionGroup([borderBottomSize, borderBottomStyle]);

        let borderLeftSize = new WI.DetailsSectionRow;

        properties.borderLeftStyle = new WI.VisualStyleKeywordPicker("border-left-style", WI.UIString("Style"), this._keywords.borderStyle);
        properties.borderLeftWidth = new WI.VisualStyleNumberInputBox("border-left-width", WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        borderLeftSize.element.appendChild(properties.borderLeftStyle.element);
        borderLeftSize.element.appendChild(properties.borderLeftWidth.element);

        let borderLeftStyle = new WI.DetailsSectionRow;

        properties.borderLeftColor = new WI.VisualStyleColorPicker("border-left-color", WI.UIString("Color"));
        properties.borderLeftRadius = new WI.VisualStyleNumberInputBox(["border-top-left-radius", "border-bottom-left-radius"], WI.UIString("Radius"), this._keywords.defaults, this._units.defaults);

        borderLeftStyle.element.appendChild(properties.borderLeftColor.element);
        borderLeftStyle.element.appendChild(properties.borderLeftRadius.element);

        let borderLeftProperties = [properties.borderLeftStyle, properties.borderLeftWidth, properties.borderLeftColor, properties.borderLeftRadius];
        let borderLeftGroup = new WI.DetailsSectionGroup([borderLeftSize, borderLeftStyle]);

        let borderTabController = new WI.VisualStyleTabbedPropertiesRow({
            "all": {title: WI.UIString("All"), element: borderAllGroup.element, properties: borderAllProperties},
            "top": {title: WI.UIString("Top"), element: borderTopGroup.element, properties: borderTopProperties},
            "right": {title: WI.UIString("Right"), element: borderRightGroup.element, properties: borderRightProperties},
            "bottom": {title: WI.UIString("Bottom"), element: borderBottomGroup.element, properties: borderBottomProperties},
            "left": {title: WI.UIString("Left"), element: borderLeftGroup.element, properties: borderLeftProperties}
        });

        let highlightMode = "border";
        this._addMetricsMouseListeners(group.properties.borderWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderTopWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderBottomWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderLeftWidth, highlightMode);
        this._addMetricsMouseListeners(group.properties.borderRightWidth, highlightMode);

        let borderGroup = new WI.DetailsSectionGroup([borderTabController, borderAllGroup, borderTopGroup, borderRightGroup, borderBottomGroup, borderLeftGroup]);

        let borderImageSourceRow = new WI.DetailsSectionRow;

        properties.borderImageSource = new WI.VisualStyleURLInput("border-image-source", WI.UIString("Image"), this._keywords.defaults.concat(["None"]));

        borderImageSourceRow.element.appendChild(properties.borderImageSource.element);

        let borderImageRepeatRow = new WI.DetailsSectionRow;

        let borderImageSliceFill = new WI.VisualStyleKeywordCheckbox("border-image-slice", WI.UIString("Fill"), "Fill");
        borderImageSliceFill.optionalProperty = true;
        properties.borderImageRepeat = new WI.VisualStyleKeywordPicker("border-image-repeat", WI.UIString("Repeat"), this._keywords.defaults.concat(["Stretch", "Repeat", "Round", "Space"]));

        borderImageRepeatRow.element.appendChild(borderImageSliceFill.element);
        borderImageRepeatRow.element.appendChild(properties.borderImageRepeat.element);

        function generateBorderImagePropertyEditors(propertyName, keywords, units) {
            let vertical = new WI.DetailsSectionRow;

            let top = new WI.VisualStyleNumberInputBox(propertyName, WI.UIString("Top"), keywords, units);
            top.masterProperty = true;
            let bottom = new WI.VisualStyleNumberInputBox(propertyName, WI.UIString("Bottom"), keywords, units);
            bottom.masterProperty = true;

            vertical.element.appendChild(top.element);
            vertical.element.appendChild(bottom.element);

            let horizontal = new WI.DetailsSectionRow;

            let left = new WI.VisualStyleNumberInputBox(propertyName, WI.UIString("Left"), keywords, units);
            left.masterProperty = true;
            let right = new WI.VisualStyleNumberInputBox(propertyName, WI.UIString("Right"), keywords, units);
            right.masterProperty = true;

            horizontal.element.appendChild(left.element);
            horizontal.element.appendChild(right.element);

            return {group: new WI.DetailsSectionGroup([vertical, horizontal]), properties: [top, bottom, left, right]};
        }

        let nonKeywordUnits = [WI.UIString("Number")];

        let borderImageUnits = Object.shallowCopy(this._units.defaults);
        borderImageUnits.basic = nonKeywordUnits.concat(borderImageUnits.basic);
        let borderImageWidth = generateBorderImagePropertyEditors("border-image-width", this._keywords.boxModel, borderImageUnits);
        properties.borderImageWidth = new WI.VisualStylePropertyCombiner("border-image-width", borderImageWidth.properties, true);

        let borderOutsetUnits = Object.shallowCopy(this._units.defaultsSansPercent);
        borderOutsetUnits.basic = nonKeywordUnits.concat(borderOutsetUnits.basic);
        let borderImageOutset = generateBorderImagePropertyEditors("border-image-outset", this._keywords.defaults, borderOutsetUnits);
        properties.borderImageOutset = new WI.VisualStylePropertyCombiner("border-image-outset", borderImageOutset.properties, true);

        let borderImageSlice = generateBorderImagePropertyEditors("border-image-slice", this._keywords.defaults, ["%"].concat(nonKeywordUnits));
        borderImageSlice.properties.push(borderImageSliceFill);
        properties.borderImageSlice = new WI.VisualStylePropertyCombiner("border-image-slice", borderImageSlice.properties, true);

        let borderImagePropertiesTabController = new WI.VisualStyleTabbedPropertiesRow({
            "width": {title: WI.UIString("Width"), element: borderImageWidth.group.element, properties: [properties.borderImageWidth]},
            "outset": {title: WI.UIString("Outset"), element: borderImageOutset.group.element, properties: [properties.borderImageOutset]},
            "slice": {title: WI.UIString("Slice"), element: borderImageSlice.group.element, properties: [properties.borderImageSlice]}
        });

        let borderImageGroup = new WI.DetailsSectionGroup([borderImageSourceRow, borderImageRepeatRow, borderImagePropertiesTabController, borderImageWidth.group, borderImageOutset.group, borderImageSlice.group]);

        group.autocompleteCompatibleProperties = [properties.borderColor, properties.borderTopColor, properties.borderBottomColor, properties.borderLeftColor, properties.borderRightColor];

        this._populateSection(group, [borderGroup, borderImageGroup]);

        let allowedBorderValues = ["solid", "dashed", "dotted", "double", "groove", "inset", "outset", "ridge"];
        properties.borderWidth.addDependency(["border-top-style", "border-right-style", "border-bottom-style", "border-left-style"], allowedBorderValues);
        properties.borderColor.addDependency(["border-top-style", "border-right-style", "border-bottom-style", "border-left-style"], allowedBorderValues);
        properties.borderTopWidth.addDependency("border-top-style", allowedBorderValues);
        properties.borderTopColor.addDependency("border-top-style", allowedBorderValues);
        properties.borderRightWidth.addDependency("border-right-style", allowedBorderValues);
        properties.borderRightColor.addDependency("border-right-style", allowedBorderValues);
        properties.borderBottomWidth.addDependency("border-bottom-style", allowedBorderValues);
        properties.borderBottomColor.addDependency("border-bottom-style", allowedBorderValues);
        properties.borderLeftWidth.addDependency("border-left-style", allowedBorderValues);
        properties.borderLeftColor.addDependency("border-left-style", allowedBorderValues);
    }

    _populateOutlineSection()
    {
        let group = this._groups.outline;
        let properties = group.properties;

        let outlineSizeRow = new WI.DetailsSectionRow;

        properties.outlineStyle = new WI.VisualStyleKeywordPicker("outline-style", WI.UIString("Style"), this._keywords.borderStyle);
        properties.outlineWidth = new WI.VisualStyleNumberInputBox("outline-width", WI.UIString("Width"), this._keywords.borderWidth, this._units.defaults);

        outlineSizeRow.element.appendChild(properties.outlineStyle.element);
        outlineSizeRow.element.appendChild(properties.outlineWidth.element);

        let outlineStyleRow = new WI.DetailsSectionRow;

        properties.outlineColor = new WI.VisualStyleColorPicker("outline-color", WI.UIString("Color"));
        properties.outlineOffset = new WI.VisualStyleNumberInputBox("outline-offset", WI.UIString("Offset"), this._keywords.defaults, this._units.defaults, true);

        outlineStyleRow.element.appendChild(properties.outlineColor.element);
        outlineStyleRow.element.appendChild(properties.outlineOffset.element);

        group.autocompleteCompatibleProperties = [properties.outlineColor];

        let outlineGroup = new WI.DetailsSectionGroup([outlineSizeRow, outlineStyleRow]);
        this._populateSection(group, [outlineGroup]);

        let allowedOutlineValues = ["solid", "dashed", "dotted", "double", "groove", "inset", "outset", "ridge"];
        properties.outlineWidth.addDependency("outline-style", allowedOutlineValues);
        properties.outlineColor.addDependency("outline-style", allowedOutlineValues);
    }

    _populateBoxShadowSection()
    {
        let group = this._groups.boxShadow;
        let properties = group.properties;

        let boxShadowRow = new WI.DetailsSectionRow;

        properties.boxShadow = new WI.VisualStyleCommaSeparatedKeywordEditor("box-shadow");

        boxShadowRow.element.appendChild(properties.boxShadow.element);

        let boxShadowHRow = new WI.DetailsSectionRow;

        let boxShadowH = new WI.VisualStyleRelativeNumberSlider("box-shadow", WI.UIString("Left"), null, this._units.defaultsSansPercent, true);

        boxShadowHRow.element.appendChild(boxShadowH.element);

        let boxShadowVRow = new WI.DetailsSectionRow;

        let boxShadowV = new WI.VisualStyleRelativeNumberSlider("box-shadow", WI.UIString("Top"), null, this._units.defaultsSansPercent, true);

        boxShadowVRow.element.appendChild(boxShadowV.element);

        let boxShadowBlurRow = new WI.DetailsSectionRow;

        let boxShadowBlur = new WI.VisualStyleNumberInputBox("box-shadow", WI.UIString("Blur"), null, this._units.defaultsSansPercent);
        boxShadowBlur.optionalProperty = true;
        let boxShadowSpread = new WI.VisualStyleNumberInputBox("box-shadow", WI.UIString("Spread"), null, this._units.defaultsSansPercent, true);
        boxShadowSpread.optionalProperty = true;

        boxShadowBlurRow.element.appendChild(boxShadowBlur.element);
        boxShadowBlurRow.element.appendChild(boxShadowSpread.element);

        let boxShadowColorRow = new WI.DetailsSectionRow;

        let boxShadowColor = new WI.VisualStyleColorPicker("box-shadow", WI.UIString("Color"));
        let boxShadowInset = new WI.VisualStyleKeywordCheckbox("box-shadow", WI.UIString("Inset"), "Inset");
        boxShadowInset.optionalProperty = true;

        boxShadowColorRow.element.appendChild(boxShadowColor.element);
        boxShadowColorRow.element.appendChild(boxShadowInset.element);

        let boxShadowProperties = [boxShadowH, boxShadowV, boxShadowBlur, boxShadowSpread, boxShadowColor, boxShadowInset];
        let boxShadowPropertyCombiner = new WI.VisualStylePropertyCombiner("box-shadow", boxShadowProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, boxShadowPropertyCombiner, boxShadowProperties);
        properties.boxShadow.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.boxShadow, boxShadowPropertyCombiner);
        boxShadowPropertyCombiner.addEventListener(WI.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(boxShadowPropertyCombiner);
        properties.boxShadow.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [boxShadowColor];
        group.specifiedWidthProperties = [properties.boxShadow];

        let boxShadow = new WI.DetailsSectionGroup([boxShadowRow, boxShadowHRow, boxShadowVRow, boxShadowBlurRow, boxShadowColorRow]);
        this._populateSection(group, [boxShadow]);
    }

    _populateListStyleSection()
    {
        let group = this._groups.listStyle;
        let properties = group.properties;

        let listStyleTypeRow = new WI.DetailsSectionRow;

        properties.listStyleType = new WI.VisualStyleKeywordPicker("list-style-type", WI.UIString("Type"), {
            basic: this._keywords.defaults.concat(["None", "Circle", "Disc", "Square", "Decimal", "Lower Alpha", "Upper Alpha", "Lower Roman", "Upper Roman"]),
            advanced: ["Decimal Leading Zero", "Asterisks", "Footnotes", "Binary", "Octal", "Lower Hexadecimal", "Upper Hexadecimal", "Lower Latin", "Upper Latin", "Lower Greek", "Upper Greek", "Arabic Indic", "Hebrew", "Hiragana", "Katakana", "Hiragana Iroha", "Katakana Iroha", "CJK Earthly Branch", "CJK Heavenly Stem", "CJK Ideographic", "Bengali", "Cambodian", "Khmer", "Devanagari", "Gujarati", "Gurmukhi", "Kannada", "Lao", "Malayalam", "Mongolian", "Myanmar", "Oriya", "Persian", "Urdu", "Telugu", "Armenian", "Lower Armenian", "Upper Armenian", "Georgian", "Tibetan", "Thai", "Afar", "Hangul Consonant", "Hangul", "Lower Norwegian", "Upper Norwegian", "Ethiopic", "Ethiopic Halehame Gez", "Ethiopic Halehame Aa Et", "Ethiopic Halehame Aa Er", "Oromo", "Ethiopic Halehame Om Et", "Sidama", "Ethiopic Halehame Sid Et", "Somali", "Ethiopic Halehame So Et", "Amharic", "Ethiopic Halehame Am Et", "Tigre", "Ethiopic Halehame Tig", "Tigrinya Er", "Ethiopic Halehame Ti Er", "Tigrinya Et", "Ethiopic Halehame Ti Et", "Ethiopic Abegede", "Ethiopic Abegede Gez", "Amharic Abegede", "Ethiopic Abegede Am Et", "Tigrinya Er Abegede", "Ethiopic Abegede Ti Er", "Tigrinya Et Abegede", "Ethiopic Abegede Ti Et"]
        });

        properties.listStylePosition = new WI.VisualStyleKeywordIconList("list-style-position", WI.UIString("Position"), ["Outside", "Inside", "Initial"]);

        listStyleTypeRow.element.appendChild(properties.listStyleType.element);
        listStyleTypeRow.element.appendChild(properties.listStylePosition.element);

        let listStyleImageRow = new WI.DetailsSectionRow;

        properties.listStyleImage = new WI.VisualStyleURLInput("list-style-image", WI.UIString("Image"), this._keywords.defaults.concat(["None"]));

        listStyleImageRow.element.appendChild(properties.listStyleImage.element);

        let listStyle = new WI.DetailsSectionGroup([listStyleTypeRow, listStyleImageRow]);
        this._populateSection(group, [listStyle]);
    }

    _populateTransitionSection()
    {
        let group = this._groups.transition;
        let properties = group.properties;

        let transitionRow = new WI.DetailsSectionRow;

        properties.transition = new WI.VisualStyleCommaSeparatedKeywordEditor("transition", null, {
            "transition-property": "all",
            "transition-timing-function": "ease",
            "transition-duration": "0",
            "transition-delay": "0"
        });

        transitionRow.element.appendChild(properties.transition.element);

        let transitionPropertyRow = new WI.DetailsSectionRow;

        let transitionProperty = new WI.VisualStylePropertyNameInput("transition-property", WI.UIString("Property"));
        transitionProperty.masterProperty = true;
        let transitionTiming = new WI.VisualStyleTimingEditor("transition-timing-function", WI.UIString("Timing"), ["Linear", "Ease", "Ease In", "Ease Out", "Ease In Out"]);
        transitionTiming.optionalProperty = true;

        transitionPropertyRow.element.appendChild(transitionProperty.element);
        transitionPropertyRow.element.appendChild(transitionTiming.element);

        let transitionDurationRow = new WI.DetailsSectionRow;

        let transitionTimeKeywords = ["s", "ms"];
        let transitionDuration = new WI.VisualStyleNumberInputBox("transition-duration", WI.UIString("Duration"), null, transitionTimeKeywords);
        transitionDuration.optionalProperty = true;
        let transitionDelay = new WI.VisualStyleNumberInputBox("transition-delay", WI.UIString("Delay"), null, transitionTimeKeywords);
        transitionDelay.optionalProperty = true;

        transitionDurationRow.element.appendChild(transitionDuration.element);
        transitionDurationRow.element.appendChild(transitionDelay.element);

        let transitionProperties = [transitionProperty, transitionDuration, transitionTiming, transitionDelay];
        let transitionPropertyCombiner = new WI.VisualStylePropertyCombiner("transition", transitionProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, transitionPropertyCombiner, transitionProperties);
        properties.transition.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.transition, transitionPropertyCombiner);
        transitionPropertyCombiner.addEventListener(WI.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(transitionPropertyCombiner);
        properties.transition.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.autocompleteCompatibleProperties = [transitionProperty];
        group.specifiedWidthProperties = [properties.transition];

        let transitionGroup = new WI.DetailsSectionGroup([transitionRow, transitionPropertyRow, transitionDurationRow]);
        this._populateSection(group, [transitionGroup]);
    }

    _populateAnimationSection()
    {
        let group = this._groups.animation;
        let properties = group.properties;

        let animationRow = new WI.DetailsSectionRow;

        properties.animation = new WI.VisualStyleCommaSeparatedKeywordEditor("animation", null, {
            "animation-name": "none",
            "animation-timing-function": "ease",
            "animation-iteration-count": "1",
            "animation-duration": "0",
            "animation-delay": "0",
            "animation-direction": "normal",
            "animation-fill-mode": "none",
            "animation-play-state": "running"
        });

        animationRow.element.appendChild(properties.animation.element);

        let animationNameRow = new WI.DetailsSectionRow;

        let animationName = new WI.VisualStyleBasicInput("animation-name", WI.UIString("Name"), WI.UIString("Enter the name of a Keyframe"));
        animationName.masterProperty = true;

        animationNameRow.element.appendChild(animationName.element);

        let animationTimingRow = new WI.DetailsSectionRow;

        let animationTiming = new WI.VisualStyleTimingEditor("animation-timing-function", WI.UIString("Timing"), ["Linear", "Ease", "Ease In", "Ease Out", "Ease In Out"]);
        animationTiming.optionalProperty = true;
        let animationIterationCount = new WI.VisualStyleNumberInputBox("animation-iteration-count", WI.UIString("Iterations"), this._keywords.defaults.concat(["Infinite"]), null);
        animationIterationCount.optionalProperty = true;

        animationTimingRow.element.appendChild(animationTiming.element);
        animationTimingRow.element.appendChild(animationIterationCount.element);

        let animationDurationRow = new WI.DetailsSectionRow;

        let animationTimeKeywords = ["s", "ms"];
        let animationDuration = new WI.VisualStyleNumberInputBox("animation-duration", WI.UIString("Duration"), null, animationTimeKeywords);
        animationDuration.optionalProperty = true;
        let animationDelay = new WI.VisualStyleNumberInputBox("animation-delay", WI.UIString("Delay"), null, animationTimeKeywords);
        animationDelay.optionalProperty = true;

        animationDurationRow.element.appendChild(animationDuration.element);
        animationDurationRow.element.appendChild(animationDelay.element);

        let animationDirectionRow = new WI.DetailsSectionRow;

        let animationDirection = new WI.VisualStyleKeywordPicker("animation-direction", WI.UIString("Direction"), {
            basic: this._keywords.normal.concat(["Reverse"]),
            advanced: ["Alternate", "Alternate Reverse"]
        });
        animationDirection.optionalProperty = true;
        let animationFillMode = new WI.VisualStyleKeywordPicker("animation-fill-mode", WI.UIString("Fill Mode"), this._keywords.defaults.concat(["None", "Forwards", "Backwards", "Both"]));
        animationFillMode.optionalProperty = true;

        animationDirectionRow.element.appendChild(animationDirection.element);
        animationDirectionRow.element.appendChild(animationFillMode.element);

        let animationStateRow = new WI.DetailsSectionRow;

        let animationPlayState = new WI.VisualStyleKeywordIconList("animation-play-state", WI.UIString("State"), ["Running", "Paused", "Initial"]);
        animationPlayState.optionalProperty = true;

        animationStateRow.element.appendChild(animationPlayState.element);

        let animationProperties = [animationName, animationDuration, animationTiming, animationDelay, animationIterationCount, animationDirection, animationFillMode, animationPlayState];
        let animationPropertyCombiner = new WI.VisualStylePropertyCombiner("animation", animationProperties);

        let noRemainingCommaSeparatedEditorItems = this._noRemainingCommaSeparatedEditorItems.bind(this, animationPropertyCombiner, animationProperties);
        properties.animation.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.NoRemainingTreeItems, noRemainingCommaSeparatedEditorItems, this);

        let selectedCommaSeparatedEditorItemValueChanged = this._selectedCommaSeparatedEditorItemValueChanged.bind(this, properties.animation, animationPropertyCombiner);
        animationPropertyCombiner.addEventListener(WI.VisualStylePropertyEditor.Event.ValueDidChange, selectedCommaSeparatedEditorItemValueChanged, this);

        let commaSeparatedEditorTreeItemSelected = this._commaSeparatedEditorTreeItemSelected.bind(animationPropertyCombiner);
        properties.animation.addEventListener(WI.VisualStyleCommaSeparatedKeywordEditor.Event.TreeItemSelected, commaSeparatedEditorTreeItemSelected, this);

        group.specifiedWidthProperties = [properties.animation];

        let animationGroup = new WI.DetailsSectionGroup([animationRow, animationNameRow, animationTimingRow, animationDurationRow, animationDirectionRow, animationStateRow]);
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

WI.VisualStyleDetailsPanel.StyleDisabledSymbol = Symbol("visual-style-style-disabled");
WI.VisualStyleDetailsPanel.InitialPropertySectionTextListSymbol = Symbol("visual-style-initial-property-section-text");

// FIXME: Add information about each property as a form of documentation.  This is currently empty as
// we do not have permission to use the info on sites like MDN and have not written anything ourselves.
WI.VisualStyleDetailsPanel.propertyReferenceInfo = {};
