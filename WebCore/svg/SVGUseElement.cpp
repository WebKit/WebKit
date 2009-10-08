/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2009. All rights reserved.

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

#if ENABLE(SVG)
#include "SVGUseElement.h"

#include "CSSStyleSelector.h"
#include "CString.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "NodeRenderStyle.h"
#include "RegisteredEventListener.h"
#include "RenderSVGTransformableContainer.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"
#include "SVGGElement.h"
#include "SVGLength.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSMILElement.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"
#include "XLinkNames.h"
#include "XMLSerializer.h"

// Dump SVGElementInstance object tree - useful to debug instanceRoot problems
// #define DUMP_INSTANCE_TREE

// Dump the deep-expanded shadow tree (where the renderers are built from)
// #define DUMP_SHADOW_TREE

namespace WebCore {

SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGURIReference()
    , m_x(this, SVGNames::xAttr, LengthModeWidth)
    , m_y(this, SVGNames::yAttr, LengthModeHeight)
    , m_width(this, SVGNames::widthAttr, LengthModeWidth)
    , m_height(this, SVGNames::heightAttr, LengthModeHeight)
    , m_href(this, XLinkNames::hrefAttr)
{
}

SVGUseElement::~SVGUseElement()
{
}

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
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(LengthModeWidth, attr->value()));
        if (widthBaseValue().value(this) < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(LengthModeHeight, attr->value()));
        if (heightBaseValue().value(this) < 0.0)
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
    buildPendingResource();
}

void SVGUseElement::removedFromDocument()
{
    m_targetElementInstance = 0;
    m_shadowTreeRootElement = 0;
    SVGElement::removedFromDocument();
}

void SVGUseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    if (!attached())
        return;

    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGURIReference::isKnownAttribute(attrName) ||
        SVGStyledTransformableElement::isKnownAttribute(attrName)) {
        buildPendingResource();

        if (m_shadowTreeRootElement)
            m_shadowTreeRootElement->setNeedsStyleRecalc();
    }
}

void SVGUseElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!attached())
        return;

    buildPendingResource();

    if (m_shadowTreeRootElement)
        m_shadowTreeRootElement->setNeedsStyleRecalc();
}
 
static bool shadowTreeContainsChangedNodes(SVGElementInstance* target)
{
    if (!target)      // when use is referencing an non-existing element, there will be no Instance tree built
        return false;

    if (target->needsUpdate())
        return true;

    for (SVGElementInstance* instance = target->firstChild(); instance; instance = instance->nextSibling())
        if (shadowTreeContainsChangedNodes(instance))
            return true;

    return false;
}

