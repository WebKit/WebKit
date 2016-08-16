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

WebInspector.SourceCodeSearchMatchObject = class SourceCodeSearchMatchObject extends WebInspector.Object
{
    constructor(sourceCode, lineText, searchTerm, textRange)
    {
        super();

        console.assert(sourceCode instanceof WebInspector.SourceCode);

        this._sourceCode = sourceCode;
        this._lineText = lineText;
        this._searchTerm = searchTerm;
        this._sourceCodeTextRange = sourceCode.createSourceCodeTextRange(textRange);
    }

    // Public

    get sourceCode() { return this._sourceCode; }
    get title() { return this._lineText; }
    get searchTerm() { return this._searchTerm; }
    get sourceCodeTextRange() { return this._sourceCodeTextRange; }

    get className()
    {
        return WebInspector.SourceCodeSearchMatchObject.SourceCodeMatchIconStyleClassName;
    }

    saveIdentityToCookie(cookie)
    {
        if (this._sourceCode.url)
            cookie[WebInspector.SourceCodeSearchMatchObject.URLCookieKey] = this._sourceCode.url.hash;

        var textRange = this._sourceCodeTextRange.textRange;
        cookie[WebInspector.SourceCodeSearchMatchObject.TextRangeKey] = [textRange.startLine, textRange.startColumn, textRange.endLine, textRange.endColumn].join();
    }
};

WebInspector.SourceCodeSearchMatchObject.SourceCodeMatchIconStyleClassName = "source-code-match-icon";

WebInspector.SourceCodeSearchMatchObject.TypeIdentifier = "source-code-search-match-object";
WebInspector.SourceCodeSearchMatchObject.URLCookieKey = "source-code-url";
WebInspector.SourceCodeSearchMatchObject.TextRangeKey = "text-range";
