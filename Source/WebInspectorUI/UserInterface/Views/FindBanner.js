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

WebInspector.FindBanner = class FindBanner extends WebInspector.NavigationItem
{
    constructor(delegate, className, fixed = false)
    {
        super();

        this._delegate = delegate || null;

        this.element.classList.add("find-banner");

        if (className)
            this.element.classList.add(className);

        this._resultCountLabel = document.createElement("label");
        this.element.appendChild(this._resultCountLabel);

        this._inputField = document.createElement("input");
        this._inputField.type = "search";
        this._inputField.spellcheck = false;
        this._inputField.incremental = true;
        this._inputField.setAttribute("results", 5);
        this._inputField.setAttribute("autosave", "inspector-search");
        this._inputField.addEventListener("keydown", this._inputFieldKeyDown.bind(this), false);
        this._inputField.addEventListener("keyup", this._inputFieldKeyUp.bind(this), false);
        this._inputField.addEventListener("search", this._inputFieldSearch.bind(this), false);
        this.element.appendChild(this._inputField);

        this._previousResultButton = document.createElement("button");
        this._previousResultButton.classList.add(WebInspector.FindBanner.SegmentedButtonStyleClassName);
        this._previousResultButton.classList.add(WebInspector.FindBanner.LeftSegmentButtonStyleClassName);
        this._previousResultButton.disabled = true;
        this._previousResultButton.addEventListener("click", this._previousResultButtonClicked.bind(this));
        this.element.appendChild(this._previousResultButton);

        let previousResultButtonGlyphElement = document.createElement("div");
        previousResultButtonGlyphElement.classList.add(WebInspector.FindBanner.SegmentGlyphStyleClassName);
        this._previousResultButton.appendChild(previousResultButtonGlyphElement);

        this._nextResultButton = document.createElement("button");
        this._nextResultButton.classList.add(WebInspector.FindBanner.SegmentedButtonStyleClassName);
        this._nextResultButton.classList.add(WebInspector.FindBanner.RightSegmentButtonStyleClassName);
        this._nextResultButton.disabled = true;
        this._nextResultButton.addEventListener("click", this._nextResultButtonClicked.bind(this));
        this.element.appendChild(this._nextResultButton);

        let nextResultButtonGlyphElement = document.createElement("div");
        nextResultButtonGlyphElement.classList.add(WebInspector.FindBanner.SegmentGlyphStyleClassName);
        this._nextResultButton.appendChild(nextResultButtonGlyphElement);

        if (fixed)
            this._clearAndBlurKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape, this._clearAndBlur.bind(this), this.element);
        else {
            let doneButtonElement = document.createElement("button");
            doneButtonElement.textContent = WebInspector.UIString("Done");
            doneButtonElement.addEventListener("click", this._doneButtonClicked.bind(this));
            this.element.appendChild(doneButtonElement);
            this._hideKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape, this.hide.bind(this), this.element);
        }

        this._numberOfResults = null;
        this._searchBackwards = false;
        this._searchKeyPressed = false;
        this._previousSearchValue = "";

        this._populateFindKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "E", this._populateSearchQueryFromSelection.bind(this));
        this._populateFindKeyboardShortcut.implicitlyPreventsDefault = false;
        this._findNextKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "G", this._nextResultButtonClicked.bind(this));
        this._findPreviousKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "G", this._previousResultButtonClicked.bind(this));

        this.disableKeyboardShortcuts();
    }

    // Public

    get delegate() { return this._delegate; }
    set delegate(newDelegate) { this._delegate = newDelegate || null; }
    get inputField() { return this._inputField; }
    get searchQuery() { return this._inputField.value || ""; }
    set searchQuery(query) { this._inputField.value = query || ""; }
    get numberOfResults() { return this._numberOfResults; }

    set numberOfResults(numberOfResults)
    {
        if (numberOfResults === undefined || isNaN(numberOfResults))
            numberOfResults = null;

        this._numberOfResults = numberOfResults;

        this._previousResultButton.disabled = this._nextResultButton.disabled = (numberOfResults <= 0);

        if (numberOfResults === null)
            this._resultCountLabel.textContent = "";
        else if (numberOfResults <= 0)
            this._resultCountLabel.textContent = WebInspector.UIString("Not found");
        else if (numberOfResults === 1)
            this._resultCountLabel.textContent = WebInspector.UIString("1 match");
        else if (numberOfResults > 1)
            this._resultCountLabel.textContent = WebInspector.UIString("%d matches").format(numberOfResults);
    }

    get targetElement()
    {
        return this._targetElement;
    }

    set targetElement(element)
    {
        function delayedWork()
        {
            oldTargetElement.classList.remove(WebInspector.FindBanner.NoTransitionStyleClassName);
            this.element.classList.remove(WebInspector.FindBanner.NoTransitionStyleClassName);
        }

        if (this._targetElement) {
            var oldTargetElement = this._targetElement;

            this._targetElement.classList.add(WebInspector.FindBanner.NoTransitionStyleClassName);
            this._targetElement.classList.remove(WebInspector.FindBanner.SupportsFindBannerStyleClassName);
            this._targetElement.classList.remove(WebInspector.FindBanner.ShowingFindBannerStyleClassName);

            this.element.classList.add(WebInspector.FindBanner.NoTransitionStyleClassName);
            this.element.classList.remove(WebInspector.FindBanner.ShowingStyleClassName);

            // Delay so we can remove the no transition style class after the other style changes are committed.
            setTimeout(delayedWork.bind(this), 0);
        }

        this._targetElement = element || null;

        if (this._targetElement)
            this._targetElement.classList.add(WebInspector.FindBanner.SupportsFindBannerStyleClassName);
    }

    get showing()
    {
        return this.element.classList.contains(WebInspector.FindBanner.ShowingStyleClassName);
    }

    focus()
    {
        // FIXME: Workaround for: <https://webkit.org/b/149504> Caret missing from <input> after clearing text and calling select()
        if (!this._inputField.value.length)
            this._inputField.focus();
        else
            this._inputField.select();
    }

    _clearAndBlur()
    {
        this.numberOfResults = null;

        this._inputField.value = "";
        this._previousSearchValue = "";

        if (this._delegate.findBannerSearchCleared)
            this._delegate.findBannerSearchCleared();
        if (this._delegate.findBannerWantsToClearAndBlur)
            this._delegate.findBannerWantsToClearAndBlur();
    }

    show()
    {
        console.assert(this._targetElement);
        if (!this._targetElement)
            return;

        console.assert(this._targetElement.parentNode);
        if (!this._targetElement.parentNode)
            return;

        if (this.element.parentNode !== this._targetElement.parentNode)
            this._targetElement.parentNode.insertBefore(this.element, this._targetElement);

        function delayedWork()
        {
            this._targetElement.classList.add(WebInspector.FindBanner.ShowingFindBannerStyleClassName);
            this.element.classList.add(WebInspector.FindBanner.ShowingStyleClassName);

            this._inputField.select();
        }

        // Delay adding the classes in case the target element and/or the find banner were just added to
        // the document. Adding the class right away will prevent the animation from working the first time.
        setTimeout(delayedWork.bind(this), 0);

        this.dispatchEventToListeners(WebInspector.FindBanner.Event.DidShow);
    }

    hide()
    {
        console.assert(this._targetElement);
        if (!this._targetElement)
            return;

        this._inputField.blur();

        this._targetElement.classList.remove(WebInspector.FindBanner.ShowingFindBannerStyleClassName);
        this.element.classList.remove(WebInspector.FindBanner.ShowingStyleClassName);

        this.dispatchEventToListeners(WebInspector.FindBanner.Event.DidHide);
    }

    enableKeyboardShortcuts()
    {
        this._populateFindKeyboardShortcut.disabled = false;
        this._findNextKeyboardShortcut.disabled = false;
        this._findPreviousKeyboardShortcut.disabled = false;
    }

    disableKeyboardShortcuts()
    {
        this._populateFindKeyboardShortcut.disabled = true;
        this._findNextKeyboardShortcut.disabled = true;
        this._findPreviousKeyboardShortcut.disabled = true;
    }

    // Private

    _inputFieldKeyDown(event)
    {
        if (event.keyIdentifier === "Shift")
            this._searchBackwards = true;
        else if (event.keyIdentifier === "Enter")
            this._searchKeyPressed = true;
    }

    _inputFieldKeyUp(event)
    {
        if (event.keyIdentifier === "Shift")
            this._searchBackwards = false;
        else if (event.keyIdentifier === "Enter")
            this._searchKeyPressed = false;
    }

    _inputFieldSearch(event)
    {
        if (this._inputField.value) {
            if (this._previousSearchValue !== this.searchQuery) {
                if (this._delegate && typeof this._delegate.findBannerPerformSearch === "function")
                    this._delegate.findBannerPerformSearch(this, this.searchQuery);
            } else if (this._searchKeyPressed && this._numberOfResults > 0) {
                if (this._searchBackwards) {
                    if (this._delegate && typeof this._delegate.findBannerRevealPreviousResult === "function")
                        this._delegate.findBannerRevealPreviousResult(this);
                } else {
                    if (this._delegate && typeof this._delegate.findBannerRevealNextResult === "function")
                        this._delegate.findBannerRevealNextResult(this);
                }
            }
        } else {
            this.numberOfResults = null;
            if (this._delegate && typeof this._delegate.findBannerSearchCleared === "function")
                this._delegate.findBannerSearchCleared(this);
        }

        this._previousSearchValue = this.searchQuery;
    }

    _populateSearchQueryFromSelection(event)
    {
        if (this._delegate && typeof this._delegate.findBannerSearchQueryForSelection === "function") {
            var query = this._delegate.findBannerSearchQueryForSelection(this);
            if (query) {
                this.searchQuery = query;

                if (this._delegate && typeof this._delegate.findBannerPerformSearch === "function")
                    this._delegate.findBannerPerformSearch(this, this.searchQuery);
            }
        }
    }

    _previousResultButtonClicked(event)
    {
        if (this._delegate && typeof this._delegate.findBannerRevealPreviousResult === "function")
            this._delegate.findBannerRevealPreviousResult(this);
    }

    _nextResultButtonClicked(event)
    {
        if (this._delegate && typeof this._delegate.findBannerRevealNextResult === "function")
            this._delegate.findBannerRevealNextResult(this);
    }

    _doneButtonClicked(event)
    {
        this.hide();
    }
};

WebInspector.FindBanner.SupportsFindBannerStyleClassName = "supports-find-banner";
WebInspector.FindBanner.ShowingFindBannerStyleClassName = "showing-find-banner";
WebInspector.FindBanner.NoTransitionStyleClassName = "no-find-banner-transition";
WebInspector.FindBanner.ShowingStyleClassName = "showing";
WebInspector.FindBanner.SegmentedButtonStyleClassName = "segmented";
WebInspector.FindBanner.LeftSegmentButtonStyleClassName = "left";
WebInspector.FindBanner.RightSegmentButtonStyleClassName = "right";
WebInspector.FindBanner.SegmentGlyphStyleClassName = "glyph";

WebInspector.FindBanner.Event = {
    DidShow: "find-banner-did-show",
    DidHide: "find-banner-did-hide"
};