void SVGUseElement::recalcStyle(StyleChange change)
{
    if (attached() && needsStyleRecalc() && shadowTreeContainsChangedNodes(m_targetElementInstance.get())) {
        buildPendingResource();

        if (m_shadowTreeRootElement)
            m_shadowTreeRootElement->setNeedsStyleRecalc();
    }

    SVGStyledElement::recalcStyle(change);

    // The shadow tree root element is NOT a direct child element of us.
    // So we have to take care it receives style updates, manually.
    if (!m_shadowTreeRootElement || !m_shadowTreeRootElement->attached())
        return;

    // Mimic Element::recalcStyle(). The main difference is that we don't call attach() on the
    // shadow tree root element, but call attachShadowTree() here. Calling attach() will crash
    // as the shadow tree root element has no (direct) parent node. Yes, shadow trees are tricky.
    if (change >= Inherit || m_shadowTreeRootElement->needsStyleRecalc()) {
        RefPtr<RenderStyle> newStyle = document()->styleSelector()->styleForElement(m_shadowTreeRootElement.get());
        StyleChange ch = Node::diff(m_shadowTreeRootElement->renderStyle(), newStyle.get());
        if (ch == Detach) {
            ASSERT(m_shadowTreeRootElement->attached());
            m_shadowTreeRootElement->detach();
            attachShadowTree();

            // attach recalulates the style for all children. No need to do it twice.
            m_shadowTreeRootElement->setNeedsStyleRecalc(NoStyleChange);
            m_shadowTreeRootElement->setChildNeedsStyleRecalc(false);
            return;
        }
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

    text += String::format("SVGElementInstance this=%p, (parentNode=%s, firstChild=%s, correspondingElement=%s (%p), shadowTreeElement=%p, id=%s)\n",
                           targetInstance, parentNodeName.latin1().data(), firstChildNodeName.latin1().data(), elementNodeName.latin1().data(),
                           element, targetInstance->shadowTreeElement(), elementId.latin1().data());

    for (unsigned int i = 0; i < depth; ++i)
        text += "  ";

    HashSet<SVGElementInstance*> elementInstances = element->instancesForElement();
    text += String::format("Corresponding element is associated with %i instance(s):\n", elementInstances.size());

    HashSet<SVGElementInstance*>::iterator end = elementInstances.end();
    for (HashSet<SVGElementInstance*>::iterator it = elementInstances.begin(); it != end; ++it) {
        for (unsigned int i = 0; i < depth; ++i)
            text += "  ";

        text += String::format(" -> SVGElementInstance this=%p, (refCount: %i, shadowTreeElement in document? %i)\n",
                               *it, (*it)->refCount(), (*it)->shadowTreeElement()->inDocument());
    }

    ++depth;

    for (SVGElementInstance* instance = targetInstance->firstChild(); instance; instance = instance->nextSibling())
        dumpInstanceTree(depth, text, instance);

    --depth;
}
#endif

static bool isDisallowedElement(Node* element)
{
#if ENABLE(SVG_FOREIGN_OBJECT)
    // <foreignObject> should never be contained in a <use> tree. Too dangerous side effects possible.
    if (element->hasTagName(SVGNames::foreignObjectTag))
        return true;
#endif
#if ENABLE(SVG_ANIMATION)
    if (SVGSMILElement::isSMILElement(element))
        return true;
#endif

    return false;
}

static bool subtreeContainsDisallowedElement(Node* start)
{
    if (isDisallowedElement(start))
        return true;

    for (Node* cur = start->firstChild(); cur; cur = cur->nextSibling()) {
        if (subtreeContainsDisallowedElement(cur))
            return true;
    }

    return false;
}

void SVGUseElement::buildPendingResource()
{
    String id = SVGURIReference::getTarget(href());
    Element* targetElement = document()->getElementById(id);

    if (!targetElement) {
        // TODO: We want to deregister as pending resource, if our href() changed!
        // TODO: Move to svgAttributeChanged, once we're fixing use & the new dynamic update concept.
        document()->accessSVGExtensions()->addPendingResource(id, this);
        return;
    }

    // Do not build the shadow/instance tree for <use> elements living in a shadow tree.
    // The will be expanded soon anyway - see expandUseElementsInShadowTree().
    Node* parent = parentNode();
    while (parent) {
        if (parent->isShadowNode())
            return;

        parent = parent->parentNode();
    }
 
    SVGElement* target = 0;
    if (targetElement && targetElement->isSVGElement())
        target = static_cast<SVGElement*>(targetElement);

    if (m_targetElementInstance) {
        m_targetElementInstance->forgetWrapper();
        m_targetElementInstance = 0;
    }

    // Do not allow self-referencing.
    // 'target' may be null, if it's a non SVG namespaced element.
    if (!target || target == this) {
        m_shadowTreeRootElement = 0;
        return;
    }

    // Why a seperated instance/shadow tree? SVG demands it:
    // The instance tree is accesable from JavaScript, and has to
    // expose a 1:1 copy of the referenced tree, whereas internally we need
    // to alter the tree for correct "use-on-symbol", "use-on-svg" support.  
 
    // Build instance tree. Create root SVGElementInstance object for the first sub-tree node.
    //
    // Spec: If the 'use' element references a simple graphics element such as a 'rect', then there is only a
    // single SVGElementInstance object, and the correspondingElement attribute on this SVGElementInstance object
    // is the SVGRectElement that corresponds to the referenced 'rect' element.
    m_targetElementInstance = SVGElementInstance::create(this, target);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    bool foundProblem = false;
    buildInstanceTree(target, m_targetElementInstance.get(), foundProblem);

    // SVG specification does not say a word about <use> & cycles. My view on this is: just ignore it!
    // Non-appearing <use> content is easier to debug, then half-appearing content.
    if (foundProblem) {
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
    if (x().value(this) != 0.0 || y().value(this) != 0.0) {
        String transformString = String::format("translate(%f, %f)", x().value(this), y().value(this));
        m_shadowTreeRootElement->setAttribute(SVGNames::transformAttr, transformString);
    }

    // Build shadow tree from instance tree
    // This also handles the special cases: <use> on <symbol>, <use> on <svg>.
    buildShadowTree(target, m_targetElementInstance.get());

#if ENABLE(SVG) && ENABLE(SVG_USE)
    // Expand all <use> elements in the shadow tree.
    // Expand means: replace the actual <use> element by what it references.
    expandUseElementsInShadowTree(m_shadowTreeRootElement.get());

    // Expand all <symbol> elements in the shadow tree.
    // Expand means: replace the actual <symbol> element by the <svg> element.
    expandSymbolElementsInShadowTree(m_shadowTreeRootElement.get());

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

    PassRefPtr<XMLSerializer> serializer = XMLSerializer::create();

    String markup = serializer->serializeToString(m_shadowTreeRootElement.get(), ec);
    ASSERT(ec == 0);

    fprintf(stderr, "Dumping <use> shadow tree markup:\n%s\n", markup.latin1().data());
#endif

    // Transfer event listeners assigned to the referenced element to our shadow tree elements.
    transferEventListenersToShadowTree(m_targetElementInstance.get());

    // The DOM side is setup properly. Now we have to attach the root shadow
    // tree element manually - using attach() won't work for "shadow nodes".
    attachShadowTree();
}

RenderObject* SVGUseElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGTransformableContainer(this);
}

void SVGUseElement::attach()
{
    SVGStyledTransformableElement::attach();

    // If we're a pending resource, this doesn't have any effect.
    attachShadowTree();
}

void SVGUseElement::detach()
{
    SVGStyledTransformableElement::detach();

    if (m_shadowTreeRootElement)
        m_shadowTreeRootElement->detach();
}

static bool isDirectReference(Node* n)
{
    return n->hasTagName(SVGNames::pathTag) ||
           n->hasTagName(SVGNames::rectTag) ||
           n->hasTagName(SVGNames::circleTag) ||
           n->hasTagName(SVGNames::ellipseTag) ||
           n->hasTagName(SVGNames::polygonTag) ||
           n->hasTagName(SVGNames::polylineTag) ||
           n->hasTagName(SVGNames::textTag);
}

Path SVGUseElement::toClipPath() const
{
    if (!m_shadowTreeRootElement)
        const_cast<SVGUseElement*>(this)->buildPendingResource();

    Node* n = m_shadowTreeRootElement->firstChild();
    if (n->isSVGElement() && static_cast<SVGElement*>(n)->isStyledTransformable()) {
        if (!isDirectReference(n))
            // Spec: Indirect references are an error (14.3.5)
            document()->accessSVGExtensions()->reportError("Not allowed to use indirect reference in <clip-path>");
        else
            return static_cast<SVGStyledTransformableElement*>(n)->toClipPath();
    }

    return Path();
}

void SVGUseElement::buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundProblem)
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
        SVGElement* element = 0;
        if (node->isSVGElement())
            element = static_cast<SVGElement*>(node);

        // Skip any non-svg nodes or any disallowed element.
        if (!element || isDisallowedElement(element))
            continue;

        // Create SVGElementInstance object, for both container/non-container nodes.
        RefPtr<SVGElementInstance> instancePtr = SVGElementInstance::create(this, element);
        targetInstance->appendChild(instancePtr.get());

        // Enter recursion, appending new instance tree nodes to the "instance" object.
        if (element->hasChildNodes())
            buildInstanceTree(element, instancePtr.get(), foundProblem);

        // Spec: If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
        // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
        if (element->hasTagName(SVGNames::useTag))
            handleDeepUseReferencing(static_cast<SVGUseElement*>(element), instancePtr.get(), foundProblem);
    }

    // Spec: If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
    // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
    if (target->hasTagName(SVGNames::useTag))
        handleDeepUseReferencing(static_cast<SVGUseElement*>(target), targetInstance, foundProblem);
}

