/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

// Dump SVGElementInstance object tree - useful to debug instanceRoot problems
// #define DUMP_INSTANCE_TREE

// Dump the deep-expanded shadow tree (where the renderes are built from)
// #define DUMP_SHADOW_TREE

#if ENABLE(SVG)
#include "SVGUseElement.h"

#include "CSSStyleSelector.h"
#include "CString.h"
#include "Document.h"
#include "Event.h"
#include "HTMLNames.h"
#include "RenderSVGContainer.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"
#include "SVGGElement.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"
#include "XLinkNames.h"
#include "XMLSerializer.h"
#include <wtf/OwnPtr.h>

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
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
        if (width().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
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

void SVGUseElement::removedFromDocument()
{
    SVGElement::removedFromDocument();

    m_targetElementInstance = 0;
    m_shadowTreeRootElement = 0;
}

void SVGUseElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    // Avoid calling SVGStyledElement::attributeChanged(), as it always calls notifyAttributeChange.
    SVGElement::attributeChanged(attr, preserveDecls);

    if (!attached())
       return;

    // Only update the tree if x/y/width/height or xlink:href changed.
    if (attr->name() == SVGNames::xAttr || attr->name() == SVGNames::yAttr ||
        attr->name() == SVGNames::widthAttr || attr->name() == SVGNames::heightAttr ||
        attr->name().matches(XLinkNames::hrefAttr))
        buildPendingResource();
    else if (m_shadowTreeRootElement)
        m_shadowTreeRootElement->setChanged();
}

void SVGUseElement::notifyAttributeChange() const
{
    if (!attached())
        return;

    // NOTE: A lot of room for improvments here. This is too slow.
    // It has to be done correctly, by implementing attributeChanged().
    const_cast<SVGUseElement*>(this)->buildPendingResource();

    if (m_shadowTreeRootElement)
        m_shadowTreeRootElement->setChanged();
}

void SVGUseElement::recalcStyle(StyleChange change)
{
    SVGStyledElement::recalcStyle(change);

    // The shadow tree root element is NOT a direct child element of us.
    // So we have to take care it receives style updates, manually.
    if (!m_shadowTreeRootElement || !m_shadowTreeRootElement->attached())
        return;

    // Mimic Element::recalcStyle(). The main difference is that we don't call attach() on the
    // shadow tree root element, but call attachShadowTree() here. Calling attach() will crash
    // as the shadow tree root element has no (direct) parent node. Yes, shadow trees are tricky.
    if (change >= Inherit || m_shadowTreeRootElement->changed()) {
        RenderStyle* newStyle = document()->styleSelector()->styleForElement(m_shadowTreeRootElement.get());
        StyleChange ch = m_shadowTreeRootElement->diff(m_shadowTreeRootElement->renderStyle(), newStyle);
        if (ch == Detach) {
            ASSERT(m_shadowTreeRootElement->attached());
            m_shadowTreeRootElement->detach();
            attachShadowTree();

            // attach recalulates the style for all children. No need to do it twice.
            m_shadowTreeRootElement->setChanged(NoStyleChange);
            m_shadowTreeRootElement->setHasChangedChild(false);
            newStyle->deref(document()->renderArena());
            return;
        }

        newStyle->deref(document()->renderArena());
    }

    // Only change==Detach needs special treatment, for anything else recalcStyle() works.
    m_shadowTreeRootElement->recalcStyle(change);
}

#ifdef DUMP_INSTANCE_TREE
void dumpInstanceTree(unsigned int& depth, String& text, SVGElementInstance* targetInstance)
{
    SVGElement* element = targetInstance->correspondingElement();
    ASSERT(element);

    String elementId = element->getIDAttribute();
    String elementNodeName = element->nodeName();
    String parentNodeName = element->parentNode() ? element->parentNode()->nodeName() : "null";
    String firstChildNodeName = element->firstChild() ? element->firstChild()->nodeName() : "null";

    for (unsigned int i = 0; i < depth; ++i)
        text += "  ";

    text += String::format("SVGElementInstance (parentNode=%s, firstChild=%s, correspondingElement=%s, id=%s)\n",
                           parentNodeName.latin1().data(), firstChildNodeName.latin1().data(), elementNodeName.latin1().data(), elementId.latin1().data());
 
    depth++;

    for (SVGElementInstance* instance = targetInstance->firstChild(); instance; instance = instance->nextSibling())
        dumpInstanceTree(depth, text, instance);

    depth--;
}
#endif

