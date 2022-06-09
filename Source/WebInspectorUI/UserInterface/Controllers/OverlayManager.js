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

WI.OverlayManager = class OverlayManager extends WI.Object
{
    constructor()
    {
        super();

        this._gridOverlayForNodeMap = new Map;
        this._nextDefaultGridColorIndex = 0;
        this._gridColorForNodeMap = new WeakMap;
        this._gridColorSettingForNodeMap = new WeakMap;

        WI.settings.gridOverlayShowExtendedGridLines.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowLineNames.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowLineNumbers.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowTrackSizes.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowAreaNames.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleMainResourceDidChange, this);
    }

    // Public

    showGridOverlay(domNode, {color, initiator} = {})
    {
        console.assert(!domNode.destroyed, domNode);
        if (domNode.destroyed)
            return;

        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!color || color instanceof WI.Color, color);
        console.assert(domNode.layoutContextType === WI.DOMNode.LayoutContextType.Grid, domNode.layoutContextType);

        color ||= this.getGridColorForNode(domNode);
        let target = WI.assumingMainTarget();
        let commandArguments = {
            nodeId: domNode.id,
            gridColor: color.toProtocol(),
            showLineNames: WI.settings.gridOverlayShowLineNames.value,
            showLineNumbers: WI.settings.gridOverlayShowLineNumbers.value,
            showExtendedGridLines: WI.settings.gridOverlayShowExtendedGridLines.value,
            showTrackSizes: WI.settings.gridOverlayShowTrackSizes.value,
            showAreaNames: WI.settings.gridOverlayShowAreaNames.value,
        };
        target.DOMAgent.showGridOverlay.invoke(commandArguments);

        let overlay = {domNode, ...commandArguments, initiator};

        // The method to show the overlay will be called repeatedly while updating the grid overlay color. Avoid adding duplicate event listeners
        if (!this._gridOverlayForNodeMap.has(domNode))
            domNode.addEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);

        this._gridOverlayForNodeMap.set(domNode, overlay);

        this.dispatchEventToListeners(WI.OverlayManager.Event.GridOverlayShown, overlay);
    }

    hideGridOverlay(domNode)
    {
        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!domNode.destroyed, domNode);
        console.assert(domNode.layoutContextType === WI.DOMNode.LayoutContextType.Grid, domNode.layoutContextType);
        if (domNode.destroyed)
            return;

        let overlay = this._gridOverlayForNodeMap.take(domNode);
        if (!overlay)
            return;

        let target = WI.assumingMainTarget();
        target.DOMAgent.hideGridOverlay(domNode.id);

        domNode.removeEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);
        this.dispatchEventToListeners(WI.OverlayManager.Event.GridOverlayHidden, overlay);
    }

    hasVisibleGridOverlays()
    {
        return this._gridOverlayForNodeMap.size > 0;
    }

    isGridOverlayVisible(domNode)
    {
        return this._gridOverlayForNodeMap.has(domNode);
    }

    toggleGridOverlay(domNode, options)
    {
        if (this.isGridOverlayVisible(domNode))
            this.hideGridOverlay(domNode);
        else
            this.showGridOverlay(domNode, options);
    }

    getGridColorForNode(domNode)
    {
        let color = this._gridColorForNodeMap.get(domNode);
        if (color)
            return color;

        const defaultGridHSLColors = [
            [329, 91, 70],
            [207, 96, 69],
            [92, 90, 64],
            [291, 73, 68],
            [40, 97, 57],
        ];

        let colorSetting = this._gridColorSettingForNodeMap.get(domNode);
        if (!colorSetting) {
            let defaultColor = defaultGridHSLColors[this._nextDefaultGridColorIndex];
            this._nextDefaultGridColorIndex = (this._nextDefaultGridColorIndex + 1) % defaultGridHSLColors.length;

            let url = domNode.ownerDocument.documentURL || WI.networkManager.mainFrame.url;
            colorSetting = new WI.Setting(`grid-overlay-color-${url.hash}-${domNode.path().hash}`, defaultColor);
            this._gridColorSettingForNodeMap.set(domNode, colorSetting);
        }

        color = new WI.Color(WI.Color.Format.HSL, colorSetting.value);
        this._gridColorForNodeMap.set(domNode, color);

        return color;
    }

    setGridColorForNode(domNode, color)
    {
        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(color instanceof WI.Color, color);

        let colorSetting = this._gridColorSettingForNodeMap.get(domNode);
        console.assert(colorSetting, "There should already be a setting created form a previous call to getGridColorForNode()");
        colorSetting.value = color.hsl;

        this._gridColorForNodeMap.set(domNode, color);
    }

    // Private

    _handleLayoutContextTypeChanged(event)
    {
        let domNode = event.target;
        console.assert(domNode.layoutContextType !== WI.DOMNode.LayoutContextType.Grid, domNode);

        domNode.removeEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);

        // When the context type changes, the overlay is automatically hidden on the backend. Here, we only update the map and notify listeners.
        let overlay = this._gridOverlayForNodeMap.take(domNode);
        this.dispatchEventToListeners(WI.OverlayManager.Event.GridOverlayHidden, overlay);
    }

    _handleGridSettingChanged(event)
    {
        for (let [domNode, overlay] of this._gridOverlayForNodeMap) {
            // Refresh all shown overlays. Latest settings values will be used.
            this.showGridOverlay(domNode, {color: overlay.color, initiator: overlay.initiator});
        }
    }

    _handleMainResourceDidChange(event)
    {
        // Consider the following scenario:
        // 1. Click on the badge of an element with `display: grid`. The 1st overlay color is used.
        // 2. Reload the webpage.
        // 3. Click on the badge of the same element.
        //
        // We should see the same 1st default overlay color. If we don't reset _nextDefaultGridColorIndex,
        // the 2nd default color would be used instead.
        //
        // `domNode.id` is different for the same DOM element after page reload.
        this._nextDefaultGridColorIndex = 0;
    }
};

WI.OverlayManager.Event = {
    GridOverlayShown: "overlay-manager-grid-overlay-shown",
    GridOverlayHidden: "overlay-manager-grid-overlay-hidden",
};