void SVGUseElement::handleDeepUseReferencing(SVGUseElement* use, SVGElementInstance* targetInstance, bool& foundProblem)
{
    String id = SVGURIReference::getTarget(use->href());
    Element* targetElement = document()->getElementById(id); 
    SVGElement* target = 0;
    if (targetElement && targetElement->isSVGElement())
        target = static_cast<SVGElement*>(targetElement);

    if (!target)
        return;

    // Cycle detection first!
    foundProblem = (target == this);

    // Shortcut for self-references
    if (foundProblem)
        return;

    SVGElementInstance* instance = targetInstance->parentNode();
    while (instance) {
        SVGElement* element = instance->correspondingElement();

        if (element->getIDAttribute() == id) {
            foundProblem = true;
            return;
        }
    
        instance = instance->parentNode();
    }

    // Create an instance object, even if we're dealing with a cycle
    RefPtr<SVGElementInstance> newInstance = SVGElementInstance::create(this, target);
    targetInstance->appendChild(newInstance);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    buildInstanceTree(target, newInstance.get(), foundProblem);
}

void SVGUseElement::alterShadowTreeForSVGTag(SVGElement* target)
{
    String widthString = String::number(width().value(this));
    String heightString = String::number(height().value(this));

    if (hasAttribute(SVGNames::widthAttr))
        target->setAttribute(SVGNames::widthAttr, widthString);

    if (hasAttribute(SVGNames::heightAttr))
        target->setAttribute(SVGNames::heightAttr, heightString);
}