void SVGUseElement::buildPendingResource()
{
    // Do not build the shadow/instance tree for <use> elements living in a shadow tree.
    // The will be expanded soon anyway - see expandUseElementsInShadowTree().
    Node* parent = parentNode();
    while (parent) {
        if (parent->isShadowNode())
            return;

        parent = parent->parentNode();
    }

    String id = SVGURIReference::getTarget(href());
    Element* targetElement = ownerDocument()->getElementById(id); 
    SVGElement* target = svg_dynamic_cast(targetElement);

    // Do not allow self-referencing.
    if (!target || target == this)
        return;

    // Why a seperated instance/shadow tree? SVG demands it:
    // The instance tree is accesable from JavaScript, and has to
    // expose a 1:1 copy of the referenced tree, whereas internally we need
    // to alter the tree for correct "use-on-symbol", "use-on-svg" support.  
 
    // Build instance tree. Create root SVGElementInstance object for the first sub-tree node.
    //
    // Spec: If the 'use' element references a simple graphics element such as a 'rect', then there is only a
    // single SVGElementInstance object, and the correspondingElement attribute on this SVGElementInstance object
    // is the SVGRectElement that corresponds to the referenced 'rect' element.
    m_targetElementInstance = new SVGElementInstance(this, target);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    bool foundCycle = false;
    buildInstanceTree(target, m_targetElementInstance.get(), foundCycle);

    // SVG specification does not say a word about <use> & cycles. My view on this is: just ignore it!
    // Non-appearing <use> content is easier to debug, then half-appearing content.
    if (foundCycle) {
        m_targetElementInstance = 0;
        m_shadowTreeRootElement = 0;
        return;
    }

    // Assure instance tree building was successfull
    ASSERT(m_targetElementInstance);
    ASSERT(m_targetElementInstance->correspondingUseElement() == this);

    // Setup shadow tree root node
    m_shadowTreeRootElement = new SVGGElement(SVGNames::gTag, document());
    m_shadowTreeRootElement->setInDocument();
    m_shadowTreeRootElement->setShadowParentNode(this);

    // Spec: An additional transformation translate(x,y) is appended to the end
    // (i.e., right-side) of the transform attribute on the generated 'g', where x
    // and y represent the values of the x and y attributes on the 'use' element. 
    if (x().value() != 0.0 || y().value() != 0.0) {
        String transformString = String::format("translate(%f, %f)", x().value(), y().value());
        m_shadowTreeRootElement->setAttribute(SVGNames::transformAttr, transformString);
    }

    // Build shadow tree from instance tree
    // This also handles the special cases: <use> on <symbol>, <use> on <svg>.
    buildShadowTree(target, m_targetElementInstance.get());

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
    // Expand all <use> elements in the shadow tree.
    // Expand means: replace the actual <use> element by what it references.
    expandUseElementsInShadowTree(m_shadowTreeRootElement.get());
#endif

    // Now that the shadow tree is completly expanded, we can associate
    // shadow tree elements <-> instances in the instance tree.
    associateInstancesWithShadowTreeElements(m_shadowTreeRootElement->firstChild(), m_targetElementInstance.get());

    // Eventually dump instance tree
#ifdef DUMP_INSTANCE_TREE
    String text;
    unsigned int depth = 0;

    dumpInstanceTree(depth, text, m_targetElementInstance.get());
    fprintf(stderr, "\nDumping <use> instance tree:\n%s\n", text.latin1().data());
#endif

    // Eventually dump shadow tree
#ifdef DUMP_SHADOW_TREE
    ExceptionCode ec = 0;
    OwnPtr<XMLSerializer> serializer(new XMLSerializer());

    String markup = serializer->serializeToString(m_shadowTreeRootElement.get(), ec);
    ASSERT(ec == 0);

    fprintf(stderr, "Dumping <use> shadow tree markup:\n%s\n", markup.latin1().data());
#endif

    // The DOM side is setup properly. Now we have to attach the root shadow
    // tree element manually - using attach() won't work for "shadow nodes".
    attachShadowTree();
}

RenderObject* SVGUseElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGContainer(this);
}

void SVGUseElement::attach()
{
    SVGStyledTransformableElement::attach();

    // If we're a pending resource, this doesn't have any effect.
    attachShadowTree();
}

