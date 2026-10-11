/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

WI.SymbolicBreakpoint = class SymbolicBreakpoint extends WI.Breakpoint
{
    constructor(symbol, {caseSensitive, isRegex, sourceMapped, disabled, actions, condition, ignoreCount, autoContinue} = {})
    {
        console.assert(typeof symbol === "string" && symbol.trim().length, symbol);
        console.assert(!sourceMapped || !ignoreCount, ignoreCount, sourceMapped);

        super({disabled, condition, actions, ignoreCount, autoContinue});

        this._symbol = symbol;
        this._caseSensitive = caseSensitive !== undefined ? !!caseSensitive : true;
        this._isRegex = isRegex !== undefined ? !!isRegex : false;
        this._sourceMapped = !!sourceMapped;

        this._symbolRegex = null;
        this._lowercaseSymbol = null;
    }

    // Static

    static supported(target)
    {
        // COMPATIBILITY (iOS 16.0): Debugger.addSymbolicBreakpoint did exist yet.
        return (target || InspectorBackend).hasCommand("Debugger.addSymbolicBreakpoint");
    }

    static fromJSON(json)
    {
        return new WI.SymbolicBreakpoint(json.symbol, {
            caseSensitive: json.caseSensitive,
            isRegex: json.isRegex,
            sourceMapped: json.sourceMapped,
            disabled: json.disabled,
            condition: json.condition,
            actions: json.actions?.map((actionJSON) => WI.BreakpointAction.fromJSON(actionJSON)) || [],
            ignoreCount: json.ignoreCount,
            autoContinue: json.autoContinue,
        });
    }

    // Public

    get symbol() { return this._symbol; }
    get caseSensitive() { return this._caseSensitive; }
    get isRegex() { return this._isRegex; }
    get sourceMapped() { return this._sourceMapped; }

    get displayName()
    {
        let displayName;
        if (this._isRegex)
            displayName = "/" + this._symbol + "/" + (!this._caseSensitive ? "i" : "");
        else {
            displayName = this._symbol;
            if (!this._caseSensitive)
                displayName = WI.UIString("%s (Case Insensitive)", "%s (Case Insensitive) @ Symbolic Breakpoint", "Label for case-insensitive match pattern of a symbolic breakpoint.").format(displayName);
        }
        if (this._sourceMapped)
            displayName = WI.UIString("%s (Source Mapped)", "%s (Source Mapped) @ Symbolic Breakpoint", "Label for a source-mapped symbolic breakpoint.").format(displayName);
        return displayName;
    }

    get editable()
    {
        return true;
    }

    matches(symbol)
    {
        if (!symbol || this.disabled)
            return false;

        if (this._isRegex) {
            this._symbolRegex ||= new RegExp(this._symbol, !this._caseSensitive ? "i" : "");
            return this._symbolRegex.test(symbol);
        }

        if (!this._caseSensitive) {
            this._lowercaseSymbol ||= this._symbol.toLowerCase();
            return this._lowercaseSymbol === symbol.toLowerCase();
        }

        return symbol === this._symbol;
    }

    equals(other)
    {
        console.assert(other instanceof WI.SymbolicBreakpoint, other);

        return this._symbol === other.symbol
            && this._caseSensitive === other.caseSensitive
            && this._isRegex === other.isRegex
            && this._sourceMapped === other.sourceMapped;
    }

    remove()
    {
        super.remove();

        WI.debuggerManager.removeSymbolicBreakpoint(this);
    }

    saveIdentityToCookie(cookie)
    {
        cookie["symbolic-breakpoint-symbol"] = this._symbol;
        cookie["symbolic-breakpoint-symbolic-case-sensitive"] = this._caseSensitive;
        cookie["symbolic-breakpoint-symbolic-is-regex"] = this._isRegex;
        cookie["symbolic-breakpoint-symbolic-source-mapped"] = this._sourceMapped;
    }

    toJSON(key)
    {
        let json = super.toJSON(key);
        json.symbol = this._symbol;
        json.caseSensitive = this._caseSensitive;
        json.isRegex = this._isRegex;
        json.sourceMapped = this._sourceMapped;
        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.symbolicBreakpoints.keyPath] = this._symbol + "-" + this._caseSensitive + "-" + this._isRegex + "-" + this._sourceMapped;
        return json;
    }
};

WI.SymbolicBreakpoint.ReferencePage = WI.ReferencePage.SymbolicBreakpoints;
