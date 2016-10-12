/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.OpenResourceDialog = class OpenResourceDialog extends WebInspector.Dialog
{
    constructor(delegate)
    {
        super(delegate);

        this.element.classList.add("open-resource-dialog");

        let fieldElement = this.element.appendChild(document.createElement("div"));
        fieldElement.classList.add("field");

        this._inputElement = fieldElement.appendChild(document.createElement("input"));
        this._inputElement.type = "text";
        this._inputElement.placeholder = WebInspector.UIString("File or Resource");
        this._inputElement.spellcheck = false;

        this._clearIconElement = fieldElement.appendChild(document.createElement("img"));

        this._inputElement.addEventListener("input", this._handleInputEvent.bind(this));
        this._inputElement.addEventListener("keydown", this._handleKeydownEvent.bind(this));
        this._inputElement.addEventListener("keyup", this._handleKeyupEvent.bind(this));
        this._inputElement.addEventListener("blur", this._handleBlurEvent.bind(this));
        this._clearIconElement.addEventListener("mousedown", this._handleMousedownEvent.bind(this));
        this._clearIconElement.addEventListener("click", this._handleClickEvent.bind(this));

        this._treeOutline = new WebInspector.TreeOutline(document.createElement("ol"));
        this._treeOutline.allowsRepeatSelection = true;
        this._treeOutline.disclosureButtons = false;
        this._treeOutline.large = true;

        this._treeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this._treeOutline.element.addEventListener("focus", () => {this._inputElement.focus();});

        this.element.appendChild(this._treeOutline.element);

        this._queryController = new WebInspector.ResourceQueryController;
        this._filteredResults = [];
    }

    // Protected

    _populateResourceTreeOutline()
    {
        function createHighlightedTitleFragment(title, highlightTextRanges)
        {
            let titleFragment = document.createDocumentFragment();
            let lastIndex = 0;
            for (let textRange of highlightTextRanges) {
                if (textRange.startColumn > lastIndex)
                    titleFragment.append(title.substring(lastIndex, textRange.startColumn));

                let highlightSpan = document.createElement("span");
                highlightSpan.classList.add("highlighted");
                highlightSpan.append(title.substring(textRange.startColumn, textRange.endColumn));
                titleFragment.append(highlightSpan);
                lastIndex = textRange.endColumn;
            }

            if (lastIndex < title.length)
                titleFragment.append(title.substring(lastIndex, title.length));

            return titleFragment;
        }

        function createTreeElement(representedObject)
        {
            let treeElement = null;

            if (representedObject instanceof WebInspector.SourceMapResource)
                treeElement = new WebInspector.SourceMapResourceTreeElement(representedObject);
            else if (representedObject instanceof WebInspector.Resource)
                treeElement = new WebInspector.ResourceTreeElement(representedObject);
            else if (representedObject instanceof WebInspector.Script)
                treeElement = new WebInspector.ScriptTreeElement(representedObject);

            return treeElement;
        }

        for (let result of this._filteredResults) {
            let resource = result.resource;
            if (this._treeOutline.findTreeElement(resource))
                continue;

            let treeElement = createTreeElement(resource);
            if (!treeElement)
                continue;

            treeElement.mainTitle = createHighlightedTitleFragment(resource.displayName, result.matchingTextRanges);
            this._treeOutline.appendChild(treeElement);
        }

        if (this._treeOutline.children.length)
            this._treeOutline.children[0].select(true, false, true, true);
    }

    didDismissDialog()
    {
        WebInspector.Frame.removeEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WebInspector.Frame.removeEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);

        this._queryController.reset();
    }

    didPresentDialog()
    {
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);

        if (WebInspector.frameResourceManager.mainFrame)
            this._addResourcesForFrame(WebInspector.frameResourceManager.mainFrame);

        this._inputElement.focus();
        this._clear();
    }

    // Private

    _handleInputEvent(event)
    {
        let force = this._inputElement.value !== "";
        this.element.classList.toggle(WebInspector.OpenResourceDialog.NonEmptyClassName, force);
    }

    _handleKeydownEvent(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Key.Escape.keyCode) {
            if (this._inputElement.value === "")
                this.dismiss();
            else
                this._clear();

            event.preventDefault();
        } else if (event.keyCode === WebInspector.KeyboardShortcut.Key.Enter.keyCode) {
            if (this._treeOutline.selectedTreeElement) {
                this.dismiss(this._treeOutline.selectedTreeElement.representedObject);
                event.preventDefault();
                return;
            }

            this._inputElement.select();
        } else if (event.keyCode === WebInspector.KeyboardShortcut.Key.Up.keyCode || event.keyCode === WebInspector.KeyboardShortcut.Key.Down.keyCode) {
            let treeElement = this._treeOutline.selectedTreeElement;
            if (!treeElement)
                return;

            let adjacentSiblingProperty = event.keyCode === WebInspector.KeyboardShortcut.Key.Up.keyCode ? "previousSibling" : "nextSibling";
            treeElement = treeElement[adjacentSiblingProperty];
            if (treeElement)
                treeElement.revealAndSelect(true, false, true, true);

            event.preventDefault();
        }
    }

    _handleKeyupEvent(event)
    {
        if (event.keyCode === WebInspector.KeyboardShortcut.Key.Up.keyCode || event.keyCode === WebInspector.KeyboardShortcut.Key.Down.keyCode)
            return;

        this._updateFilter();
    }

    _handleBlurEvent(event)
    {
        // Prevent the dialog from being dismissed while handling a tree item click event.
        if (event.relatedTarget === this._treeOutline.element)
            return;

        this.dismiss();
    }

    _handleMousedownEvent(event)
    {
        this._inputElement.select();

        // This ensures we don't get a "blur" event triggered for the text field
        // that would cause the dialog to be dismissed.
        event.preventDefault();
    }

    _handleClickEvent(event)
    {
        this._clear();
    }

    _clear()
    {
        this._inputElement.value = "";
        this.element.classList.remove(WebInspector.OpenResourceDialog.NonEmptyClassName);
        this._updateFilter();
    }

    _updateFilter()
    {
        this._filteredResults = [];
        this._treeOutline.hidden = true;
        this._treeOutline.removeChildren();

        let filterText = this._inputElement.value.trim();
        if (!filterText)
            return;

        this._filteredResults = this._queryController.executeQuery(filterText);

        this._populateResourceTreeOutline();
        if (this._treeOutline.children.length)
            this._treeOutline.hidden = false;
    }

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        if (!event.data.selectedByUser)
            return;

        this.dismiss(treeElement.representedObject);
    }

    _addResource(resource, suppressFilterUpdate)
    {
        if (!this.representedObjectIsValid(resource))
            return;

        this._queryController.addResource(resource);
        if (suppressFilterUpdate)
            return;

        this._updateFilter();
    }

    _addResourcesForFrame(frame)
    {
        const suppressFilterUpdate = true;

        let frames = [frame];
        while (frames.length) {
            let currentFrame = frames.shift();
            let resources = [currentFrame.mainResource].concat(currentFrame.resources);
            for (let resource of resources)
                this._addResource(resource, suppressFilterUpdate);

            frames = frames.concat(frame.childFrames);
        }

        this._updateFilter();
    }

    _mainResourceDidChange(event)
    {
        if (event.target.isMainFrame())
            this._queryController.reset();

        this._addResource(event.target.mainResource);
    }

    _resourceWasAdded(event)
    {
        this._addResource(event.data.resource);
    }
};

WebInspector.OpenResourceDialog.NonEmptyClassName = "non-empty";