void SVGUseElement::buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundCycle)
{
    ASSERT(target);
    ASSERT(targetInstance);

    // A general description from the SVG spec, describing what buildInstanceTree() actually does.
    //
    // Spec: If the 'use' element references a 'g' which contains two 'rect' elements, then the instance tree
    // contains three SVGElementInstance objects, a root SVGElementInstance object whose correspondingElement
    // is the SVGGElement object for the 'g', and then two child SVGElementInstance objects, each of which has
    // its correspondingElement that is an SVGRectElement object.

    for (Node* node = target->firstChild(); node; node = node->nextSibling()) {
        SVGElement* element = svg_dynamic_cast(node);

        // Skip any non-svg nodes.
        if (!element)
            continue;

        // Create SVGElementInstance object, for both container/non-container nodes.
        SVGElementInstance* instancePtr = new SVGElementInstance(this, element);

        RefPtr<SVGElementInstance> instance = instancePtr;
        targetInstance->appendChild(instance.release());

        // Enter recursion, appending new instance tree nodes to the "instance" object.
        if (element->hasChildNodes())
            buildInstanceTree(element, instancePtr, foundCycle);

        // Spec: If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
        // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
        if (element->hasTagName(SVGNames::useTag))
            handleDeepUseReferencing(element, instancePtr, foundCycle);
    }

    // Spec: If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
    // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
    if (target->hasTagName(SVGNames::useTag))
        handleDeepUseReferencing(target, targetInstance, foundCycle);
}

void SVGUseElement::handleDeepUseReferencing(SVGElement* use, SVGElementInstance* targetInstance, bool& foundCycle)
{
    String id = SVGURIReference::getTarget(use->href());
    Element* targetElement = ownerDocument()->getElementById(id); 
    SVGElement* target = svg_dynamic_cast(targetElement);

    if (!target)
        return;

    // Cycle detection first!
    foundCycle = (target == this);

    // Shortcut for self-references
    if (foundCycle)
        return;

    SVGElementInstance* instance = targetInstance->parentNode();
    while (instance) {
        SVGElement* element = instance->correspondingElement();

        if (element->getIDAttribute() == id) {
            foundCycle = true;
            return;
        }
    
        instance = instance->parentNode();
    }

    // Create an instance object, even if we're dealing with a cycle
    SVGElementInstance* newInstance = new SVGElementInstance(this, target);
    targetInstance->appendChild(newInstance);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    buildInstanceTree(target, newInstance, foundCycle);
}

PassRefPtr<SVGSVGElement> SVGUseElement::buildShadowTreeForSymbolTag(SVGElement* target, SVGElementInstance* targetInstance)
{
    ExceptionCode ec = 0;

    String widthString = String::number(width().value());
    String heightString = String::number(height().value());
 
    // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
    // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
    // always have explicit values for attributes width and height. If attributes width and/or
    // height are provided on the 'use' element, then these attributes will be transferred to
    // the generated 'svg'. If attributes width and/or height are not specified, the generated
    // 'svg' element will use values of 100% for these attributes.
    RefPtr<SVGSVGElement> svgElement = new SVGSVGElement(SVGNames::svgTag, document());

    // Transfer all attributes from <symbol> to the new <svg> element
    *svgElement->attributes() = *target->attributes();

    // Explicitly re-set width/height values
    svgElement->setAttribute(SVGNames::widthAttr, hasAttribute(SVGNames::widthAttr) ? widthString : "100%");
    svgElement->setAttribute(SVGNames::heightAttr, hasAttribute(SVGNames::heightAttr) ? heightString : "100%");

    // Only clone symbol children, and add them to the new <svg> element    
    if (targetInstance) {
        // Called from buildShadowTree()
        for (SVGElementInstance* instance = targetInstance->firstChild(); instance; instance = instance->nextSibling()) {
            RefPtr<Node> newChild = instance->correspondingElement()->cloneNode(true);
            svgElement->appendChild(newChild.release(), ec);
            ASSERT(ec == 0);
        }
    } else {
        // Called from expandUseElementsInShadowTree()
        for (Node* child = target->firstChild(); child; child = child->nextSibling()) {
            RefPtr<Node> newChild = child->cloneNode(true);
            svgElement->appendChild(newChild.release(), ec);
            ASSERT(ec == 0);
        }
    }

    return svgElement;
}

