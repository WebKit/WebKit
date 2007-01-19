/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

// Debugging helper
// #define DUMP_SHADOW_TREE

#ifdef SVG_SUPPORT
#include "SVGUseElement.h"

#include "CString.h"
#include "Document.h"
#include "HTMLNames.h"
#include "RenderSVGContainer.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"
#include "SVGGElement.h"
#include "SVGHiddenElement.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"

namespace WebCore {

SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGURIReference()
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
{
}

SVGUseElement::~SVGUseElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Y, y, SVGNames::yAttr.localName(), m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height)

SVGElementInstance* SVGUseElement::instanceRoot() const
{
    return m_targetElementInstance.get();
}

SVGElementInstance* SVGUseElement::animatedInstanceRoot() const
{
    // FIXME: Implement me.
    return 0;
}
 
void SVGUseElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(this, LengthModeWidth, value));
        if (width().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(this, LengthModeHeight, value));
        if (height().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <height> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

bool SVGUseElement::hasChildNodes() const
{
    return false;
}

void SVGUseElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();

    String id = SVGURIReference::getTarget(href());
    Element* targetElement = ownerDocument()->getElementById(id);
    if (!targetElement) {
        document()->accessSVGExtensions()->addPendingResource(id, this);
        return;
    }

    buildPendingResource();
}

void SVGUseElement::notifyAttributeChange() const
{
    if (!attached() || ownerDocument()->parsing())
        return;

fprintf(stderr, "NAC NAC NAC NAC HAMMER!!!!!\n");
    const_cast<SVGUseElement*>(this)->buildPendingResource();
}

void SVGUseElement::buildPendingResource()
{
    removeChildren();

    String id = SVGURIReference::getTarget(href());
    Element* targetElement = ownerDocument()->getElementById(id); 
    SVGElement* target = svg_dynamic_cast(targetElement);
    ASSERT(target);

    // Build shadow tree.
    buildShadowTree(target);
    ASSERT(m_targetElementInstance);

    ExceptionCode ec = 0;
    String widthString = String::number(width().value());
    String heightString = String::number(height().value()); 
    String transformString = String::format("translate(%f, %f)", x().value(), y().value());

    // Spec: An additional transformation translate(x,y) is appended to the end
    // (i.e., right-side) of the transform attribute on the generated 'g', where x
    // and y represent the values of the x and y attributes on the 'use' element. 
    RefPtr<SVGElement> gElement = new SVGHiddenElement<SVGGElement>(SVGNames::gTag, document());
    gElement->setAttribute(SVGNames::transformAttr, transformString);

    if (target->hasTagName(SVGNames::symbolTag)) {
        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.
        RefPtr<SVGElement> svgElement = new SVGHiddenElement<SVGSVGElement>(SVGNames::svgTag, document());

        // Transfer all attributes from <symbol> to the new <svg> element
        *svgElement->attributes() = *target->attributes();

        // Explicitely re-set width/height values
        svgElement->setAttribute(SVGNames::widthAttr, hasAttribute(SVGNames::widthAttr) ? widthString : "100%");
        svgElement->setAttribute(SVGNames::heightAttr, hasAttribute(SVGNames::heightAttr) ? heightString : "100%");

        // IMPORTANT! Before inserted into the document, clear the id of the clone!
        ExceptionCode ec = 0;    
        svgElement->removeAttribute(HTMLNames::idAttr, ec);
        ASSERT(ec == 0);

        // Add all children of the <symbol> element to the new <svg> element
        addShadowTree(svgElement.get(), true);

        // Add new <svg> element to <g>
        gElement->appendChild(svgElement.release(), ec);
        ASSERT(ec == 0);
    } else if (target->hasTagName(SVGNames::svgTag)) {
        SVGElement* element = m_targetElementInstance->clonedElement();
        ASSERT(element->hasTagName(SVGNames::svgTag));

        if (hasAttribute(SVGNames::widthAttr))
            element->setAttribute(SVGNames::widthAttr, widthString);
        if (hasAttribute(SVGNames::heightAttr))
            element->setAttribute(SVGNames::heightAttr, heightString);
 
        addShadowTree(gElement.get());
    } else
        addShadowTree(gElement.get());

    // FIXME: The SVGHiddenElement class will go away, as well as our removeAttribute(idAttr) trickery,
    // when we move to using real shadowTree nodes like the form controls do
    appendChild(gElement.release(), ec);
    ASSERT(ec == 0);
}

RenderObject* SVGUseElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGContainer(this);
}

