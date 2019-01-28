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
            this.element.textContent = WI.UIString("CSS hasn't been modified.");
            return;
        }

        let indent = WI.indentString();

        let appendPropertyElement = (tagName, text) => {
            let propertyElement = document.createElement(tagName);
            propertyElement.className = "css-property";
            propertyElement.append(indent, text);
            this.element.append(propertyElement, "\n");
        };

        for (let cssRule of cssRules) {
            let selectorElement = document.createElement("span");
            selectorElement.append(cssRule.selectorText, " {\n");
            this.element.append(selectorElement);

            let initialCSSProperties = cssRule.initialState.style.visibleProperties;
            let cssProperties = cssRule.style.visibleProperties;

            Array.diffArrays(initialCSSProperties, cssProperties, (cssProperty, action) => {
                if (action === 0) {
                    if (cssProperty.modified) {
                        appendPropertyElement("del", cssProperty.initialState.formattedText);
                        appendPropertyElement("ins", cssProperty.formattedText);
                    } else
                        appendPropertyElement("span", cssProperty.formattedText);
                } else if (action === 1)
                    appendPropertyElement("ins", cssProperty.formattedText);
                else if (action === -1)
                    appendPropertyElement("del", cssProperty.formattedText);
            });

            this.element.append("}\n\n");
        }
    }

    // Private

    _mainResourceDidChange(event)
    {
        if (!event.target.isMainFrame())
            return;

        this.needsLayout();
    }
};