void SVGUseElement::alterShadowTreeForSVGTag(SVGElement* target)
{
    String widthString = String::number(width().value());
    String heightString = String::number(height().value());

    if (hasAttribute(SVGNames::widthAttr))
        target->setAttribute(SVGNames::widthAttr, widthString);

    if (hasAttribute(SVGNames::heightAttr))
        target->setAttribute(SVGNames::heightAttr, heightString);
}

void SVGUseElement::buildShadowTree(SVGElement* target, SVGElementInstance* targetInstance)
{
    ExceptionCode ec = 0;

    RefPtr<Node> newChild;

    // Handle use referencing <symbol> special case
    if (target->hasTagName(SVGNames::symbolTag))
        newChild = buildShadowTreeForSymbolTag(target, targetInstance);
    else
        newChild = targetInstance->correspondingElement()->cloneNode(true);

    SVGElement* newChildPtr = svg_dynamic_cast(newChild.get());
    ASSERT(newChildPtr);

    m_shadowTreeRootElement->appendChild(newChild.release(), ec);
    ASSERT(ec == 0);

    // Handle use referencing <svg> special case
    if (target->hasTagName(SVGNames::svgTag))
        alterShadowTreeForSVGTag(newChildPtr);
}

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
void SVGUseElement::expandUseElementsInShadowTree(Node* element)
{
    // Why expand the <use> elements in the shadow tree here, and not just
    // do this directly in buildShadowTree, if we encounter a <use> element?
    //
    // Short answer: Because we may miss to expand some elements. Ie. if a <symbol>
    // contains <use> tags, we'd miss them. So once we're done with settin' up the
    // actual shadow tree (after the special case modification for svg/symbol) we have
    // to walk it completely and expand all <use> elements.
    if (element->hasTagName(SVGNames::useTag)) {
        SVGUseElement* use = static_cast<SVGUseElement*>(element);

        String id = SVGURIReference::getTarget(use->href());
        Element* targetElement = ownerDocument()->getElementById(id); 
        SVGElement* target = svg_dynamic_cast(targetElement);

        // Don't ASSERT(target) here, it may be "pending", too.
        if (target) {
            // Setup sub-shadow tree root node
            RefPtr<SVGElement> cloneParent = new SVGGElement(SVGNames::gTag, document());

            // Spec: In the generated content, the 'use' will be replaced by 'g', where all attributes from the
            // 'use' element except for x, y, width, height and xlink:href are transferred to the generated 'g' element.
            transferUseAttributesToReplacedElement(use, cloneParent.get());

            // Spec: An additional transformation translate(x,y) is appended to the end
            // (i.e., right-side) of the transform attribute on the generated 'g', where x
            // and y represent the values of the x and y attributes on the 'use' element.
            if (use->x().value() != 0.0 || use->y().value() != 0.0) {
                if (!cloneParent->hasAttribute(SVGNames::transformAttr)) {
                    String transformString = String::format("translate(%f, %f)", use->x().value(), use->y().value());
                    cloneParent->setAttribute(SVGNames::transformAttr, transformString);
                } else {
                    String transformString = String::format(" translate(%f, %f)", use->x().value(), use->y().value());
                    const AtomicString& transformAttribute = cloneParent->getAttribute(SVGNames::transformAttr);
                    cloneParent->setAttribute(SVGNames::transformAttr, transformAttribute + transformString); 
                }
            }

            RefPtr<Node> newChild;

            // Handle use referencing <symbol> special case
            if (target->hasTagName(SVGNames::symbolTag))
                newChild = buildShadowTreeForSymbolTag(target, 0);
            else
                newChild = target->cloneNode(true);

            SVGElement* newChildPtr = svg_dynamic_cast(newChild.get());
            ASSERT(newChildPtr);

            ExceptionCode ec = 0;
            cloneParent->appendChild(newChild.release(), ec);
            ASSERT(ec == 0);

            // Replace <use> with referenced content.
            ASSERT(use->parentNode()); 
            use->parentNode()->replaceChild(cloneParent.release(), use, ec);
            ASSERT(ec == 0);

            // Handle use referencing <svg> special case
            if (target->hasTagName(SVGNames::svgTag))
                alterShadowTreeForSVGTag(newChildPtr);

            // Immediately stop here, and restart expanding.
            expandUseElementsInShadowTree(m_shadowTreeRootElement.get());
            return;
        }
    }

    for (RefPtr<Node> child = element->firstChild(); child; child = child->nextSibling())
        expandUseElementsInShadowTree(child.get());
}
#endif
    
