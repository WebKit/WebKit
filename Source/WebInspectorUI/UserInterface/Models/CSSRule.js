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

        this.update(sourceCodeLocation, selectorText, selectors, matchedSelectorIndices, style, mediaList, true);
    }

    // Public

    get id()
    {
        return this._id;
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
        this._mostSpecificSelector = null;
        this._style = style;
        this._mediaList = mediaList;

        this._matchedSelectors = null;
        this._matchedSelectorText = null;

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

    get matchedSelectors()
    {
        if (this._matchedSelectors)
            return this._matchedSelectors;

        this._matchedSelectors = this._selectors.filter(function(element, index) {
            return this._matchedSelectorIndices.includes(index);
        }, this);

        return this._matchedSelectors;
    }

    get matchedSelectorText()
    {
        if ("_matchedSelectorText" in this)
            return this._matchedSelectorText;

        this._matchedSelectorText = this.matchedSelectors.map(function(x) { return x.text; }).join(", ");

        return this._matchedSelectorText;
    }

    hasMatchedPseudoElementSelector()
    {
        if (this.nodeStyles && this.nodeStyles.node && this.nodeStyles.node.isPseudoElement())
            return true;

        return this.matchedSelectors.some((selector) => selector.isPseudoElementSelector());
    }

    get style()
    {
        return this._style;
    }

    get mediaList()
    {
        return this._mediaList;
    }

    get mediaText()
    {
        if (!this._mediaList.length)
            return "";

        let mediaText = "";
        for (let media of this._mediaList)
            mediaText += media.text;

        return mediaText;
    }

    isEqualTo(rule)
    {
        if (!rule)
            return false;

        return Object.shallowEqual(this._id, rule.id);
    }

    get mostSpecificSelector()
    {
        if (!this._mostSpecificSelector)
            this._mostSpecificSelector = this._determineMostSpecificSelector();

        return this._mostSpecificSelector;
    }

    selectorIsGreater(otherSelector)
    {
        var mostSpecificSelector = this.mostSpecificSelector;

        if (!mostSpecificSelector)
            return false;

        return mostSpecificSelector.isGreaterThan(otherSelector);
    }

    // Protected

    get nodeStyles()
    {
        return this._nodeStyles;
    }

    // Private

    _determineMostSpecificSelector()
    {
        if (!this._selectors || !this._selectors.length)
            return null;

        var selectors = this.matchedSelectors;

        if (!selectors.length)
            selectors = this._selectors;

        var specificSelector = selectors[0];

        for (var selector of selectors) {
            if (selector.isGreaterThan(specificSelector))
                specificSelector = selector;
        }

        return specificSelector;
    }

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
