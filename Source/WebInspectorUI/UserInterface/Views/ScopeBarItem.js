/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.ScopeBarItem = class ScopeBarItem extends WebInspector.Object
{
    constructor(id, label, isExclusive)
    {
        super();

        this.id = id;
        this.label = label;
        this.isExclusive = isExclusive;

        this._selectedSetting = new WebInspector.Setting("scopebaritem-" + id, false);

        this._markElementSelected(this._selectedSetting.value);
    }

    // Public

    get element()
    {
        if (!this._element) {
            this._element = document.createElement("li");
            this._element.textContent = this.label;
            this._element.addEventListener("click", this._clicked.bind(this), false);
        }
        return this._element;
    }

    get selected()
    {
        return this._selectedSetting.value;
    }

    set selected(selected)
    {
        this.setSelected(selected, false);
    }

    setSelected(selected, withModifier)
    {
        if (this._selectedSetting.value === selected)
            return;

        this._markElementSelected(selected);

        this._selectedSetting.value = selected;

        this.dispatchEventToListeners(WebInspector.ScopeBarItem.Event.SelectionChanged, {withModifier});
    }

    // Private

    _markElementSelected(selected)
    {
        if (selected)
            this.element.classList.add(WebInspector.ScopeBarItem.SelectedStyleClassName);
        else
            this.element.classList.remove(WebInspector.ScopeBarItem.SelectedStyleClassName);
    }

    _clicked(event)
    {
        var withModifier = (event.metaKey && !event.ctrlKey && !event.altKey && !event.shiftKey);
        this.setSelected(!this.selected, withModifier);
    }
};

WebInspector.ScopeBarItem.SelectedStyleClassName = "selected";

WebInspector.ScopeBarItem.Event = {
    SelectionChanged: "scope-bar-item-selection-did-change"
};
