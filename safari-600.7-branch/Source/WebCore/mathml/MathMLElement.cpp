/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MATHML)

#include "MathMLElement.h"

#include "ElementIterator.h"
#include "HTMLElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "MathMLSelectElement.h"
#include "RenderTableCell.h"
#include "SVGElement.h"
#include "SVGNames.h"

namespace WebCore {
    
using namespace MathMLNames;
    
MathMLElement::MathMLElement(const QualifiedName& tagName, Document& document)
    : StyledElement(tagName, document, CreateMathMLElement)
{
}
    
PassRefPtr<MathMLElement> MathMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new MathMLElement(tagName, document));
}

bool MathMLElement::isPresentationMathML() const
{
    return hasTagName(MathMLNames::mtrTag)
        || hasTagName(MathMLNames::mtdTag)
        || hasTagName(MathMLNames::maligngroupTag)
        || hasTagName(MathMLNames::malignmarkTag)
        || hasTagName(MathMLNames::mencloseTag)
        || hasTagName(MathMLNames::mglyphTag)
        || hasTagName(MathMLNames::mlabeledtrTag)
        || hasTagName(MathMLNames::mlongdivTag)
        || hasTagName(MathMLNames::mpaddedTag)
        || hasTagName(MathMLNames::msTag)
        || hasTagName(MathMLNames::mscarriesTag)
        || hasTagName(MathMLNames::mscarryTag)
        || hasTagName(MathMLNames::msgroupTag)
        || hasTagName(MathMLNames::mslineTag)
        || hasTagName(MathMLNames::msrowTag)
        || hasTagName(MathMLNames::mstackTag);
}

bool MathMLElement::isPhrasingContent(const Node& node) const
{
    // Phrasing content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#phrasing-content.

    if (!node.isElementNode())
        return node.isTextNode();

    if (node.isMathMLElement()) {
        auto& mathmlElement = toMathMLElement(node);
        return mathmlElement.hasTagName(MathMLNames::mathTag);
    }

    if (node.isSVGElement()) {
        auto& svgElement = toSVGElement(node);
        return svgElement.hasTagName(SVGNames::svgTag);
    }

    if (node.isHTMLElement()) {
        // FIXME: add the <data> and <time> tags when they are implemented.
        auto& htmlElement = toHTMLElement(node);
        return htmlElement.hasTagName(HTMLNames::aTag)
            || htmlElement.hasTagName(HTMLNames::abbrTag)
            || (htmlElement.hasTagName(HTMLNames::areaTag) && ancestorsOfType<HTMLMapElement>(htmlElement).first())
            || htmlElement.hasTagName(HTMLNames::audioTag)
            || htmlElement.hasTagName(HTMLNames::bTag)
            || htmlElement.hasTagName(HTMLNames::bdiTag)
            || htmlElement.hasTagName(HTMLNames::bdoTag)
            || htmlElement.hasTagName(HTMLNames::brTag)
            || htmlElement.hasTagName(HTMLNames::buttonTag)
            || htmlElement.hasTagName(HTMLNames::canvasTag)
            || htmlElement.hasTagName(HTMLNames::citeTag)
            || htmlElement.hasTagName(HTMLNames::codeTag)
            || htmlElement.hasTagName(HTMLNames::datalistTag)
            || htmlElement.hasTagName(HTMLNames::delTag)
            || htmlElement.hasTagName(HTMLNames::dfnTag)
            || htmlElement.hasTagName(HTMLNames::emTag)
            || htmlElement.hasTagName(HTMLNames::embedTag)
            || htmlElement.hasTagName(HTMLNames::iTag)
            || htmlElement.hasTagName(HTMLNames::iframeTag)
            || htmlElement.hasTagName(HTMLNames::imgTag)
            || htmlElement.hasTagName(HTMLNames::inputTag)
            || htmlElement.hasTagName(HTMLNames::insTag)
            || htmlElement.hasTagName(HTMLNames::kbdTag)
            || htmlElement.hasTagName(HTMLNames::keygenTag)
            || htmlElement.hasTagName(HTMLNames::labelTag)
            || htmlElement.hasTagName(HTMLNames::mapTag)
            || htmlElement.hasTagName(HTMLNames::markTag)
            || htmlElement.hasTagName(HTMLNames::meterTag)
            || htmlElement.hasTagName(HTMLNames::noscriptTag)
            || htmlElement.hasTagName(HTMLNames::objectTag)
            || htmlElement.hasTagName(HTMLNames::outputTag)
            || htmlElement.hasTagName(HTMLNames::progressTag)
            || htmlElement.hasTagName(HTMLNames::qTag)
            || htmlElement.hasTagName(HTMLNames::rubyTag)
            || htmlElement.hasTagName(HTMLNames::sTag)
            || htmlElement.hasTagName(HTMLNames::sampTag)
            || htmlElement.hasTagName(HTMLNames::scriptTag)
            || htmlElement.hasTagName(HTMLNames::selectTag)
            || htmlElement.hasTagName(HTMLNames::smallTag)
            || htmlElement.hasTagName(HTMLNames::spanTag)
            || htmlElement.hasTagName(HTMLNames::strongTag)
            || htmlElement.hasTagName(HTMLNames::subTag)
            || htmlElement.hasTagName(HTMLNames::supTag)
            || htmlElement.hasTagName(HTMLNames::templateTag)
            || htmlElement.hasTagName(HTMLNames::textareaTag)
            || htmlElement.hasTagName(HTMLNames::uTag)
            || htmlElement.hasTagName(HTMLNames::varTag)
            || htmlElement.hasTagName(HTMLNames::videoTag)
            || htmlElement.hasTagName(HTMLNames::wbrTag);
    }

    return false;
}

