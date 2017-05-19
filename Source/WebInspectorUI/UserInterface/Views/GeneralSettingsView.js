/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WebInspector.GeneralSettingsView = class GeneralSettingsView extends WebInspector.SettingsView
{
    constructor()
    {
        super("general", WebInspector.UIString("General"));
    }

    // Protected

    initialLayout()
    {
        const indentValues = [WebInspector.UIString("Tabs"), WebInspector.UIString("Spaces")];

        let [/* unused */, indentEditor] = this.addGroupWithCustomSetting(WebInspector.UIString("Prefer indent using:"), WebInspector.SettingEditor.Type.Select, {values: indentValues});
        indentEditor.value = indentValues[WebInspector.settings.indentWithTabs.value ? 0 : 1];
        indentEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => {
            WebInspector.settings.indentWithTabs.value = indentEditor.value === indentValues[0];
        });

        const widthLabel = WebInspector.UIString("spaces");
        const widthOptions = {min: 1};

        this.addSetting(WebInspector.UIString("Tab width:"), WebInspector.settings.tabSize, widthLabel, widthOptions);
        this.addSetting(WebInspector.UIString("Indent width:"), WebInspector.settings.indentUnit, widthLabel, widthOptions);

        this.addSetting(WebInspector.UIString("Line wrapping:"), WebInspector.settings.enableLineWrapping, WebInspector.UIString("Wrap lines to editor width"));

        let showGroup = this.addGroup(WebInspector.UIString("Show:"));
        showGroup.addSetting(WebInspector.settings.showWhitespaceCharacters, WebInspector.UIString("Whitespace characters"));
        showGroup.addSetting(WebInspector.settings.showInvalidCharacters, WebInspector.UIString("Invalid characters"));

        this.addSeparator();

        let stylesEditingGroup = this.addGroup(WebInspector.UIString("Styles Editing:"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesShowInlineWarnings, WebInspector.UIString("Show inline warnings"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesInsertNewline, WebInspector.UIString("Automatically insert newline"));
        stylesEditingGroup.addSetting(WebInspector.settings.stylesSelectOnFirstClick, WebInspector.UIString("Select text on first click"));

        this.addSeparator();

        this.addSetting(WebInspector.UIString("Network:"), WebInspector.settings.clearNetworkOnNavigate, WebInspector.UIString("Clear when page navigates"));

        this.addSeparator();

        this.addSetting(WebInspector.UIString("Console:"), WebInspector.settings.clearLogOnNavigate, WebInspector.UIString("Clear when page navigates"));

        this.addSeparator();

        this.addSetting(WebInspector.UIString("Debugger:"), WebInspector.settings.showScopeChainOnPause, WebInspector.UIString("Show Scope Chain on pause"));

        this.addSeparator();

        const zoomLevels = [0.6, 0.8, 1, 1.2, 1.4, 1.6, 1.8, 2, 2.2, 2.4];
        const zoomValues = zoomLevels.map((level) => [level, Number.percentageString(level, 0)]);

        let [/* unused */, zoomEditor] = this.addGroupWithCustomSetting(WebInspector.UIString("Zoom:"), WebInspector.SettingEditor.Type.Select, {values: zoomValues});
        zoomEditor.value = WebInspector.getZoomFactor();
        zoomEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => { WebInspector.setZoomFactor(zoomEditor.value); });
        WebInspector.settings.zoomFactor.addEventListener(WebInspector.Setting.Event.Changed, () => { zoomEditor.value = WebInspector.getZoomFactor().maxDecimals(2); });

        this.addSeparator();

        // This setting is only ever shown in engineering builds, so its strings are unlocalized.
        const layoutDirectionValues = [
            [WebInspector.LayoutDirection.System, WebInspector.unlocalizedString("System Default")],
            [WebInspector.LayoutDirection.LTR, WebInspector.unlocalizedString("Left to Right (LTR)")],
            [WebInspector.LayoutDirection.RTL, WebInspector.unlocalizedString("Right to Left (RTL)")],
        ];

        let [layoutDirectionGroup, layoutDirectionEditor] = this.addGroupWithCustomSetting(WebInspector.unlocalizedString("Layout Direction:"), WebInspector.SettingEditor.Type.Select, {values: layoutDirectionValues});
        this._layoutDirectionGroup = layoutDirectionGroup;
        layoutDirectionEditor.value = WebInspector.settings.layoutDirection.value;
        layoutDirectionEditor.addEventListener(WebInspector.SettingEditor.Event.ValueDidChange, () => { WebInspector.setLayoutDirection(layoutDirectionEditor.value); });
    }

    layout()
    {
        this._layoutDirectionGroup.element.classList.toggle("hidden", !WebInspector.isDebugUIEnabled());
    }
};
