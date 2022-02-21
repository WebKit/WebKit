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

        this._overlayForNodeMap = new Map;
        this._nextDefaultGridColorIndex = 0;
        this._nextDefaultFlexColorIndex = 0;
        this._colorForNodeMap = new WeakMap;
        this._colorSettingForNodeMap = new WeakMap;

        WI.settings.gridOverlayShowExtendedGridLines.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowLineNames.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowLineNumbers.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowTrackSizes.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.settings.gridOverlayShowAreaNames.addEventListener(WI.Setting.Event.Changed, this._handleGridSettingChanged, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleMainResourceDidChange, this);
    }

    // Public

    showOverlay(domNode, {color, initiator} = {})
    {
        console.assert(!domNode.destroyed, domNode);
        if (domNode.destroyed)
            return;

        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!color || color instanceof WI.Color, color);
        console.assert(Object.values(WI.DOMNode.LayoutContextType).includes(domNode.layoutContextType), domNode);

        color ||= this.getColorForNode(domNode);
        let target = WI.assumingMainTarget();
        let commandArguments = {nodeId: domNode.id};

        switch (domNode.layoutContextType) {
        case WI.DOMNode.LayoutContextType.Grid:
            commandArguments.gridColor = color.toProtocol();
            commandArguments.showLineNames = WI.settings.gridOverlayShowLineNames.value;
            commandArguments.showLineNumbers = WI.settings.gridOverlayShowLineNumbers.value;
            commandArguments.showExtendedGridLines = WI.settings.gridOverlayShowExtendedGridLines.value;
            commandArguments.showTrackSizes = WI.settings.gridOverlayShowTrackSizes.value;
            commandArguments.showAreaNames = WI.settings.gridOverlayShowAreaNames.value;
            target.DOMAgent.showGridOverlay.invoke(commandArguments);
            break;

        case WI.DOMNode.LayoutContextType.Flex:
            commandArguments.flexColor = color.toProtocol();
            target.DOMAgent.showFlexOverlay.invoke(commandArguments);
            break;
        }

        let overlay = {domNode, ...commandArguments, initiator};

        // The method to show the overlay will be called repeatedly while updating the grid overlay color. Avoid adding duplicate event listeners
        if (!this._overlayForNodeMap.has(domNode))
            domNode.addEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);

        this._overlayForNodeMap.set(domNode, overlay);

        this.dispatchEventToListeners(WI.OverlayManager.Event.OverlayShown, overlay);
    }

    hideOverlay(domNode)
    {
        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!domNode.destroyed, domNode);
        console.assert(Object.values(WI.DOMNode.LayoutContextType).includes(domNode.layoutContextType), domNode);
        if (domNode.destroyed)
            return;

        let overlay = this._overlayForNodeMap.take(domNode);
        if (!overlay)
            return;

        let target = WI.assumingMainTarget();

        switch (domNode.layoutContextType) {
        case WI.DOMNode.LayoutContextType.Grid:
            target.DOMAgent.hideGridOverlay(domNode.id);
            break;

        case WI.DOMNode.LayoutContextType.Flex:
            target.DOMAgent.hideFlexOverlay(domNode.id);
            break;
        }

        domNode.removeEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);
        this.dispatchEventToListeners(WI.OverlayManager.Event.OverlayHidden, overlay);
    }

    hasVisibleGridOverlays()
    {
        for (let domNode of this._overlayForNodeMap.keys()) {
            if (domNode.layoutContextType === WI.DOMNode.LayoutContextType.Grid)
                return true;
        }
        return false;
    }

    hasVisibleOverlay(domNode)
    {
        return this._overlayForNodeMap.has(domNode);
    }

    toggleOverlay(domNode, options)
    {
        if (this.hasVisibleOverlay(domNode))
            this.hideOverlay(domNode);
        else
            this.showOverlay(domNode, options);
    }

    getColorForNode(domNode)
    {
        let color = this._colorForNodeMap.get(domNode);
        if (color)
            return color;

        let colorSetting = this._colorSettingForNodeMap.get(domNode);
        if (!colorSetting) {
            let nextColorIndex;
            switch (domNode.layoutContextType) {
            case WI.DOMNode.LayoutContextType.Grid:
                nextColorIndex = this._nextDefaultGridColorIndex;
                this._nextDefaultGridColorIndex = (nextColorIndex + 1) % WI.OverlayManager._defaultHSLColors.length;
                break;

            case WI.DOMNode.LayoutContextType.Flex:
                nextColorIndex = this._nextDefaultFlexColorIndex;
                this._nextDefaultFlexColorIndex = (nextColorIndex + 1) % WI.OverlayManager._defaultHSLColors.length;
                break;
            }
            let defaultColor = WI.OverlayManager._defaultHSLColors[nextColorIndex];

            let url = domNode.ownerDocument.documentURL || WI.networkManager.mainFrame.url;
            colorSetting = new WI.Setting(`overlay-color-${url.hash}-${domNode.path().hash}`, defaultColor);
            this._colorSettingForNodeMap.set(domNode, colorSetting);
        }

        color = new WI.Color(WI.Color.Format.HSL, colorSetting.value);
        this._colorForNodeMap.set(domNode, color);

        return color;
    }

    setColorForNode(domNode, color)
    {
        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(color instanceof WI.Color, color);

        let colorSetting = this._colorSettingForNodeMap.get(domNode);
        console.assert(colorSetting, "There should already be a setting created form a previous call to getColorForNode()");
        colorSetting.value = color.hsl;

        this._colorForNodeMap.set(domNode, color);
    }

    // Private

    _handleLayoutContextTypeChanged(event)
    {
        let domNode = event.target;

        domNode.removeEventListener(WI.DOMNode.Event.LayoutContextTypeChanged, this._handleLayoutContextTypeChanged, this);

        // When the context type changes, the overlay is automatically hidden on the backend (even if it changes from Grid to Flex, or vice-versa).
        // Here, we only update the map and notify listeners.
        let overlay = this._overlayForNodeMap.take(domNode);
        this.dispatchEventToListeners(WI.OverlayManager.Event.OverlayHidden, overlay);
    }

    _handleGridSettingChanged(event)
    {
        for (let [domNode, overlay] of this._overlayForNodeMap) {
            if (domNode.layoutContextType === WI.DOMNode.LayoutContextType.Grid)
                this.showOverlay(domNode, {color: overlay.color, initiator: overlay.initiator});
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
        this._nextDefaultFlexColorIndex = 0;
    }
};

WI.OverlayManager._defaultHSLColors = [
    [329, 91, 70],
    [207, 96, 69],
    [92, 90, 64],
    [291, 73, 68],
    [40, 97, 57],
];

WI.OverlayManager.Event = {
    OverlayShown: "overlay-manager-overlay-shown",
    OverlayHidden: "overlay-manager-overlay-hidden",
};
