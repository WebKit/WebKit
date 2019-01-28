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

WI.CSSRule = class CSSRule extends WI.Object
{
    constructor(nodeStyles, ownerStyleSheet, id, type, sourceCodeLocation, selectorText, selectors, matchedSelectorIndices, style, mediaList)
    {
        super();

        console.assert(nodeStyles);
        this._nodeStyles = nodeStyles;

        this._ownerStyleSheet = ownerStyleSheet || null;
        this._id = id || null;
        this._type = type || null;
        this._initialState = null;

        this.update(sourceCodeLocation, selectorText, selectors, matchedSelectorIndices, style, mediaList, true);
    }

    // Public

    get id() { return this._id; }
    get initialState() { return this._initialState; }

    get stringId()
    {
        if (this._id)
            return this._id.styleSheetId + "/" + this._id.ordinal;
    }

    get ownerStyleSheet()
    {
        return this._ownerStyleSheet;
    }

    get editable()
    {
        return !!this._id && (this._type === WI.CSSStyleSheet.Type.Author || this._type === WI.CSSStyleSheet.Type.Inspector);
    }

    update(sourceCodeLocation, selectorText, selectors, matchedSelectorIndices, style, mediaList, dontFireEvents)
    {
        sourceCodeLocation = sourceCodeLocation || null;
        selectorText = selectorText || "";
        selectors = selectors || [];
        matchedSelectorIndices = matchedSelectorIndices || [];
        style = style || null;
        mediaList = mediaList || [];

        var changed = false;
        if (!dontFireEvents) {
            changed = this._selectorText !== selectorText || !Array.shallowEqual(this._selectors, selectors) ||
                !Array.shallowEqual(this._matchedSelectorIndices, matchedSelectorIndices) || this._style !== style ||
                !!this._sourceCodeLocation !== !!sourceCodeLocation || this._mediaList.length !== mediaList.length;
            // FIXME: Look for differences in the media list arrays.
        }

        if (this._style)
            this._style.ownerRule = null;

        this._sourceCodeLocation = sourceCodeLocation;
        this._selectorText = selectorText;
        this._selectors = selectors;
        this._matchedSelectorIndices = matchedSelectorIndices;
        this._style = style;
        this._mediaList = mediaList;

        if (this._style)
            this._style.ownerRule = this;

        if (changed)
            this.dispatchEventToListeners(WI.CSSRule.Event.Changed);
    }

    get type()
    {
        return this._type;
    }

    get sourceCodeLocation()
    {
        return this._sourceCodeLocation;
    }

    get selectorText()
    {
        return this._selectorText;
    }

    set selectorText(selectorText)
    {
        console.assert(this.editable);
        if (!this.editable)
            return;

        if (this._selectorText === selectorText) {
            this._selectorResolved(true);
            return;
        }

        this._nodeStyles.changeRuleSelector(this, selectorText).then(this._selectorResolved.bind(this), this._selectorRejected.bind(this));
    }

    get selectors()
    {
        return this._selectors;
    }

    get matchedSelectorIndices()
    {
        return this._matchedSelectorIndices;
    }

    get style()
    {
        return this._style;
    }

    get mediaList()
    {
        return this._mediaList;
    }

    isEqualTo(rule)
    {
        if (!rule)
            return false;

        return Object.shallowEqual(this._id, rule.id);
    }

    markModified()
    {
        if (this._initialState)
            return;

        let initialStyle = this._style.initialState || this._style;
        this._initialState = new WI.CSSRule(
            this._nodeStyles,
            this._ownerStyleSheet,
            this._id,
            this._type,
            this._sourceCodeLocation,
            this._selectorText,
            this._selectors,
            this._matchedSelectorIndices,
            initialStyle,
            this._mediaList);

        WI.cssManager.addModifiedCSSRule(this);
    }

    // Protected

    get nodeStyles()
    {
        return this._nodeStyles;
    }

    // Private

    _selectorRejected(error)
    {
        this.dispatchEventToListeners(WI.CSSRule.Event.SelectorChanged, {valid: !error});
    }

    _selectorResolved(rulePayload)
    {
        this.dispatchEventToListeners(WI.CSSRule.Event.SelectorChanged, {valid: !!rulePayload});
    }
};

WI.CSSRule.Event = {
    Changed: "css-rule-changed",
    SelectorChanged: "css-rule-invalid-selector"
};
