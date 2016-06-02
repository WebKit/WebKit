/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.GeneralTreeElement = class GeneralTreeElement extends WebInspector.TreeElement
{
    constructor(classNames, title, subtitle, representedObject, hasChildren)
    {
        super("", representedObject, hasChildren);

        this.classNames = classNames;

        this._tooltipHandledSeparately = false;
        this._mainTitle = title || "";
        this._subtitle = subtitle || "";
        this._status = "";
    }

    // Public

    get element()
    {
        return this._listItemNode;
    }

    get iconElement()
    {
        this._createElementsIfNeeded();
        return this._iconElement;
    }

    get titlesElement()
    {
        this._createElementsIfNeeded();
        return this._titlesElement;
    }

    get mainTitleElement()
    {
        this._createElementsIfNeeded();
        return this._mainTitleElement;
    }

    get subtitleElement()
    {
        this._createElementsIfNeeded();
        this._createSubtitleElementIfNeeded();
        return this._subtitleElement;
    }

    get classNames()
    {
        return this._classNames;
    }

    set classNames(x)
    {
        x = x || [];

        if (typeof x === "string")
            x = [x];

        if (Object.shallowEqual(this._classNames, x))
            return;

        if (this._listItemNode && this._classNames)
            this._listItemNode.classList.remove(...this._classNames);

        this._classNames = x;

        if (this._listItemNode)
            this._listItemNode.classList.add(...this._classNames);
    }

    addClassName(className)
    {
        if (this._classNames.includes(className))
            return;

        this._classNames.push(className);

        if (this._listItemNode)
            this._listItemNode.classList.add(className);
    }

    removeClassName(className)
    {
        if (!this._classNames.includes(className))
            return;

        this._classNames.remove(className);

        if (this._listItemNode)
            this._listItemNode.classList.remove(className);
    }

    get mainTitle()
    {
        return this._mainTitle;
    }

    set mainTitle(x)
    {
        x = x || "";

        if (this._mainTitle === x)
            return;

        this._mainTitle = x;
        this._updateTitleElements();
        this.didChange();
        this.dispatchEventToListeners(WebInspector.GeneralTreeElement.Event.MainTitleDidChange);
    }

    get subtitle()
    {
        return this._subtitle;
    }

    set subtitle(x)
    {
        x = x || "";

        if (this._subtitle === x)
            return;

        this._subtitle = x;
        this._updateTitleElements();
        this.didChange();
    }

    get status()
    {
        return this._status;
    }

    set status(x)
    {
        x = x || "";

        if (this._status === x)
            return;

        if (!this._statusElement) {
            this._statusElement = document.createElement("div");
            this._statusElement.className = WebInspector.GeneralTreeElement.StatusElementStyleClassName;
        }

        this._status = x;
        this._updateStatusElement();
    }

    get filterableData()
    {
        return {text: [this.mainTitle, this.subtitle]};
    }

    get tooltipHandledSeparately()
    {
        return this._tooltipHandledSeparately;
    }

    set tooltipHandledSeparately(x)
    {
        this._tooltipHandledSeparately = !!x;
    }

    // Overrides from TreeElement (Private)

    isEventWithinDisclosureTriangle(event)
    {
        return event.target === this._disclosureButton;
    }

    onattach()
    {
        this._createElementsIfNeeded();
        this._updateTitleElements();

        this._listItemNode.classList.add("item");

        if (this._classNames)
            this._listItemNode.classList.add(...this._classNames);

        this._listItemNode.appendChild(this._disclosureButton);
        this._listItemNode.appendChild(this._iconElement);
        if (this._statusElement)
            this._listItemNode.appendChild(this._statusElement);
        this._listItemNode.appendChild(this._titlesElement);

        if (this.oncontextmenu && typeof this.oncontextmenu === "function") {
            this._boundContextMenuEventHandler = this.oncontextmenu.bind(this);
            this._listItemNode.addEventListener("contextmenu", this._boundContextMenuEventHandler);
        }

        if (!this._boundContextMenuEventHandler && this.treeOutline.oncontextmenu && typeof this.treeOutline.oncontextmenu === "function") {
            this._boundContextMenuEventHandler = (event) => { this.treeOutline.oncontextmenu(event, this); };
            this._listItemNode.addEventListener("contextmenu", this._boundContextMenuEventHandler);
        }
    }

    ondetach()
    {
        if (this._boundContextMenuEventHandler) {
            this._listItemNode.removeEventListener("contextmenu", this._boundContextMenuEventHandler);
            this._boundContextMenuEventHandler = null;
        }
    }

    onreveal()
    {
        if (this._listItemNode)
            this._listItemNode.scrollIntoViewIfNeeded(false);
    }

    // Protected

    callFirstAncestorFunction(functionName, args)
    {
        // Call the first ancestor that implements a function named functionName (if any).
        var currentNode = this.parent;
        while (currentNode) {
            if (typeof currentNode[functionName] === "function") {
                currentNode[functionName].apply(currentNode, args);
                break;
            }

            currentNode = currentNode.parent;
        }
    }

    // Private

    _createElementsIfNeeded()
    {
        if (this._createdElements)
            return;

        this._disclosureButton = document.createElement("button");
        this._disclosureButton.className = WebInspector.GeneralTreeElement.DisclosureButtonStyleClassName;

        // Don't allow the disclosure button to be keyboard focusable. The TreeOutline is focusable and has
        // its own keybindings for toggling expand and collapse.
        this._disclosureButton.tabIndex = -1;

        this._iconElement = document.createElement("img");
        this._iconElement.className = WebInspector.GeneralTreeElement.IconElementStyleClassName;

        this._titlesElement = document.createElement("div");
        this._titlesElement.className = WebInspector.GeneralTreeElement.TitlesElementStyleClassName;

        this._mainTitleElement = document.createElement("span");
        this._mainTitleElement.className = WebInspector.GeneralTreeElement.MainTitleElementStyleClassName;
        this._titlesElement.appendChild(this._mainTitleElement);

        this._createdElements = true;
    }

    _createSubtitleElementIfNeeded()
    {
        if (this._subtitleElement)
            return;

        this._subtitleElement = document.createElement("span");
        this._subtitleElement.className = WebInspector.GeneralTreeElement.SubtitleElementStyleClassName;
        this._titlesElement.appendChild(this._subtitleElement);
    }

    _updateTitleElements()
    {
        if (!this._createdElements)
            return;

        if (typeof this._mainTitle === "string") {
            if (this._mainTitleElement.textContent !== this._mainTitle)
                this._mainTitleElement.textContent = this._mainTitle;
        } else if (this._mainTitle instanceof Node) {
            this._mainTitleElement.removeChildren();
            this._mainTitleElement.appendChild(this._mainTitle);
        }

        if (typeof this._subtitle === "string" && this._subtitle) {
            this._createSubtitleElementIfNeeded();
            if (this._subtitleElement.textContent !== this._subtitle)
                this._subtitleElement.textContent = this._subtitle;
            this._titlesElement.classList.remove(WebInspector.GeneralTreeElement.NoSubtitleStyleClassName);
        } else if (this._subtitle instanceof Node) {
            this._createSubtitleElementIfNeeded();
            this._subtitleElement.removeChildren();
            this._subtitleElement.appendChild(this._subtitle);
        } else {
            if (this._subtitleElement)
                this._subtitleElement.textContent = "";
            this._titlesElement.classList.add(WebInspector.GeneralTreeElement.NoSubtitleStyleClassName);
        }

        // Set a default tooltip if there isn't a custom one already assigned.
        if (!this.tooltip && !this._tooltipHandledSeparately)
            this._updateTitleTooltip();
    }

    _updateTitleTooltip()
    {
        console.assert(this._listItemNode);
        if (!this._listItemNode)
            return;

        // Get the textContent for the elements since they can contain other nodes,
        // and the tool tip only cares about the text.
        let mainTitleText = this._mainTitleElement.textContent;
        let subtitleText = this._subtitleElement ? this._subtitleElement.textContent : "";
        let large = this.treeOutline  && this.treeOutline.large;
        if (mainTitleText && subtitleText)
            this._listItemNode.title = mainTitleText + (large ? "\n" : " \u2014 ") + subtitleText;
        else if (mainTitleText)
            this._listItemNode.title = mainTitleText;
        else
            this._listItemNode.title = subtitleText;
    }

    _updateStatusElement()
    {
        if (!this._statusElement)
            return;

        if (!this._statusElement.parentNode && this._listItemNode)
            this._listItemNode.insertBefore(this._statusElement, this._titlesElement);

        if (this._status instanceof Node) {
            this._statusElement.removeChildren();
            this._statusElement.appendChild(this._status);
        } else
            this._statusElement.textContent = this._status;
    }
};

WebInspector.GeneralTreeElement.DisclosureButtonStyleClassName = "disclosure-button";
WebInspector.GeneralTreeElement.IconElementStyleClassName = "icon";
WebInspector.GeneralTreeElement.StatusElementStyleClassName = "status";
WebInspector.GeneralTreeElement.TitlesElementStyleClassName = "titles";
WebInspector.GeneralTreeElement.MainTitleElementStyleClassName = "title";
WebInspector.GeneralTreeElement.SubtitleElementStyleClassName = "subtitle";
WebInspector.GeneralTreeElement.NoSubtitleStyleClassName = "no-subtitle";

WebInspector.GeneralTreeElement.Event = {
    MainTitleDidChange: "general-tree-element-main-title-did-change"
};
