/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.SearchUtilities = class SearchUtilities {
    static get defaultSettings()
    {
        return {
            caseSensitive: WI.settings.searchCaseSensitive,
            regularExpression: WI.settings.searchRegularExpression,
        };
    }

    static createSettings(namePrefix, options = {})
    {
        let settings = {};
        for (let [key, defaultSetting] of Object.entries(WI.SearchUtilities.defaultSettings)) {
            let setting = new WI.Setting(namePrefix + "-" + defaultSetting.name, defaultSetting.value);
            defaultSetting.addEventListener(WI.Setting.Event.Changed, (event) => {
                setting.value = defaultSetting.value;
            });
            settings[key] = setting;

            if (options.handleChanged)
                setting.addEventListener(WI.Setting.Event.Changed, options.handleChanged);
        }
        return settings;
    }

    static regExpForString(query, settings = {})
    {
        function checkSetting(setting) {
            return setting instanceof WI.Setting ? setting.value : !!setting;
        }

        if (!checkSetting(settings.regularExpression))
            query = simpleGlobStringToRegExp(query);

        let flags = "g";
        if (!checkSetting(settings.caseSensitive))
            flags += "i";

        return new RegExp(query, flags);
    }

    static createSettingsButton(settings)
    {
        console.assert(!isEmptyObject(settings));

        let button = document.createElement("button");
        button.addEventListener("click", (event) => {
            event.stop();

            let contextMenu = WI.ContextMenu.createFromEvent(event);

            if (settings.caseSensitive) {
                contextMenu.appendCheckboxItem(WI.UIString("Case Sensitive", "Case Sensitive @ Context Menu", "Context menu label for whether searches should be case sensitive."), () => {
                    settings.caseSensitive.value = !settings.caseSensitive.value;
                }, settings.caseSensitive.value);
            }

            if (settings.regularExpression) {
                contextMenu.appendCheckboxItem(WI.UIString("Regular Expression", "Regular Expression @ Context Menu", "Context menu label for whether searches should be treated as regular expressions."), () => {
                    settings.regularExpression.value = !settings.regularExpression.value;
                }, settings.regularExpression.value);
            }

            contextMenu.show();
        });
        button.classList.add("search-settings");
        button.tabIndex = -1;

        button.appendChild(WI.ImageUtilities.useSVGSymbol("Images/Gear.svg", "glyph"));

        function toggleActive() {
            button.classList.toggle("active", Object.values(settings).some((setting) => !!setting.value));
        }
        settings.caseSensitive.addEventListener(WI.Setting.Event.Changed, toggleActive);
        settings.regularExpression.addEventListener(WI.Setting.Event.Changed, toggleActive);
        toggleActive();

        return button;
    }
};
