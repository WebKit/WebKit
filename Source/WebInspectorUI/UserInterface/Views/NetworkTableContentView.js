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

WI.NetworkTableContentView = class NetworkTableContentView extends WI.ContentView
{
    constructor(representedObject, extraArguments)
    {
        super(representedObject);

        // FIXME: Network Table.
        // FIXME: Network Timeline.
        // FIXME: Filter text field.
        // FIXME: Filter scope bar.
        // FIXME: Throttling.
        // FIXME: HAR Export.

        // COMPATIBILITY (iOS 10.3): Network.setDisableResourceCaching did not exist.
        if (window.NetworkAgent && NetworkAgent.setResourceCachingDisabled) {
            let toolTipForDisableResourceCache = WI.UIString("Ignore the resource cache when loading resources");
            let activatedToolTipForDisableResourceCache = WI.UIString("Use the resource cache when loading resources");
            this._disableResourceCacheNavigationItem = new WI.ActivateButtonNavigationItem("disable-resource-cache", toolTipForDisableResourceCache, activatedToolTipForDisableResourceCache, "Images/IgnoreCaches.svg", 16, 16);
            this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;

            this._disableResourceCacheNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleDisableResourceCache, this);
            WI.resourceCachingDisabledSetting.addEventListener(WI.Setting.Event.Changed, this._resourceCachingDisabledSettingChanged, this);
        }

        this._clearNetworkItemsNavigationItem = new WI.ButtonNavigationItem("clear-network-items", WI.UIString("Clear Network Items (%s)").format(WI.clearKeyboardShortcut.displayName), "Images/NavigationItemClear.svg", 16, 16);
        this._clearNetworkItemsNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, () => this.reset());
    }

    // Public

    get selectionPathComponents()
    {
        return null;
    }

    get navigationItems()
    {
        let items = [];

        if (this._disableResourceCacheNavigationItem)
            items.push(this._disableResourceCacheNavigationItem);
        items.push(this._clearNetworkItemsNavigationItem);

        return items;
    }

    shown()
    {
        super.shown();

        // FIXME: Implement.
    }

    hidden()
    {
        // FIXME: Implment.

        super.hidden();
    }

    closed()
    {
        // FIXME: Implement

        super.closed();
    }

    reset()
    {
        // FIXME: Implement
    }

    // Protected

    layout()
    {
        // FIXME: Implement
    }

    handleClearShortcut(event)
    {
        this.reset();
    }

    // Private

    _resourceCachingDisabledSettingChanged()
    {
        this._disableResourceCacheNavigationItem.activated = WI.resourceCachingDisabledSetting.value;
    }

    _toggleDisableResourceCache()
    {
        WI.resourceCachingDisabledSetting.value = !WI.resourceCachingDisabledSetting.value;
    }
};
