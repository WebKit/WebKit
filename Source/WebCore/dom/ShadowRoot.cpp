/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "CSSStyleSheet.h"
#include "ChildListMutationScope.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "HTMLParserIdioms.h"
#include "HTMLSlotElement.h"
#if ENABLE(PICTURE_IN_PICTURE_API)
#include "NotImplemented.h"
#endif
#include "RenderElement.h"
#include "SlotAssignment.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleSheetList.h"
#include "WebAnimation.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ShadowRoot);

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope {
    bool flags[4];
    uint8_t mode;
    void* styleScope;
    void* styleSheetList;
    WeakPtr<Element, WeakPtrImplWithEventTargetData> host;
    void* slotAssignment;
    std::optional<HashMap<AtomString, AtomString>> partMappings;
};

static_assert(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), "shadowroot should stay small");
#if !ASSERT_ENABLED
static_assert(sizeof(WeakPtr<Element, WeakPtrImplWithEventTargetData>) == sizeof(void*), "WeakPtr should be same size as raw pointer");
#endif

ShadowRoot::ShadowRoot(Document& document, ShadowRootMode type, SlotAssignmentMode assignmentMode, DelegatesFocus delegatesFocus, AvailableToElementInternals availableToElementInternals)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_delegatesFocus(delegatesFocus == DelegatesFocus::Yes)
    , m_availableToElementInternals(availableToElementInternals == AvailableToElementInternals::Yes)
    , m_type(type)
    , m_slotAssignmentMode(assignmentMode)
    , m_styleScope(makeUnique<Style::Scope>(*this))
{
    if (type == ShadowRootMode::UserAgent)
        setNodeFlag(NodeFlag::HasBeenInUserAgentShadowTree);
}


ShadowRoot::ShadowRoot(Document& document, std::unique_ptr<SlotAssignment>&& slotAssignment)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_type(ShadowRootMode::UserAgent)
    , m_styleScope(makeUnique<Style::Scope>(*this))
    , m_slotAssignment(WTFMove(slotAssignment))
{
    setNodeFlag(NodeFlag::HasBeenInUserAgentShadowTree);
}


ShadowRoot::~ShadowRoot()
{
    if (isConnected())
        document().didRemoveInDocumentShadowRoot(*this);

    if (m_styleSheetList)
        m_styleSheetList->detach();

    // We cannot let ContainerNode destructor call willBeDeletedFrom()
    // for this ShadowRoot instance because TreeScope destructor
    // clears Node::m_treeScope thus ContainerNode is no longer able
    // to access it Document reference after that.
    willBeDeletedFrom(document());

    ASSERT(!m_hasBegunDeletingDetachedChildren);
    m_hasBegunDeletingDetachedChildren = true;

    // We must remove all of our children first before the TreeScope destructor
    // runs so we don't go through Node::setTreeScopeRecursively for each child with a
    // destructed tree scope in each descendant.
    removeDetachedChildren();
}

Node::InsertedIntoAncestorResult ShadowRoot::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    DocumentFragment::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        document().didInsertInDocumentShadowRoot(*this);
    if (!adoptedStyleSheets().isEmpty() && document().frame())
        styleScope().didChangeActiveStyleSheetCandidates();
    return InsertedIntoAncestorResult::Done;
}

void ShadowRoot::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    DocumentFragment::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        document().didRemoveInDocumentShadowRoot(*this);
}

void ShadowRoot::childrenChanged(const ChildChange& childChange)
{
    DocumentFragment::childrenChanged(childChange);

    if (!m_host || m_type == ShadowRootMode::UserAgent)
        return; // Don't support first-child, nth-of-type, etc... in UA shadow roots as an optimization.

    // FIXME: Avoid always invalidating style just for first-child, etc... as done in Element::childrenChanged.
    switch (childChange.type) {
    case ChildChange::Type::ElementInserted:
    case ChildChange::Type::ElementRemoved:
        m_host->invalidateStyleForSubtreeInternal();
        break;
    case ChildChange::Type::TextInserted:
    case ChildChange::Type::TextRemoved:
    case ChildChange::Type::TextChanged:
    case ChildChange::Type::AllChildrenRemoved:
    case ChildChange::Type::NonContentsChildRemoved:
    case ChildChange::Type::NonContentsChildInserted:
    case ChildChange::Type::AllChildrenReplaced:
        break;
    }
}