void SVGUseElement::buildShadowTree(SVGElement* target)
{
    // Clone whole target sub-tree
    RefPtr<Node> clone = target->cloneNode(true);
    SVGElement* svgClone = svg_dynamic_cast(clone.get());
    ASSERT(svgClone);

    // IMPORTANT! Before inserted into the document, clear the id of the clone!
    ExceptionCode ec = 0;    
    svgClone->removeAttribute(HTMLNames::idAttr, ec);
    ASSERT(ec == 0);

    // Create root SVGElementInstance object for the first sub-tree node.
    //
    // Spec: If the 'use' element references a simple graphics element such as a 'rect', then there is only a
    // single SVGElementInstance object, and the correspondingElement attribute on this SVGElementInstance object
    // is the SVGRectElement that corresponds to the referenced 'rect' element.
    m_targetElementInstance = new SVGElementInstance(this, svgClone, target);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    recursiveShadowTreeBuilder(target, svgClone, m_targetElementInstance.get());
}

void SVGUseElement::recursiveShadowTreeBuilder(SVGElement* target, SVGElement* clonedTarget, SVGElementInstance* targetInstance)
{
    ASSERT(target);
    ASSERT(clonedTarget);
    ASSERT(targetInstance);

    // A general description from the SVG spec, describing what recursiveShadowTreeBuilder() actually does.
    //
    // Spec: If the 'use' element references a 'g' which contains two 'rect' elements, then the instance tree
    // contains three SVGElementInstance objects, a root SVGElementInstance object whose correspondingElement
    // is the SVGGElement object for the 'g', and then two child SVGElementInstance objects, each of which has
    // its correspondingElement that is an SVGRectElement object.

    // FIXME: Complex deep references. (only simple forward references work)
    // If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
    // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
    Node* node = target->firstChild();
    for (Node* clonedNode = clonedTarget->firstChild(); clonedNode; clonedNode = clonedNode->nextSibling()) {
        SVGElement* element = svg_dynamic_cast(node);
        SVGElement* clonedElement = svg_dynamic_cast(clonedNode);

        // Skip any non-svg nodes.
        if (!element || !clonedElement) {
            if (node)
                node = node->nextSibling();

            continue;
        }

        // IMPORTANT! Before inserted into the document, clear the id of the clone!
        ExceptionCode ec = 0;
        clonedElement->removeAttribute(HTMLNames::idAttr, ec);
        ASSERT(ec == 0);

        // Create SVGElementInstance object, for both container/non-container nodes.
        SVGElementInstance* instance = new SVGElementInstance(this, clonedElement, element);
        targetInstance->appendChild(instance);

        // Enter recursion, adding shadow tree nodes to "instance" as target.
        if (clonedElement->hasChildNodes())
            recursiveShadowTreeBuilder(clonedElement, element, instance);

        node = node->nextSibling();
    }
}

#ifdef DUMP_SHADOW_TREE
void dumpShadowTree(unsigned int& depth, String& text, SVGElementInstance* targetInstance)
{
    SVGElement* element = targetInstance->clonedElement();
    ASSERT(element);

    String elementNodeName = element->nodeName();
    String parentNodeName = element->parentNode() ? element->parentNode()->nodeName() : "null";
    String firstChildNodeName = element->firstChild() ? element->firstChild()->nodeName() : "null";

    for (unsigned int i = 0; i < depth; ++i)
        text += "  ";

    text += String::format("SVGElementInstance (parentNode=%s, firstChild=%s, correspondingElement=%s)\n",
                           parentNodeName.latin1().data(), firstChildNodeName.latin1().data(), elementNodeName.latin1().data());
 
    depth++;

    for (SVGElementInstance* instance = targetInstance->firstChild(); instance; instance = instance->nextSibling())
        dumpShadowTree(depth, text, instance);

    depth--;
}
#endif

void SVGUseElement::addShadowTree(SVGElement* target, bool onlyAddChildren)
{
    ASSERT(m_targetElementInstance && m_targetElementInstance->correspondingUseElement() == this);

#ifdef DUMP_SHADOW_TREE
    unsigned int depth = 0; String text;
    dumpShadowTree(depth, text, m_targetElementInstance.get());
    fprintf(stderr, "Dumping <use> shadow tree:\n%s\n", text.latin1().data());
#endif

    ExceptionCode ec = 0;

    if (onlyAddChildren) {
        for (SVGElementInstance* instance = m_targetElementInstance->firstChild(); instance; instance = instance->nextSibling()) {
            target->appendChild(instance->clonedElement(), ec);
            ASSERT(ec == 0);
        }
    } else {
        target->appendChild(m_targetElementInstance->clonedElement(), ec);
        ASSERT(ec == 0);
    }
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
