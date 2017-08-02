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

WI.VisualStyleTabbedPropertiesRow = class VisualStyleTabbedPropertiesRow extends WI.DetailsSectionRow
{
    constructor(tabMap)
    {
        super();

        this._element.classList.add("visual-style-tabbed-properties-row");

        let containerElement = document.createElement("div");
        containerElement.classList.add("visual-style-tabbed-properties-row-container");

        this._tabButtons = [];
        this._tabMap = tabMap;
        for (let key in this._tabMap) {
            let button = document.createElement("button");
            button.id = key;
            button.textContent = this._tabMap[key].title;
            button.addEventListener("click", this._handleButtonClicked.bind(this));

            containerElement.appendChild(button);
            this._tabButtons.push(button);
        }

        let firstButton = this._tabButtons[0];
        firstButton.classList.add("selected");
        this._tabMap[firstButton.id].element.classList.add("visible");

        this._element.appendChild(containerElement);
    }

    // Private

    _handleButtonClicked(event)
    {
        for (let item of this._tabButtons) {
            let tab = this._tabMap[item.id];
            let selected = item === event.target;
            item.classList.toggle("selected", selected);
            tab.element.classList.toggle("visible", selected);
            for (let propertyEditor of tab.properties)
                propertyEditor.update();
        }
    }
};
