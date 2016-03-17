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
        this._treeOutline.disclosureButtons = false;
        this._treeOutline.large = true;

        this._treeOutline.element.addEventListener("focus", () => {this._inputElement.focus();});

        this.element.appendChild(this._treeOutline.element);

        this._resources = [];
        this._filteredResources = [];
    }

    // Protected

    _populateResourceTreeOutline()
    {
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

        for (let resource of this._filteredResources) {
            let treeElement = createTreeElement(resource);
            if (!treeElement)
                continue;

            if (this._treeOutline.findTreeElement(resource))
                continue;

            this._treeOutline.appendChild(treeElement);
        }

        if (this._treeOutline.children.length)
            this._treeOutline.children[0].select(true);
    }

    didPresentDialog()
    {
        this._inputElement.focus();
        this._clear();

        let frames = [WebInspector.frameResourceManager.mainFrame];
        while (frames.length) {
            let frame = frames.shift();
            for (let resource of frame.resources) {
                if (!this.representedObjectIsValid(resource))
                    continue;

                this._resources.push(resource);
            }

            frames = frames.concat(frame.childFrames);
        }
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
        this._filteredResources = [];
        this._treeOutline.hidden = true;
        this._treeOutline.removeChildren();

        let filterText = this._inputElement.value.trim();
        if (!filterText)
            return;

        // FIXME: <https://webkit.org/b/155324> Web Inspector: Improve filtering in OpenResourceDialog
        let filters = [simpleGlobStringToRegExp(filterText)];

        for (let resource of this._resources) {
            for (let i = 0; i < filters.length; ++i) {
                if (!filters[i].test(resource.displayName))
                    continue;

                resource.__weight = filters.length - i;
                this._filteredResources.push(resource);
                break;
            }
        }

        // Sort filtered resources by weight, then alphabetically.
        this._filteredResources.sort((a, b) => {
            if (a.__weight === b.__weight)
                return a.displayName.localeCompare(b.displayName);

            return b.__weight - a.__weight;
        });

        this._populateResourceTreeOutline();
        if (this._treeOutline.children.length)
            this._treeOutline.hidden = false;
    }
};

WebInspector.OpenResourceDialog.NonEmptyClassName = "non-empty";
