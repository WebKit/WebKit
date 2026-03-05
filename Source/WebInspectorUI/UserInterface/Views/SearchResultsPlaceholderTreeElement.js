/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

WI.SearchResultsPlaceholderTreeElement = class SearchResultsPlaceholderTreeElement extends WI.TreeElement
{
    constructor(delegate, incrementCount, remainingCount)
    {
        const title = "";
        super(title);

        console.assert(delegate?.searchResultsPlaceholderLoadMoreResults, delegate);
        console.assert(incrementCount > 0, incrementCount);
        console.assert(remainingCount > 0, remainingCount);

        this._delegate = delegate;
        this._incrementCount = Math.min(incrementCount, remainingCount);
        this._remainingCount = remainingCount > incrementCount ? remainingCount : 0;
    }

    // Protected

    onattach()
    {
        if (this._incrementCount) {
            this._showMoreButtonElement = this._listItemNode.appendChild(document.createElement("button"));
            this._showMoreButtonElement.textContent = WI.UIString("Show %d More").format(this._incrementCount);
            this._showMoreButtonElement.addEventListener("click", this._handleShowMoreButtonClicked.bind(this));
        }

        if (this._remainingCount) {
            this._showAllButtonElement = this._listItemNode.appendChild(document.createElement("button"));
            this._showAllButtonElement.textContent = WI.UIString("Show All (%d More)").format(this._remainingCount);
            this._showAllButtonElement.addEventListener("click", this._handleShowAllButtonClicked.bind(this));
        }

        this._listItemNode.classList.add("item", "search-results-placeholder");
    }

    // Private

    _handleShowMoreButtonClicked(event)
    {
        this._delegate.searchResultsPlaceholderLoadMoreResults?.(this, this._incrementCount);
    }

    _handleShowAllButtonClicked(event)
    {
        this._delegate.searchResultsPlaceholderLoadMoreResults?.(this, this._remainingCount);
    }
}
