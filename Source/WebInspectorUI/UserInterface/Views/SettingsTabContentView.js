/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Devin Rousso <dcrousso+webkit@gmail.com>. All rights reserved.
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

WebInspector.SettingsTabContentView = class SettingsTabContentView extends WebInspector.TabContentView
{
    constructor(identifier)
    {
        let tabBarItem = new WebInspector.PinnedTabBarItem("Images/Gear.svg", WebInspector.UIString("Open Settings"));

        super(identifier || "settings", "settings", tabBarItem);

        // Ensures that the Settings tab is displayable from a pinned tab bar item.
        tabBarItem.representedObject = this;
    }

    static tabInfo()
    {
        return {
            image: "Images/Gear.svg",
            title: WebInspector.UIString("Settings"),
        };
    }

    static isEphemeral()
    {
        return true;
    }

    static shouldSaveTab()
    {
        return false;
    }

    // Public

    get type()
    {
        return WebInspector.SettingsTabContentView.Type;
    }

    initialLayout()
    {
        let header = this.element.createChild("div", "header");
        header.textContent = WebInspector.UIString("Settings");

        let createContainer = (title, createValueController) => {
            let container = this.element.createChild("div", "setting-container");

            let titleContainer = container.createChild("div", "setting-name");
            titleContainer.textContent = title;

            let valueControllerContainer = container.createChild("div", "setting-value-controller");
            let labelElement = valueControllerContainer.createChild("label");
            if (typeof createValueController === "function")
                createValueController(labelElement);
        };

        let createCheckbox = (setting) => {
            let checkbox = document.createElement("input");
            checkbox.type = "checkbox";
            checkbox.checked = setting.value;
            checkbox.addEventListener("change", (event) => {
                setting.value = checkbox.checked;
            });
            return checkbox;
        };

        createContainer(WebInspector.UIString("Prefer indent using:"), (valueControllerContainer) => {
            let select = valueControllerContainer.createChild("select");
            select.addEventListener("change", (event) => {
                WebInspector.settings.indentWithTabs.value = select.value === "tabs";
            });

            let tabsOption = select.createChild("option");
            tabsOption.value = "tabs";
            tabsOption.textContent = WebInspector.UIString("Tabs");
            tabsOption.selected = WebInspector.settings.indentWithTabs.value;

            let spacesOption = select.createChild("option");
            spacesOption.value = "spaces";
            spacesOption.textContent = WebInspector.UIString("Spaces");
            spacesOption.selected = !WebInspector.settings.indentWithTabs.value;
        });

        createContainer(WebInspector.UIString("Tab width:"), (valueControllerContainer) => {
            let input = valueControllerContainer.createChild("input");
            input.type = "number";
            input.min = 1;
            input.value = WebInspector.settings.tabSize.value;
            input.addEventListener("change", (event) => {
                WebInspector.settings.tabSize.value = parseInt(input.value) || 4;
            });

            valueControllerContainer.append(WebInspector.UIString("spaces"));
        });

        createContainer(WebInspector.UIString("Indent width:"), (valueControllerContainer) => {
            let input = valueControllerContainer.createChild("input");
            input.type = "number";
            input.min = 1;
            input.value = WebInspector.settings.indentUnit.value;
            input.addEventListener("change", (event) => {
                WebInspector.settings.indentUnit.value = parseInt(input.value) || 4;
            });

            valueControllerContainer.append(WebInspector.UIString("spaces"));
        });

        createContainer(WebInspector.UIString("Line wrapping:"), (valueControllerContainer) => {
            let checkbox = createCheckbox(WebInspector.settings.enableLineWrapping);
            valueControllerContainer.appendChild(checkbox);

            valueControllerContainer.append(WebInspector.UIString("Wrap lines to editor width"));
        });

        createContainer(WebInspector.UIString("Whitespace Characters:"), (valueControllerContainer) => {
            let checkbox = createCheckbox(WebInspector.settings.showWhitespaceCharacters);
            valueControllerContainer.appendChild(checkbox);

            valueControllerContainer.append(WebInspector.UIString("Visible"));
        });

        createContainer(WebInspector.UIString("Invalid Characters:"), (valueControllerContainer) => {
            let checkbox = createCheckbox(WebInspector.settings.showInvalidCharacters);
            valueControllerContainer.appendChild(checkbox);

            valueControllerContainer.append(WebInspector.UIString("Visible"));
        });

        this.element.appendChild(document.createElement("br"));

        createContainer(WebInspector.UIString("Zoom:"), (valueControllerContainer) => {
            let select = valueControllerContainer.createChild("select");
            select.addEventListener("change", (event) => {
                WebInspector.setZoomFactor(select.value);
            });

            let currentZoom = WebInspector.getZoomFactor().maxDecimals(1);
            [0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2, 2.4].forEach((level) => {
                let option = select.createChild("option");
                option.value = level;
                option.textContent = `${Math.round(level * 100)}%`;
                option.selected = currentZoom === level;
            });
        });
    }
};

WebInspector.SettingsTabContentView.Type = "settings";