void ShadowRoot::moveShadowRootToNewParentScope(TreeScope& newScope, Document& newDocument)
{
    auto& oldDocument = documentScope();
    setParentTreeScope(newScope);
    moveShadowRootToNewDocument(oldDocument, newDocument);
}

void ShadowRoot::moveShadowRootToNewDocument(Document& oldDocument, Document& newDocument)
{
    if (oldDocument.templateDocumentHost() != &newDocument && newDocument.templateDocumentHost() != &oldDocument)
        setAdoptedStyleSheets({ });

    setDocumentScope(newDocument);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!parentTreeScope() || &parentTreeScope()->documentScope() == &newDocument);

    // Style scopes are document specific.
    m_styleScope = makeUnique<Style::Scope>(*this);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&m_styleScope->document() == &newDocument);
}

Style::Scope& ShadowRoot::styleScope()
{
    return *m_styleScope;
}

StyleSheetList& ShadowRoot::styleSheets()
{
    if (!m_styleSheetList)
        m_styleSheetList = StyleSheetList::create(*this);
    return *m_styleSheetList;
}

String ShadowRoot::innerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreesOfChildren, nullptr, ResolveURLs::NoExcludingURLsForPrivacy);
}

ExceptionOr<void> ShadowRoot::setInnerHTML(const String& markup)
{
    if (markup.isEmpty()) {
        ChildListMutationScope mutation(*this);
        removeChildren();
        return { };
    }

    auto fragment = createFragmentForInnerOuterHTML(*host(), markup, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    if (fragment.hasException())
        return fragment.releaseException();
    return replaceChildrenWithFragment(*this, fragment.releaseReturnValue());
}

bool ShadowRoot::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        return false;
    }
}

void ShadowRoot::setResetStyleInheritance(bool value)
{
    // If this was ever changed after initialization, child styles would need to be invalidated here.
    m_resetStyleInheritance = value;
}

Ref<Node> ShadowRoot::cloneNodeInternal(Document&, CloningOperation)
{
    RELEASE_ASSERT_NOT_REACHED();
    return *static_cast<Node*>(nullptr); // ShadowRoots should never be cloned.
}

void ShadowRoot::removeAllEventListeners()
{
    DocumentFragment::removeAllEventListeners();
    for (Node* node = firstChild(); node; node = NodeTraversal::next(*node))
        node->removeAllEventListeners();
}


HTMLSlotElement* ShadowRoot::findAssignedSlot(const Node& node)
{
    ASSERT(node.parentNode() == host());
    if (!m_slotAssignment)
        return nullptr;
    return m_slotAssignment->findAssignedSlot(node);
}

void ShadowRoot::renameSlotElement(HTMLSlotElement& slot, const AtomString& oldName, const AtomString& newName)
{
    ASSERT(m_slotAssignment);
    return m_slotAssignment->renameSlotElement(slot, oldName, newName, *this);
}

void ShadowRoot::addSlotElementByName(const AtomString& name, HTMLSlotElement& slot)
{
    ASSERT(&slot.rootNode() == this);
    if (!m_slotAssignment) {
        if (m_slotAssignmentMode == SlotAssignmentMode::Named)
            m_slotAssignment = makeUnique<NamedSlotAssignment>();
        else
            m_slotAssignment = makeUnique<ManualSlotAssignment>();
    }

    return m_slotAssignment->addSlotElementByName(name, slot, *this);
}

void ShadowRoot::removeSlotElementByName(const AtomString& name, HTMLSlotElement& slot, ContainerNode& oldParentOfRemovedTree)
{
    ASSERT(m_slotAssignment);
    return m_slotAssignment->removeSlotElementByName(name, slot, &oldParentOfRemovedTree, *this);
}

void ShadowRoot::slotManualAssignmentDidChange(HTMLSlotElement& slot, Vector<WeakPtr<Node, WeakPtrImplWithEventTargetData>>& previous, Vector<WeakPtr<Node, WeakPtrImplWithEventTargetData>>& current)
{
    ASSERT(m_slotAssignment);
    m_slotAssignment->slotManualAssignmentDidChange(slot, previous, current, *this);
}

