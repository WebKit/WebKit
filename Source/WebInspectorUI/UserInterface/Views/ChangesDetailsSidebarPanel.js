/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.ChangesDetailsSidebarPanel = class ChangesDetailsSidebarPanel extends WI.DetailsSidebarPanel
{
    constructor()
    {
        super("changes-details", WI.UIString("Changes"));

        this.element.classList.add("changes-panel");
    }

    // Public

    inspect(objects)
    {
        return true;
    }

    supportsDOMNode(nodeToInspect)
    {
        // Display Changes panel regardless of the selected DOM node.
        return true;
    }

    shown()
    {
        // `shown` may get called before initialLayout when Elements tab is opened.
        // When Changes panel is selected, `shown` is called and this time it's after initialLayout.
        if (this.didInitialLayout) {
            this.needsLayout();
            WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        }

        super.shown();
    }

    detached()
    {
        super.detached();

        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Protected

    layout()
    {
        super.layout();

        this.element.removeChildren();

        let cssRules = WI.cssManager.modifiedCSSRules;

        this.element.classList.toggle("empty", !cssRules.length);
        if (!cssRules.length) {
            this.element.textContent = WI.UIString("No CSS Changes");
            return;
        }

        let rulesForStylesheet = new Map();
        for (let cssRule of cssRules) {
            let cssRules = rulesForStylesheet.get(cssRule.ownerStyleSheet);
            if (!cssRules) {
                cssRules = [];
                rulesForStylesheet.set(cssRule.ownerStyleSheet, cssRules);
            }
            cssRules.push(cssRule);
        }

        for (let [styleSheet, cssRules] of rulesForStylesheet) {
            let resourceSection = this.element.appendChild(document.createElement("section"));
            resourceSection.classList.add("resource-section");

            let resourceHeader = resourceSection.appendChild(document.createElement("div"));
            resourceHeader.classList.add("header");
            resourceHeader.append(this._createLocationLink(styleSheet));

            for (let cssRule of cssRules)
                resourceSection.append(this._createRuleElement(cssRule));
        }
    }

    // Private

    _createRuleElement(cssRule)
    {
        let ruleElement = document.createElement("div");
        ruleElement.classList.add("css-rule");

        let selectorElement = ruleElement.appendChild(document.createElement("span"));
        selectorElement.classList.add("selector-line");
        selectorElement.append(cssRule.selectorText, " {\n");

        let appendProperty = (cssProperty, className) => {
            let propertyLineElement = ruleElement.appendChild(document.createElement("div"));
            propertyLineElement.classList.add("css-property-line", className);
            let stylePropertyView = new WI.SpreadsheetStyleProperty(null, cssProperty, {readOnly: true});
            propertyLineElement.append(WI.indentString(), stylePropertyView.element, "\n");
        };

        let initialCSSProperties = cssRule.initialState.style.visibleProperties;
        let cssProperties = cssRule.style.visibleProperties;

        Array.diffArrays(initialCSSProperties, cssProperties, (cssProperty, action) => {
            if (action === 0) {
                if (cssProperty.modified) {
                    appendProperty(cssProperty.initialState, "removed");
                    appendProperty(cssProperty, "added");
                } else
                    appendProperty(cssProperty, "unchanged");
            } else if (action === 1)
                appendProperty(cssProperty, "added");
            else if (action === -1)
                appendProperty(cssProperty, "removed");
        });

        let closeBraceElement = document.createElement("span");
        closeBraceElement.className = "close-brace";
        closeBraceElement.textContent = "}";

        ruleElement.append(closeBraceElement, "\n");
        return ruleElement;
    }

    _createLocationLink(styleSheet)
    {
        const options = {
            nameStyle: WI.SourceCodeLocation.NameStyle.Short,
            columnStyle: WI.SourceCodeLocation.ColumnStyle.Hidden,
            dontFloat: true,
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };

        const lineNumber = 0;
        const columnNumber = 0;
        let sourceCodeLocation = styleSheet.createSourceCodeLocation(lineNumber, columnNumber);
        return WI.createSourceCodeLocationLink(sourceCodeLocation, options);
    }

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this.needsLayout();
    }
};