bool MathMLElement::isFlowContent(const Node& node) const
{
    // Flow content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#flow-content

    if (isPhrasingContent(node))
        return true;

    if (!node.isHTMLElement())
        return false;

    auto& htmlElement = toHTMLElement(node);
    // FIXME add the <dialog> tag when it is implemented.
    return htmlElement.hasTagName(HTMLNames::addressTag)
        || htmlElement.hasTagName(HTMLNames::articleTag)
        || htmlElement.hasTagName(HTMLNames::asideTag)
        || htmlElement.hasTagName(HTMLNames::blockquoteTag)
        || htmlElement.hasTagName(HTMLNames::detailsTag)
        || htmlElement.hasTagName(HTMLNames::divTag)
        || htmlElement.hasTagName(HTMLNames::dlTag)
        || htmlElement.hasTagName(HTMLNames::fieldsetTag)
        || htmlElement.hasTagName(HTMLNames::figureTag)
        || htmlElement.hasTagName(HTMLNames::footerTag)
        || htmlElement.hasTagName(HTMLNames::formTag)
        || htmlElement.hasTagName(HTMLNames::h1Tag)
        || htmlElement.hasTagName(HTMLNames::h2Tag)
        || htmlElement.hasTagName(HTMLNames::h3Tag)
        || htmlElement.hasTagName(HTMLNames::h4Tag)
        || htmlElement.hasTagName(HTMLNames::h5Tag)
        || htmlElement.hasTagName(HTMLNames::h6Tag)
        || htmlElement.hasTagName(HTMLNames::headerTag)
        || htmlElement.hasTagName(HTMLNames::hrTag)
        || htmlElement.hasTagName(HTMLNames::mainTag)
        || htmlElement.hasTagName(HTMLNames::navTag)
        || htmlElement.hasTagName(HTMLNames::olTag)
        || htmlElement.hasTagName(HTMLNames::pTag)
        || htmlElement.hasTagName(HTMLNames::preTag)
        || htmlElement.hasTagName(HTMLNames::sectionTag)
        || (htmlElement.hasTagName(HTMLNames::styleTag) && htmlElement.hasAttribute("scoped"))
        || htmlElement.hasTagName(HTMLNames::tableTag)
        || htmlElement.hasTagName(HTMLNames::ulTag);
}

