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
#include "HistogramSupport.h"
#include "InsertionPoint.h"
#include "NodeRareData.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "StyleResolver.h"
#include "Text.h"
#include "markup.h"

// FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
#define GuardOrphanShadowRoot(rejectStatement) \
    if (!this->host()) {                       \
        rejectStatement;                       \
        return;                                \
    }

namespace WebCore {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    void* pointers[3];
    unsigned countersAndFlags[1];
};

COMPILE_ASSERT(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), shadowroot_should_stay_small);

ShadowRoot::ShadowRoot(Document* document)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(this)
    , m_prev(0)
    , m_next(0)
    , m_numberOfStyles(0)
    , m_applyAuthorStyles(false)
    , m_resetStyleInheritance(false)
    , m_isAuthorShadowRoot(false)
    , m_registeredWithParentShadowRoot(false)
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

    // We must remove all of our children first before the TreeScope destructor
    // runs so we don't go through TreeScopeAdopter for each child with a
    // destructed tree scope in each descendant.
    removeAllChildren();

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
    return element->areAuthorShadowsAllowed();
}

enum ShadowRootUsageOriginType {
    ShadowRootUsageOriginWeb = 0,
    ShadowRootUsageOriginNotWeb,
    ShadowRootUsageOriginTypes
};

static inline ShadowRootUsageOriginType determineUsageType(Element* host)
{
    // Enables only on CHROMIUM since this cost won't worth paying for platforms which don't collect this metrics.
#if PLATFORM(CHROMIUM)
    if (!host)
        return ShadowRootUsageOriginWeb;
    return host->document()->url().string().startsWith("http") ? ShadowRootUsageOriginWeb : ShadowRootUsageOriginNotWeb;
#else
    UNUSED_PARAM(host);
    return ShadowRootUsageOriginWeb;
#endif
}

PassRefPtr<ShadowRoot> ShadowRoot::create(Element* element, ExceptionCode& ec)
{
    HistogramSupport::histogramEnumeration("WebCore.ShadowRoot.constructor", determineUsageType(element), ShadowRootUsageOriginTypes);
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
    shadowRoot->setType(type);

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

PassRefPtr<Node> ShadowRoot::cloneNode(bool)
{
    // ShadowRoot should not be arbitrarily cloned.
    return 0;
}

PassRefPtr<Node> ShadowRoot::cloneNode(bool, ExceptionCode& ec)
{
    ec = DATA_CLONE_ERR;
    return 0;
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionCode& ec)
{
    GuardOrphanShadowRoot(ec = INVALID_ACCESS_ERR);

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

void ShadowRoot::recalcStyle(StyleChange change)
{
    // ShadowRoot doesn't support custom callbacks.
    ASSERT(!hasCustomCallbacks());

    StyleResolver* styleResolver = document()->styleResolver();
    styleResolver->pushParentShadowRoot(this);

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isElementNode())
            static_cast<Element*>(child)->recalcStyle(change);
        else if (child->isTextNode())
            toText(child)->recalcTextStyle(change);
    }

    styleResolver->popParentShadowRoot(this);
    clearNeedsStyleRecalc();
    clearChildNeedsStyleRecalc();
}

ElementShadow* ShadowRoot::owner() const
{
    if (host())
        return host()->shadow();
    return 0;
}

bool ShadowRoot::hasInsertionPoint() const
{
    return hasShadowInsertionPoint() || hasContentElement();
}

bool ShadowRoot::applyAuthorStyles() const
{
    return m_applyAuthorStyles;
}

void ShadowRoot::setApplyAuthorStyles(bool value)
{
    GuardOrphanShadowRoot({ });

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
    GuardOrphanShadowRoot({ });

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
    DocumentFragment::attach();
    styleResolver->popParentShadowRoot(this);
}

