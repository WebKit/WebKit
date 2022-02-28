/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

WI.CSSGridNodeOverlayListSection = class CSSGridNodeOverlayListSection extends WI.NodeOverlayListSection
{
    // Protected

    get sectionLabel()
    {
        return WI.UIString("Grid Overlays", "Page Overlays @ Layout Sidebar Section Header", "Heading for list of grid nodes");
    }

    initialLayout()
    {
        let settingsGroup = new WI.SettingsGroup(WI.UIString("Page Overlay Options", "Page Overlay Options @ Layout Panel Grid Section Header", "Heading for list of grid overlay options"));
        this.element.append(settingsGroup.element);

        settingsGroup.addSetting(WI.settings.gridOverlayShowTrackSizes, WI.UIString("Track Sizes", "Track sizes @ Layout Panel Overlay Options", "Label for option to toggle the track sizes setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowLineNumbers, WI.UIString("Line Numbers", "Line numbers @ Layout Panel Overlay Options", "Label for option to toggle the line numbers setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowLineNames, WI.UIString("Line Names", "Line names @ Layout Panel Overlay Options", "Label for option to toggle the line names setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowAreaNames, WI.UIString("Area Names", "Area names @ Layout Panel Overlay Options", "Label for option to toggle the area names setting for CSS grid overlays"));
        settingsGroup.addSetting(WI.settings.gridOverlayShowExtendedGridLines, WI.UIString("Extended Grid Lines", "Show extended lines @ Layout Panel Overlay Options", "Label for option to toggle the extended lines setting for CSS grid overlays"));

        super.initialLayout();
    }
};
