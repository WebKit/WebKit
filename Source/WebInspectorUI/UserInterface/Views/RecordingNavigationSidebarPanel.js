/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.RecordingNavigationSidebarPanel = class RecordingNavigationSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor()
    {
        super("recording", WebInspector.UIString("Recording"));

        this.contentTreeOutline.customIndent = true;

        this.filterBar.placeholder = WebInspector.UIString("Filter Actions");

        this.recording = null;

        this._importButton = null;
        this._exportButton = null;
    }

    // Static

    static disallowInstanceForClass()
    {
        return true;
    }

    // Public

    set recording(recording)
    {
        if (recording === this._recording)
            return;

        this.contentTreeOutline.removeChildren();

        this._recording = recording;

        if (this._recording) {
            this.contentTreeOutline.element.dataset.indent = Number.countDigits(this._recording.actions.length);

            if (this._recording.actions[0] instanceof WebInspector.RecordingInitialStateAction)
                this.contentTreeOutline.appendChild(new WebInspector.RecordingActionTreeElement(this._recording.actions[0], 0, this._recording.type));

            let cumulativeActionIndex = 1;
            this._recording.frames.forEach((frame, frameIndex) => {
                let folder = new WebInspector.FolderTreeElement(WebInspector.UIString("Frame %d").format((frameIndex + 1).toLocaleString()));
                this.contentTreeOutline.appendChild(folder);

                for (let i = 0; i < frame.actions.length; ++i)
                    folder.appendChild(new WebInspector.RecordingActionTreeElement(frame.actions[i], cumulativeActionIndex + i, this._recording.type));

                if (frame.incomplete)
                    folder.subtitle = WebInspector.UIString("Incomplete");

                if (this._recording.frames.length === 1)
                    folder.expand();

                cumulativeActionIndex += frame.actions.length;
            });
        }

        this.updateEmptyContentPlaceholder(WebInspector.UIString("No Recording Data"));

        if (this._exportButton)
            this._exportButton.disabled = !this.contentTreeOutline.children.length;
    }

    updateActionIndex(index, options = {})
    {
        console.assert(!this._recording || (index >= 0 && index < this._recording.actions.length));
        if (!this._recording || index < 0 || index >= this._recording.actions.length)
            return;

        let treeOutline = this.contentTreeOutline;
        if (!(this._recording.actions[0] instanceof WebInspector.RecordingInitialStateAction) || index) {
            treeOutline = treeOutline.children[0];
            while (index > treeOutline.children.length) {
                index -= treeOutline.children.length;
                treeOutline = treeOutline.nextSibling;
            }

            // Must subtract one from the final result since the initial state is considered index 0.
            --index;
        }

        let treeElementToSelect = treeOutline.children[index];
        if (treeElementToSelect.parent && !treeElementToSelect.parent.expanded)
            treeElementToSelect = treeElementToSelect.parent;

        const omitFocus = false;
        const selectedByUser = false;
        let suppressOnSelect = !(treeElementToSelect instanceof WebInspector.FolderTreeElement);
        const suppressOnDeselect = true;
        treeElementToSelect.revealAndSelect(omitFocus, selectedByUser, suppressOnSelect, suppressOnDeselect);
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        const role = "button";

        const importLabel = WebInspector.UIString("Import");
        let importNavigationItem = new WebInspector.NavigationItem("recording-import", role, importLabel);

        this._importButton = importNavigationItem.element.appendChild(document.createElement("button"));
        this._importButton.textContent = importLabel;
        this._importButton.addEventListener("click", this._importNavigationItemClicked.bind(this));

        const exportLabel = WebInspector.UIString("Export");
        let exportNavigationItem = new WebInspector.NavigationItem("recording-export", role, exportLabel);

        this._exportButton = exportNavigationItem.element.appendChild(document.createElement("button"));
        this._exportButton.textContent = exportLabel;
        this._exportButton.disabled = !this.contentTreeOutline.children.length;
        this._exportButton.addEventListener("click", this._exportNavigationItemClicked.bind(this));

        const element = null;
        this.addSubview(new WebInspector.NavigationBar(element, [importNavigationItem, exportNavigationItem]));
    }

    // Private

    _importNavigationItemClicked(event)
    {
        WebInspector.loadDataFromFile((data) => {
            if (!data)
                return;

            let payload = null;
            try {
                payload = JSON.parse(data);
            } catch (e) {
                WebInspector.Recording.synthesizeError(e);
                return;
            }

            this.dispatchEventToListeners(WebInspector.RecordingNavigationSidebarPanel.Event.Import, {payload});
        });
    }

    _exportNavigationItemClicked(event)
    {
        if (!this._recording)
            return;

        const forceSaveAs = true;
        WebInspector.saveDataToFile({
            url: "web-inspector:///Recording.json",
            content: JSON.stringify(this._recording.toJSON()),
        }, forceSaveAs);
    }
};

WebInspector.RecordingNavigationSidebarPanel.Event = {
    Import: "recording-navigation-sidebar-panel-import",
};