int MathMLElement::colSpan() const
{
    if (!hasTagName(mtdTag))
        return 1;
    const AtomicString& colSpanValue = fastGetAttribute(columnspanAttr);
    return std::max(1, colSpanValue.toInt());
}

int MathMLElement::rowSpan() const
{
    if (!hasTagName(mtdTag))
        return 1;
    const AtomicString& rowSpanValue = fastGetAttribute(rowspanAttr);
    return std::max(1, rowSpanValue.toInt());
}

void MathMLElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == rowspanAttr) {
        if (renderer() && renderer()->isTableCell() && hasTagName(mtdTag))
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else if (name == columnspanAttr) {
        if (renderer() && renderer()->isTableCell() && hasTagName(mtdTag))
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else
        StyledElement::parseAttribute(name, value);
}

bool MathMLElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == colorAttr || name == dirAttr || name == fontfamilyAttr || name == fontsizeAttr || name == fontstyleAttr || name == fontweightAttr || name == mathbackgroundAttr || name == mathcolorAttr || name == mathsizeAttr)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

void MathMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == mathbackgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == mathsizeAttr) {
        // The following three values of mathsize are handled in WebCore/css/mathml.css
        if (value != "normal" && value != "small" && value != "big")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, value);
    } else if (name == mathcolorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    // FIXME: deprecated attributes that should loose in a conflict with a non deprecated attribute
    else if (name == fontsizeAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, value);
    else if (name == backgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == colorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    else if (name == fontstyleAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontStyle, value);
    else if (name == fontweightAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontWeight, value);
    else if (name == fontfamilyAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontFamily, value);
    else if (name == dirAttr) {
        if (hasTagName(mathTag) || hasTagName(mrowTag) || hasTagName(mstyleTag) || isMathMLToken())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, value);
    }  else {
        ASSERT(!isPresentationAttribute(name));
        StyledElement::collectStyleForPresentationAttribute(name, value
        , style);
    }
}

bool MathMLElement::childShouldCreateRenderer(const Node& child) const
{
    if (hasTagName(annotation_xmlTag)) {
        const AtomicString& value = fastGetAttribute(MathMLNames::encodingAttr);

        // See annotation-xml.model.mathml, annotation-xml.model.svg and annotation-xml.model.xhtml in the HTML5 RelaxNG schema.

        if (child.isMathMLElement() && (MathMLSelectElement::isMathMLEncoding(value) || MathMLSelectElement::isHTMLEncoding(value))) {
            auto& mathmlElement = toMathMLElement(child);
            return mathmlElement.hasTagName(MathMLNames::mathTag);
        }

        if (child.isSVGElement() && (MathMLSelectElement::isSVGEncoding(value) || MathMLSelectElement::isHTMLEncoding(value))) {
            auto& svgElement = toSVGElement(child);
            return svgElement.hasTagName(SVGNames::svgTag);
        }

        if (child.isHTMLElement() && MathMLSelectElement::isHTMLEncoding(value)) {
            auto& htmlElement = toHTMLElement(child);
            return htmlElement.hasTagName(HTMLNames::htmlTag) || (isFlowContent(htmlElement) && StyledElement::childShouldCreateRenderer(child));
        }

        return false;
    }

    // In general, only MathML children are allowed. Text nodes are only visible in token MathML elements.
    return child.isMathMLElement();
}

void MathMLElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (isSemanticAnnotation() && (name == MathMLNames::srcAttr || name == MathMLNames::encodingAttr)) {
        Element* parent = parentElement();
        if (parent && parent->isMathMLElement() && parent->hasTagName(semanticsTag))
            toMathMLElement(parent)->updateSelectedChild();
    }
    StyledElement::attributeChanged(name, oldValue, newValue, reason);
}

}

#endif // ENABLE(MATHML)