void SVGUseElement::removeDisallowedElementsFromSubtree(Node* subtree)
{
    ASSERT(!subtree->inDocument());
    ExceptionCode ec;
    Node* node = subtree->firstChild();
    while (node) {
        if (isDisallowedElement(node)) {
            Node* next = node->traverseNextSibling(subtree);
            // The subtree is not in document so this won't generate events that could mutate the tree.
            node->parent()->removeChild(node, ec);
            node = next;
        } else
            node = node->traverseNextNode(subtree);
    }
}

void SVGUseElement::buildShadowTree(SVGElement* target, SVGElementInstance* targetInstance)
{
    // For instance <use> on <foreignObject> (direct case).
    if (isDisallowedElement(target))
        return;

    RefPtr<Element> newChild = targetInstance->correspondingElement()->cloneElementWithChildren();

    // We don't walk the target tree element-by-element, and clone each element,
    // but instead use cloneElementWithChildren(). This is an optimization for the common
    // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
    // Though if there are disallowed elements in the subtree, we have to remove them.
    // For instance: <use> on <g> containing <foreignObject> (indirect case).
    if (subtreeContainsDisallowedElement(newChild.get()))
        removeDisallowedElementsFromSubtree(newChild.get());

    SVGElement* newChildPtr = 0;
    if (newChild->isSVGElement())
        newChildPtr = static_cast<SVGElement*>(newChild.get());
    ASSERT(newChildPtr);

    ExceptionCode ec = 0;
    m_shadowTreeRootElement->appendChild(newChild.release(), ec);
    ASSERT(ec == 0);

    // Handle use referencing <svg> special case
    if (target->hasTagName(SVGNames::svgTag))
        alterShadowTreeForSVGTag(newChildPtr);
}