void ShadowRoot::didRemoveManuallyAssignedNode(HTMLSlotElement& slot, const Node& node)
{
    ASSERT(m_slotAssignment);
    m_slotAssignment->didRemoveManuallyAssignedNode(slot, node, *this);
}

void ShadowRoot::slotFallbackDidChange(HTMLSlotElement& slot)
{
    ASSERT(&slot.rootNode() == this);
    return m_slotAssignment->slotFallbackDidChange(slot, *this);
}

const Vector<WeakPtr<Node, WeakPtrImplWithEventTargetData>>* ShadowRoot::assignedNodesForSlot(const HTMLSlotElement& slot)
{
    if (!m_slotAssignment)
        return nullptr;
    return m_slotAssignment->assignedNodesForSlot(slot, *this);
}

static std::optional<std::pair<AtomString, AtomString>> parsePartMapping(StringView mappingString)
{
    const auto end = mappingString.length();

    auto skipWhitespace = [&](auto position) {
        while (position < end && isHTMLSpace(mappingString[position]))
            ++position;
        return position;
    };

    auto collectValue = [&](auto position) {
        while (position < end && (!isHTMLSpace(mappingString[position]) && mappingString[position] != ':'))
            ++position;
        return position;
    };

    size_t begin = 0;
    begin = skipWhitespace(begin);

    auto firstPartEnd = collectValue(begin);
    if (firstPartEnd == begin)
        return { };

    auto firstPart = mappingString.substring(begin, firstPartEnd - begin).toAtomString();

    begin = skipWhitespace(firstPartEnd);
    if (begin == end)
        return std::make_pair(firstPart, firstPart);

    if (mappingString[begin] != ':')
        return { };

    begin = skipWhitespace(begin + 1);

    auto secondPartEnd = collectValue(begin);
    if (secondPartEnd == begin)
        return { };

    auto secondPart = mappingString.substring(begin, secondPartEnd - begin).toAtomString();

    begin = skipWhitespace(secondPartEnd);
    if (begin != end)
        return { };

    return std::make_pair(firstPart, secondPart);
}

static ShadowRoot::PartMappings parsePartMappingsList(StringView mappingsListString)
{
    ShadowRoot::PartMappings mappings;

    const auto end = mappingsListString.length();

    size_t begin = 0;
    while (begin < end) {
        size_t mappingEnd = begin;
        while (mappingEnd < end && mappingsListString[mappingEnd] != ',')
            ++mappingEnd;

        auto result = parsePartMapping(mappingsListString.substring(begin, mappingEnd - begin));
        if (result)
            mappings.add(result->first, Vector<AtomString, 1>()).iterator->value.append(result->second);

        if (mappingEnd == end)
            break;

        begin = mappingEnd + 1;
    }

    return mappings;
}

const ShadowRoot::PartMappings& ShadowRoot::partMappings() const
{
    if (!m_partMappings) {
        auto exportpartsValue = host()->attributeWithoutSynchronization(HTMLNames::exportpartsAttr);
        m_partMappings = parsePartMappingsList(exportpartsValue);
    }

    return *m_partMappings;
}

void ShadowRoot::invalidatePartMappings()
{
    m_partMappings = { };
}

Vector<ShadowRoot*> assignedShadowRootsIfSlotted(const Node& node)
{
    Vector<ShadowRoot*> result;
    for (auto* slot = node.assignedSlot(); slot; slot = slot->assignedSlot()) {
        ASSERT(slot->containingShadowRoot());
        result.append(slot->containingShadowRoot());
    }
    return result;
}

#if ENABLE(PICTURE_IN_PICTURE_API)
HTMLVideoElement* ShadowRoot::pictureInPictureElement() const
{
    notImplemented();
    return nullptr;
}
#endif

Vector<RefPtr<WebAnimation>> ShadowRoot::getAnimations()
{
    return document().matchingAnimations([&] (Element& target) -> bool {
        return target.containingShadowRoot() == this;
    });
}

}
