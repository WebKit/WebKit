/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

// FIXME: Investigate folding this into SourceCodeLocation proper so it can always be as lazy as possible.

// Lazily compute the full SourceCodeLocation information only when such information is needed.
//  - raw information doesn't require initialization, we have that information
//  - formatted information does require initialization, done by overriding public APIs.
//  - display information does require initialization, done by overriding private funnel API resolveMappedLocation.

WebInspector.LazySourceCodeLocation = function(sourceCode, lineNumber, columnNumber)
{
    WebInspector.SourceCodeLocation.call(this, null, lineNumber, columnNumber);

    console.assert(sourceCode);

    this._initialized = false;
    this._lazySourceCode = sourceCode;
};

WebInspector.LazySourceCodeLocation.prototype = {
    constructor: WebInspector.LazySourceCodeLocation,

    // Public

    isEqual: function(other)
    {
        if (!other)
            return false;
        return this._lazySourceCode === other._sourceCode && this._lineNumber === other._lineNumber && this._columnNumber === other._columnNumber;
    },

    get sourceCode()
    {
        return this._lazySourceCode;
    },

    set sourceCode(sourceCode)
    {
        // Getter and setter must be provided together.
        this.setSourceCode(sourceCode);
    },

    get formattedLineNumber()
    {
        this._lazyInitialization();
        return this._formattedLineNumber;
    },

    get formattedColumnNumber()
    {
        this._lazyInitialization();
        return this._formattedColumnNumber;
    },

    formattedPosition: function()
    {
        this._lazyInitialization();
        return new WebInspector.SourceCodePosition(this._formattedLineNumber, this._formattedColumnNumber);
    },

    hasFormattedLocation: function()
    {
        this._lazyInitialization();
        return WebInspector.SourceCodeLocation.prototype.hasFormattedLocation.call(this);
    },

    hasDifferentDisplayLocation: function()
    {
        this._lazyInitialization();
        return WebInspector.SourceCodeLocation.prototype.hasDifferentDisplayLocation.call(this);
    },

    // Protected

    resolveMappedLocation: function()
    {
        this._lazyInitialization();
        WebInspector.SourceCodeLocation.prototype.resolveMappedLocation.call(this);
    },

    // Private

    _lazyInitialization: function()
    {
        if (!this._initialized) {
            this._initialized = true;
            this.sourceCode = this._lazySourceCode;
        }
    }
};

WebInspector.LazySourceCodeLocation.prototype.__proto__ = WebInspector.SourceCodeLocation.prototype;