#if ENABLE(SVG) && ENABLE(SVG_USE)
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
        Element* targetElement = document()->getElementById(id); 
        SVGElement* target = 0;
        if (targetElement && targetElement->isSVGElement())
            target = static_cast<SVGElement*>(targetElement);

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
            if (use->x().value(this) != 0.0 || use->y().value(this) != 0.0) {
                if (!cloneParent->hasAttribute(SVGNames::transformAttr)) {
                    String transformString = String::format("translate(%f, %f)", use->x().value(this), use->y().value(this));
                    cloneParent->setAttribute(SVGNames::transformAttr, transformString);
                } else {
                    String transformString = String::format(" translate(%f, %f)", use->x().value(this), use->y().value(this));
                    const AtomicString& transformAttribute = cloneParent->getAttribute(SVGNames::transformAttr);
                    cloneParent->setAttribute(SVGNames::transformAttr, transformAttribute + transformString); 
                }
            }

            ExceptionCode ec = 0;
 
            // For instance <use> on <foreignObject> (direct case).
            if (isDisallowedElement(target)) {
                // We still have to setup the <use> replacment (<g>). Otherwhise
                // associateInstancesWithShadowTreeElements() makes wrong assumptions.
                // Replace <use> with referenced content.
                ASSERT(use->parentNode()); 
                use->parentNode()->replaceChild(cloneParent.release(), use, ec);
                ASSERT(ec == 0);
                return;
            }

            RefPtr<Element> newChild = target->cloneElementWithChildren();

            // We don't walk the target tree element-by-element, and clone each element,
            // but instead use cloneElementWithChildren(). This is an optimization for the common
            // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
            // Though if there are disallowed elements in the subtree, we have to remove them.
            // For instance: <use> on <g> containing <foreignObject> (indirect case).
            if (subtreeContainsDisallowedElement(newChild.get()))
                removeDisallowedElementsFromSubtree(newChild.get());

            SVGElement* newChildPtr = 0;
            if (newChild->isSVGElement())
                newChildPtr = static_cast<SVGElement*>(newChild.get());
            ASSERT(newChildPtr);

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

void SVGUseElement::expandSymbolElementsInShadowTree(Node* element)
{
    if (element->hasTagName(SVGNames::symbolTag)) {
        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.
        RefPtr<SVGSVGElement> svgElement = new SVGSVGElement(SVGNames::svgTag, document());

        // Transfer all attributes from <symbol> to the new <svg> element
        svgElement->attributes()->setAttributes(*element->attributes());

        // Explicitly re-set width/height values
        String widthString = String::number(width().value(this));
        String heightString = String::number(height().value(this)); 

        svgElement->setAttribute(SVGNames::widthAttr, hasAttribute(SVGNames::widthAttr) ? widthString : "100%");
        svgElement->setAttribute(SVGNames::heightAttr, hasAttribute(SVGNames::heightAttr) ? heightString : "100%");

        ExceptionCode ec = 0;

        // Only clone symbol children, and add them to the new <svg> element    
        for (Node* child = element->firstChild(); child; child = child->nextSibling()) {
            RefPtr<Node> newChild = child->cloneNode(true);
            svgElement->appendChild(newChild.release(), ec);
            ASSERT(ec == 0);
        }
    
        // We don't walk the target tree element-by-element, and clone each element,
        // but instead use cloneNode(deep=true). This is an optimization for the common
        // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
        // Though if there are disallowed elements in the subtree, we have to remove them.
        // For instance: <use> on <g> containing <foreignObject> (indirect case).
        if (subtreeContainsDisallowedElement(svgElement.get()))
            removeDisallowedElementsFromSubtree(svgElement.get());

        // Replace <symbol> with <svg>.
        ASSERT(element->parentNode()); 
        element->parentNode()->replaceChild(svgElement.release(), element, ec);
        ASSERT(ec == 0);

        // Immediately stop here, and restart expanding.
        expandSymbolElementsInShadowTree(m_shadowTreeRootElement.get());
        return;
    }

    for (RefPtr<Node> child = element->firstChild(); child; child = child->nextSibling())
        expandSymbolElementsInShadowTree(child.get());
}

