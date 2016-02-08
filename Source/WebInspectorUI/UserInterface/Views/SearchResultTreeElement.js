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

WebInspector.SearchResultTreeElement = class SearchResultTreeElement extends WebInspector.GeneralTreeElement
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WebInspector.DOMSearchMatchObject || representedObject instanceof WebInspector.SourceCodeSearchMatchObject);

        var title = WebInspector.SearchResultTreeElement.truncateAndHighlightTitle(representedObject.title, representedObject.searchTerm, representedObject.sourceCodeTextRange);

        super(representedObject.className, title, null, representedObject, false);
    }

    // Static

    static truncateAndHighlightTitle(title, searchTerm, sourceCodeTextRange)
    {
        // Use the original location, since those line/column offsets match the line text in title.
        var textRange = sourceCodeTextRange.textRange;

        var searchTermIndex = textRange.startColumn;

        // We should only have one line text ranges, so make sure that is the case.
        console.assert(textRange.startLine === textRange.endLine);

        // Show some characters before the matching text (if there are enough) for context. TreeOutline takes care of the truncating
        // at the end of the string.
        var modifiedTitle = null;
        if (searchTermIndex > WebInspector.SearchResultTreeElement.CharactersToShowBeforeSearchMatch) {
            modifiedTitle = ellipsis + title.substring(searchTermIndex - WebInspector.SearchResultTreeElement.CharactersToShowBeforeSearchMatch);
            searchTermIndex = WebInspector.SearchResultTreeElement.CharactersToShowBeforeSearchMatch + 1;
        } else
            modifiedTitle = title;

        // Truncate the tail of the title so the tooltip isn't so large.
        modifiedTitle = modifiedTitle.trimEnd(searchTermIndex + searchTerm.length + WebInspector.SearchResultTreeElement.CharactersToShowAfterSearchMatch);

        console.assert(modifiedTitle.substring(searchTermIndex, searchTermIndex + searchTerm.length).toLowerCase() === searchTerm.toLowerCase());

        var highlightedTitle = document.createDocumentFragment();

        highlightedTitle.append(modifiedTitle.substring(0, searchTermIndex));

        var highlightSpan = document.createElement("span");
        highlightSpan.className = "highlighted";
        highlightSpan.append(modifiedTitle.substring(searchTermIndex, searchTermIndex + searchTerm.length));
        highlightedTitle.appendChild(highlightSpan);

        highlightedTitle.append(modifiedTitle.substring(searchTermIndex + searchTerm.length));

        return highlightedTitle;
    }

    // Public

    get filterableData()
    {
        return {text: [this.representedObject.title]};
    }
};

WebInspector.SearchResultTreeElement.CharactersToShowBeforeSearchMatch = 15;
WebInspector.SearchResultTreeElement.CharactersToShowAfterSearchMatch = 50;
