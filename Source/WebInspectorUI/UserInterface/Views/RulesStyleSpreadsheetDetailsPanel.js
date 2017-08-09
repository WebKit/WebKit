/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.RulesStyleSpreadsheetDetailsPanel = class RulesStyleSpreadsheetDetailsPanel extends WI.StyleDetailsPanel
{
    constructor(delegate)
    {
        super(delegate, "rules", "rules", WI.UIString("Styles \u2014 Rules"));

        let styleSpreadsheetElement = document.createElement("div");
        styleSpreadsheetElement.classList.add("style-spreadsheet");
        styleSpreadsheetElement.innerHTML = `
<section class="style-rule"><span class="selector selector-inline">Style Attribute</span> { }</section>

<section class="style-rule">
<span class="styles-source"><a href="#"><span class="resource-name">styleRule.css</span><span class="line-info">:24</span></a></span>
<span class="selector-line"><span class="selector"><span class="matched">h1</span>, h2</span> {</span>
<div class="declarations">
    <div class="property"><input type="checkbox" checked><span class="name">font</span>: <span class="value">14px/32px sans-serif</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">margin</span>: <span class="value">0</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">padding</span>: <span class="value">8px 16px</span>;</div>
}</div></section>

<section class="style-rule">
<span class="styles-source"><a href="#"><span class="resource-name">webkit.org</span><span class="line-info">:110</span></a></span>
<span class="selector-line"><span class="selector"><span class="matched">.home .page-layer</span></span> {</span>
<div class="declarations">
    <div class="property"><input type="checkbox" checked><span class="name">background-color</span>: <span class="value">#f7f7f7</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">border-top</span>: <span class="value">1px solid #e7e7e7</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">position</span>: <span class="value">relative</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">z-index</span>: <span class="value">1</span>;</div>
}</div></section>

<section class="inherited"><strong>Inherited From: </strong><span role="link" title="body" class="node-link">body</span></section>

<section class="style-rule">
<span class="styles-source"><a href="#"><span class="resource-name">style.css</span><span class="line-info">:39</span></a></span>
<span class="selector-line"><span class="selector"><span class="matched">body</span></span> {</span>
<div class="declarations">
    <div class="property"><input type="checkbox" checked><span class="name">background-color</span>: <span class="value">#f7f7f7</span>;</div>
    <div class="property"><input type="checkbox" checked><span class="name">font-size</span>: <span class="value">1.6rem</span>;</div>
    <div class="property property-disabled"><input type="checkbox">/* <span class="name">font-weight</span>: <span class="value">400</span>; */</div>
    <div class="property"><input type="checkbox" checked><span class="name">line-height</span>: <span class="value">1.4</span>;</div>
}</div></section>`;

        this.element.append(styleSpreadsheetElement);
    }

    // Public

    filterDidChange()
    {
        // Called by WI.CSSStyleDetailsSidebarPanel.prototype._filterDidChange.
        // Unimplemented.
    }

    scrollToSectionAndHighlightProperty(property)
    {
        // Unimplemented.
    }
};