#endif
    
void SVGUseElement::attachShadowTree()
{
    if (!m_shadowTreeRootElement || m_shadowTreeRootElement->attached() || !document()->shouldCreateRenderers() || !attached() || !renderer())
        return;

    // Inspired by RenderTextControl::createSubtreeIfNeeded(). 
    if (renderer()->canHaveChildren() && childShouldCreateRenderer(m_shadowTreeRootElement.get())) {
        RefPtr<RenderStyle> style = m_shadowTreeRootElement->styleForRenderer();

        if (m_shadowTreeRootElement->rendererIsNeeded(style.get())) {
            m_shadowTreeRootElement->setRenderer(m_shadowTreeRootElement->createRenderer(document()->renderArena(), style.get()));
            if (RenderObject* shadowRenderer = m_shadowTreeRootElement->renderer()) {
                shadowRenderer->setStyle(style.release());
                renderer()->addChild(shadowRenderer, m_shadowTreeRootElement->nextRenderer());
                m_shadowTreeRootElement->setAttached();
            }
        }

        // This will take care of attaching all shadow tree child nodes.
        for (Node* child = m_shadowTreeRootElement->firstChild(); child; child = child->nextSibling())
            child->attach();
    }
}
 
void SVGUseElement::transferEventListenersToShadowTree(SVGElementInstance* target)
{
    if (!target)
        return;

    SVGElement* originalElement = target->correspondingElement();
    ASSERT(originalElement);

    if (SVGElement* shadowTreeElement = target->shadowTreeElement()) {
        if (EventTargetData* d = originalElement->eventTargetData()) {
            EventListenerMap& map = d->eventListenerMap;
            EventListenerMap::iterator end = map.end();
            for (EventListenerMap::iterator it = map.begin(); it != end; ++it) {
                EventListenerVector& entry = it->second;
                for (size_t i = 0; i < entry.size(); ++i) {
                    // Event listeners created from markup have already been transfered to the shadow tree during cloning.
                    if (entry[i].listener->wasCreatedFromMarkup())
                        continue;
                    shadowTreeElement->addEventListener(it->first, entry[i].listener, entry[i].useCapture);
                }
            }
        }
    }

    for (SVGElementInstance* instance = target->firstChild(); instance; instance = instance->nextSibling())
        transferEventListenersToShadowTree(instance);
}

void SVGUseElement::associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance)
{
    if (!target || !targetInstance)
        return;

    SVGElement* originalElement = targetInstance->correspondingElement();

    if (originalElement->hasTagName(SVGNames::useTag)) {
#if ENABLE(SVG) && ENABLE(SVG_USE)
        // <use> gets replaced by <g>
        ASSERT(target->nodeName() == SVGNames::gTag);
#else 
        ASSERT(target->nodeName() == SVGNames::gTag || target->nodeName() == SVGNames::useTag);
#endif
    } else if (originalElement->hasTagName(SVGNames::symbolTag)) {
        // <symbol> gets replaced by <svg>
#if ENABLE(SVG) && ENABLE(SVG_USE) && ENABLE(SVG_FOREIGN_OBJECT)
        ASSERT(target->nodeName() == SVGNames::svgTag);
#endif
    } else
        ASSERT(target->nodeName() == originalElement->nodeName());

    SVGElement* element = 0;
    if (target->isSVGElement())
        element = static_cast<SVGElement*>(target);

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

    // We're dispatching a mutation event during shadow tree construction
    // this instance hasn't yet been associated to a shadowTree element.
    if (!instance->shadowTreeElement())
        return 0;

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

    to->attributes()->setAttributes(*from->attributes());

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
