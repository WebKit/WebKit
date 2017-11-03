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

WI.ScopeRadioButtonNavigationItem = class ScopeRadioButtonNavigationItem extends WI.RadioButtonNavigationItem
{
    constructor(identifier, toolTip, scopeItems, defaultScopeItem)
    {
        super(identifier, toolTip);

        this._scopeItems = scopeItems || [];

        this._element.classList.add("scope-radio-button-navigation-item");
        this._element.title = defaultScopeItem ? defaultScopeItem.label : this._scopeItems[0].label;

        this._scopeItemSelect = document.createElement("select");
        this._scopeItemSelect.classList.add("scope-radio-button-item-select");

        for (var item of this._scopeItems) {
            var option = document.createElement("option");
            option.value = item.identifier;
            option.text = item.label;
            this._scopeItemSelect.appendChild(option);
        }

        this.selectedItemIdentifier = defaultScopeItem ? defaultScopeItem.identifier : this._scopeItems[0].identifier;
        this._scopeItemSelect.addEventListener("change", this._handleItemChanged.bind(this));
        this._element.appendChild(this._scopeItemSelect);

        this._element.appendChild(WI.ImageUtilities.useSVGSymbol("Images/UpDownArrows.svg", "arrows"));
    }

    // Public

    set selectedItemIdentifier(identifier)
    {
        if (!identifier)
            return;

        this._scopeItemSelect.value = identifier;
    }

    get selectedItemIdentifier()
    {
        return this._scopeItemSelect.value;
    }

    dontPreventDefaultOnNavigationBarMouseDown()
    {
        return true;
    }

    // Private

    _handleItemChanged()
    {
        var selectedItemIdentifier;
        for (var item of this._scopeItems) {
            if (item.identifier !== this.selectedItemIdentifier)
                continue;

            selectedItemIdentifier = item;
            break;
        }

        this._element.title = selectedItemIdentifier.label;
        this.dispatchEventToListeners(WI.ScopeRadioButtonNavigationItem.Event.SelectedItemChanged);
    }
};

WI.ScopeRadioButtonNavigationItem.Event = {
    SelectedItemChanged: "scope-radio-button-navigation-item-selected-item-changed"
};
