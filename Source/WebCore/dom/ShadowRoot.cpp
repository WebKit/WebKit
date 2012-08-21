/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "ShadowRoot.h"

#include "ContentDistributor.h"
#include "DOMSelection.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "ElementShadow.h"
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "InsertionPoint.h"
#include "NodeRareData.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "StyleResolver.h"
#include "markup.h"

namespace WebCore {

ShadowRoot::ShadowRoot(Document* document)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(this)
    , m_prev(0)
    , m_next(0)
    , m_applyAuthorStyles(false)
    , m_resetStyleInheritance(false)
    , m_insertionPointAssignedTo(0)
{
    ASSERT(document);
    
    // Assume document as parent scope.
    setParentTreeScope(document);
    // Shadow tree scopes have the scope pointer point to themselves.
    // This way, direct children will receive the correct scope pointer.
    ensureRareData()->setTreeScope(this);
}

ShadowRoot::~ShadowRoot()
{
    ASSERT(!m_prev);
    ASSERT(!m_next);

    // We must call clearRareData() here since a ShadowRoot class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();
}

static bool allowsAuthorShadowRoot(Element* element)
{
#if ENABLE(SHADOW_DOM)
    if (RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
        return true;
#endif

    // FIXME: The elements in Shadow DOM of an input element assumes that they have renderer if the input
    // element has a renderer. However, this does not hold until input elemnet is AuthorShadowDOM-ready.
    // So we would like to prohibit having a AuthorShadowDOM for a while. The same thing happens to
    // textarea element also.
    // https://bugs.webkit.org/show_bug.cgi?id=92608
    if (isHTMLInputElement(element) || isHTMLTextAreaElement(element))
        return false;

    // FIXME: We disable multiple shadow subtrees for SVG for while, because there will be problems to support it.
    // https://bugs.webkit.org/show_bug.cgi?id=78205
    // Especially SVG TREF recreates shadow root dynamically.
    // https://bugs.webkit.org/show_bug.cgi?id=77938
    if (element->isSVGElement())
        return false;

    return true;
}

PassRefPtr<ShadowRoot> ShadowRoot::create(Element* element, ExceptionCode& ec)
{
    return create(element, AuthorShadowRoot, ec);
}

PassRefPtr<ShadowRoot> ShadowRoot::create(Element* element, ShadowRootType type, ExceptionCode& ec)
{
    if (!element) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }

    // Since some elements recreates shadow root dynamically, multiple shadow subtrees won't work well in that element.
    // Until they are fixed, we disable adding author shadow root for them.
    if (type == AuthorShadowRoot && !allowsAuthorShadowRoot(element)) {
        ec = HIERARCHY_REQUEST_ERR;
        return 0;
    }

    RefPtr<ShadowRoot> shadowRoot = adoptRef(new ShadowRoot(element->document()));
#ifndef NDEBUG
    shadowRoot->m_type = type;
#endif

    ec = 0;
    element->ensureShadow()->addShadowRoot(element, shadowRoot, type, ec);
    if (ec)
        return 0;
    ASSERT(element == shadowRoot->host());
    ASSERT(element->shadow());
    return shadowRoot.release();
}

String ShadowRoot::nodeName() const
{
    return "#shadow-root";
}

PassRefPtr<Node> ShadowRoot::cloneNode(bool, ExceptionCode& ec)
{
    // ShadowRoot should not be arbitrarily cloned.
    ec = DATA_CLONE_ERR;
    return 0;
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionCode& ec)
{
    if (RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, host(), AllowScriptingContent, ec))
        replaceChildrenWithFragment(this, fragment.release(), ec);
}

bool ShadowRoot::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case ENTITY_REFERENCE_NODE:
        return true;
    default:
        return false;
    }
}

ElementShadow* ShadowRoot::owner() const
{
    if (host())
        return host()->shadow();
    return 0;
}

bool ShadowRoot::hasInsertionPoint() const
{
    for (Node* n = firstChild(); n; n = n->traverseNextNode(this)) {
        if (isInsertionPoint(n))
            return true;
    }

    return false;
}

bool ShadowRoot::applyAuthorStyles() const
{
    return m_applyAuthorStyles;
}

void ShadowRoot::setApplyAuthorStyles(bool value)
{
    if (m_applyAuthorStyles != value) {
        m_applyAuthorStyles = value;
        host()->setNeedsStyleRecalc();
    }
}

bool ShadowRoot::resetStyleInheritance() const
{
    return m_resetStyleInheritance;
}

void ShadowRoot::setResetStyleInheritance(bool value)
{
    if (value != m_resetStyleInheritance) {
        m_resetStyleInheritance = value;
        if (attached() && owner())
            owner()->recalcStyle(Force);
    }
}

void ShadowRoot::attach()
{
    StyleResolver* styleResolver = document()->styleResolver();
    styleResolver->pushParentShadowRoot(this);
    attachChildrenIfNeeded();
    attachAsNode();
    styleResolver->popParentShadowRoot(this);
}

void ShadowRoot::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    owner()->invalidateDistribution();
}

}