Node::InsertionNotificationRequest ShadowRoot::insertedInto(ContainerNode* insertionPoint)
{
    DocumentFragment::insertedInto(insertionPoint);

    if (!insertionPoint->inDocument() || !isOldest())
        return InsertionDone;

    // FIXME: When parsing <video controls>, insertedInto() is called many times without invoking removedFrom.
    // For now, we check m_registeredWithParentShadowroot. We would like to ASSERT(!m_registeredShadowRoot) here.
    // https://bugs.webkit.org/show_bug.cig?id=101316
    if (m_registeredWithParentShadowRoot)
        return InsertionDone;

    if (ShadowRoot* root = host()->containingShadowRoot()) {
        root->registerElementShadow();
        m_registeredWithParentShadowRoot = true;
    }

    return InsertionDone;
}

void ShadowRoot::removedFrom(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument() && m_registeredWithParentShadowRoot) {
        ShadowRoot* root = host()->containingShadowRoot();
        if (!root)
            root = insertionPoint->containingShadowRoot();

        if (root)
            root->unregisterElementShadow();
        m_registeredWithParentShadowRoot = false;
    }

    DocumentFragment::removedFrom(insertionPoint);
}

InsertionPoint* ShadowRoot::assignedTo() const
{
    if (!distributionData())
        return 0;

    return distributionData()->insertionPointAssignedTo();
}

void ShadowRoot::setAssignedTo(InsertionPoint* insertionPoint)
{
    ASSERT(!assignedTo() || !insertionPoint);
    ensureDistributionData()->setInsertionPointAssignedTo(insertionPoint);
}

void ShadowRoot::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    GuardOrphanShadowRoot({ });

    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    owner()->invalidateDistribution();
}

const Vector<RefPtr<InsertionPoint> >& ShadowRoot::insertionPointList()
{
    typedef Vector<RefPtr<InsertionPoint> > InsertionPointVector;
    DEFINE_STATIC_LOCAL(InsertionPointVector, emptyVector, ());

    return distributionData() ? distributionData()->ensureInsertionPointList(this) : emptyVector;
}

void ShadowRoot::registerScopedHTMLStyleChild()
{
    ++m_numberOfStyles;
    setHasScopedHTMLStyleChild(true);
}

void ShadowRoot::unregisterScopedHTMLStyleChild()
{
    ASSERT(hasScopedHTMLStyleChild() && m_numberOfStyles > 0);
    --m_numberOfStyles;
    setHasScopedHTMLStyleChild(m_numberOfStyles > 0);
}

inline ShadowRootContentDistributionData* ShadowRoot::ensureDistributionData()
{
    if (m_distributionData)
        return m_distributionData.get();

    m_distributionData = adoptPtr(new ShadowRootContentDistributionData);
    return m_distributionData.get();
}   

void ShadowRoot::registerInsertionPoint(InsertionPoint* point)
{
    ensureDistributionData()->regiterInsertionPoint(this, point);
}

void ShadowRoot::unregisterInsertionPoint(InsertionPoint* point)
{
    ensureDistributionData()->unregisterInsertionPoint(this, point);
}

bool ShadowRoot::hasShadowInsertionPoint() const
{
    if (!distributionData())
        return false;

    return distributionData()->hasShadowElementChildren();
}

bool ShadowRoot::hasContentElement() const
{
    if (!distributionData())
        return false;

    return distributionData()->hasContentElementChildren();
}

void ShadowRoot::registerElementShadow()
{
    ensureDistributionData()->incrementNumberOfElementShadowChildren();
}

void ShadowRoot::unregisterElementShadow()
{
    ASSERT(hasElementShadow());
    distributionData()->decrementNumberOfElementShadowChildren();
}

bool ShadowRoot::hasElementShadow() const
{
    if (!distributionData())
        return false;

    return distributionData()->hasElementShadowChildren();
}

unsigned ShadowRoot::countElementShadow() const 
{
    if (!distributionData())
        return 0;

    return distributionData()->numberOfElementShadowChildren();
}

void ShadowRoot::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    DocumentFragment::reportMemoryUsage(memoryObjectInfo);
    TreeScope::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_prev);
    info.addMember(m_next);
    info.addMember(m_distributionData);
}

}
