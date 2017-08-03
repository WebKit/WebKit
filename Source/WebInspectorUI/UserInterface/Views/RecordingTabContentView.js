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

WI.RecordingTabContentView = class RecordingTabContentView extends WI.ContentBrowserTabContentView
{
    constructor()
    {
        let {image, title} = WI.RecordingTabContentView.tabInfo();
        let tabBarItem = new WI.GeneralTabBarItem(image, title);

        const navigationSidebarPanelConstructor = WI.RecordingNavigationSidebarPanel;
        const detailsSidebarPanelConstructors = [WI.RecordingStateDetailsSidebarPanel, WI.RecordingTraceDetailsSidebarPanel, WI.CanvasDetailsSidebarPanel];
        const disableBackForward = true;
        let flexibleNavigationItem = new WI.ScrubberNavigationItem;
        super("recording", "recording", tabBarItem, navigationSidebarPanelConstructor, detailsSidebarPanelConstructors, disableBackForward, flexibleNavigationItem);

        this._visualActionIndexes = [];

        this._scrubberNavigationItem = flexibleNavigationItem;
        this._scrubberNavigationItem.value = 0;
        this._scrubberNavigationItem.disabled = true;
        this._scrubberNavigationItem.addEventListener(WI.ScrubberNavigationItem.Event.ValueChanged, this._scrubberNavigationItemValueChanged, this);

        this.navigationSidebarPanel.addEventListener(WI.RecordingNavigationSidebarPanel.Event.Import, this._navigationSidebarImport, this);
        this.navigationSidebarPanel.contentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._navigationSidebarTreeOutlineSelectionChanged, this);

        this._recording = null;
    }

    // Static

    static tabInfo()
    {
        return {
            image: "Images/Recording.svg",
            title: WI.UIString("Recording"),
        };
    }

    static isTabAllowed()
    {
        return !!window.CanvasAgent;
    }

    // Public

    get type()
    {
        return WI.RecordingTabContentView.Type;
    }

    canShowRepresentedObject(representedObject)
    {
        // Once a recording has been loaded for this tab, do not allow another one to be loaded.
        // This will cause new tabs to be opened for each recording, which is the desired behavior.
        if (this._recording)
            return false;

        return representedObject instanceof WI.Recording;
    }

    showRepresentedObject(representedObject, cookie)
    {
        super.showRepresentedObject(representedObject, cookie);

        this._recording = representedObject;

        this._visualActionIndexes = [];
        representedObject.actions.forEach((action, i) => {
            if (action.isVisual)
                this._visualActionIndexes.push(i);
        });

        this._scrubberNavigationItem.value = 0;
        this._scrubberNavigationItem.min = 0;
        this._scrubberNavigationItem.max = representedObject.actions.length - 1;
        this._scrubberNavigationItem.disabled = false;

        this.navigationSidebarPanel.recording = representedObject;

        this._updateActionIndex(this._scrubberNavigationItem.value);
    }

    // Protected

    restoreStateFromCookie(restorationType)
    {
        // Don't attempt to do anything to this tab.
    }

    saveStateToCookie(cookie)
    {
        // Don't attempt to do anything to this tab.
    }

    closed()
    {
        super.closed();

        for (let detailsSidebarPanel of this.detailsSidebarPanels)
            detailsSidebarPanel.recording = null;

        this.navigationSidebarPanel.recording = null;
    }

    // Private

    _updateActionIndex(index, options = {})
    {
        this._scrubberNavigationItem.value = index;

        options.onCompleteCallback = (context) => {
            for (let detailsSidebarPanel of this.detailsSidebarPanels) {
                if (detailsSidebarPanel.updateActionIndex)
                    detailsSidebarPanel.updateActionIndex(index, context, options);
            }
        };

        this.contentBrowser.currentContentView.updateActionIndex(index, options);

        // This must be placed last, as it is possible for the update of the NavigationSidebarPanel
        // to trigger another update of the index, such as if the selected index is not expanded.
        if (!options.suppressNavigationUpdate)
            this.navigationSidebarPanel.updateActionIndex(index, options);
    }

    _scrubberNavigationItemValueChanged(event)
    {
        for (let i = 0; i <= this._visualActionIndexes.length; ++i) {
            if (this._visualActionIndexes[i] < this._scrubberNavigationItem.value)
                continue;

            let min = i ? this._visualActionIndexes[i - 1] : this._scrubberNavigationItem.min;
            let max = i < this._visualActionIndexes.length ? this._visualActionIndexes[i] : this._scrubberNavigationItem.max;
            this._updateActionIndex(this._scrubberNavigationItem.value >= (min + max) / 2 ? max : min);
            return;
        }
    }

    _navigationSidebarImport(event)
    {
        let recording = WI.Recording.fromPayload(event.data.payload);
        if (!recording) {
            WI.Recording.synthesizeError(WI.UIString("unsupported version."));
            return;
        }

        this.showRepresentedObject(recording);
    }

    _navigationSidebarTreeOutlineSelectionChanged(event)
    {
        // Ignore deselect events.
        if (!event.data.selectedElement)
            return;

        let options = {suppressNavigationUpdate: true};
        let selectedTreeElement = this.navigationSidebarPanel.contentTreeOutline.selectedTreeElement;
        if (selectedTreeElement instanceof WI.FolderTreeElement)
            selectedTreeElement = selectedTreeElement.children.lastValue;
        this._updateActionIndex(selectedTreeElement.index, options);
    }
};

WI.RecordingTabContentView.Type = "recording";