void SVGUseElement::attachShadowTree()
{
    if (!m_shadowTreeRootElement || m_shadowTreeRootElement->attached() || !document()->shouldCreateRenderers() || !attached() || !renderer())
        return;

    // Inspired by RenderTextControl::createSubtreeIfNeeded(). 
    if (renderer()->canHaveChildren() && childShouldCreateRenderer(m_shadowTreeRootElement.get())) {
        RenderStyle* style = m_shadowTreeRootElement->styleForRenderer(renderer());

        if (m_shadowTreeRootElement->rendererIsNeeded(style)) {
            m_shadowTreeRootElement->setRenderer(m_shadowTreeRootElement->createRenderer(document()->renderArena(), style));
            if (RenderObject* shadowRenderer = m_shadowTreeRootElement->renderer()) {
                shadowRenderer->setStyle(style);
                renderer()->addChild(shadowRenderer, m_shadowTreeRootElement->nextRenderer());

                // Mimic SVGStyledTransformableElement::attach() functionality
                SVGGElement* gElement = static_cast<SVGGElement*>(m_shadowTreeRootElement.get());
                shadowRenderer->setLocalTransform(gElement->localMatrix());

                m_shadowTreeRootElement->setAttached();
            }
        }

        style->deref(document()->renderArena());

        // This will take care of attaching all shadow tree child nodes.
        for (Node* child = m_shadowTreeRootElement->firstChild(); child; child = child->nextSibling())
            child->attach();
    }
}

void SVGUseElement::associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance)
{
    if (!target || !targetInstance)
        return;

    SVGElement* originalElement = targetInstance->correspondingElement();

    if (originalElement->hasTagName(SVGNames::useTag)) {
#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
        // <use> gets replaced by <g>
        ASSERT(target->nodeName() == SVGNames::gTag);
#else 
        ASSERT(target->nodeName() == SVGNames::gTag || target->nodeName() == SVGNames::useTag);
#endif
    } else if (originalElement->hasTagName(SVGNames::symbolTag)) {
        // <symbol> gets replaced by <svg>
        ASSERT(target->nodeName() == SVGNames::svgTag);
    } else
        ASSERT(target->nodeName() == originalElement->nodeName());

    SVGElement* element = svg_dynamic_cast(target);

    ASSERT(!targetInstance->shadowTreeElement());
    targetInstance->setShadowTreeElement(element);

    Node* node = target->firstChild();
    for (SVGElementInstance* instance = targetInstance->firstChild(); node && instance; instance = instance->nextSibling()) {
        // Skip any non-svg elements in shadow tree
        while (node && !node->isSVGElement())
           node = node->nextSibling();

        associateInstancesWithShadowTreeElements(node, instance);
        node = node->nextSibling();
    }
}

SVGElementInstance* SVGUseElement::instanceForShadowTreeElement(Node* element) const
{
    return instanceForShadowTreeElement(element, m_targetElementInstance.get());
}

SVGElementInstance* SVGUseElement::instanceForShadowTreeElement(Node* element, SVGElementInstance* instance) const
{
    ASSERT(element);
    ASSERT(instance);
    ASSERT(instance->shadowTreeElement());

    if (element == instance->shadowTreeElement())
        return instance;

    for (SVGElementInstance* current = instance->firstChild(); current; current = current->nextSibling()) {
        SVGElementInstance* search = instanceForShadowTreeElement(element, current);
        if (search)
            return search;
    }

    return 0;
}

void SVGUseElement::transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const
{
    ASSERT(from);
    ASSERT(to);

    *to->attributes() = *from->attributes();

    ExceptionCode ec = 0;

    to->removeAttribute(SVGNames::xAttr, ec);
    ASSERT(ec == 0);

    to->removeAttribute(SVGNames::yAttr, ec);
    ASSERT(ec == 0);

    to->removeAttribute(SVGNames::widthAttr, ec);
    ASSERT(ec == 0);

    to->removeAttribute(SVGNames::heightAttr, ec);
    ASSERT(ec == 0);

    to->removeAttribute(XLinkNames::hrefAttr, ec);
    ASSERT(ec == 0);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
