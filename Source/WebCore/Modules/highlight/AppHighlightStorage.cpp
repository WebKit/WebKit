/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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


#include "config.h"
#include "AppHighlightStorage.h"

#include "AppHighlight.h"
#include "AppHighlightRangeData.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "HTMLBodyElement.h"
#include "HighlightRegister.h"
#include "Node.h"
#include "Position.h"
#include "RenderedDocumentMarker.h"
#include "SimpleRange.h"
#include "StaticRange.h"
#include "TextIndicator.h"
#include "TextIterator.h"
#include <wtf/UUID.h>

namespace WebCore {

#if ENABLE(APP_HIGHLIGHTS)

static constexpr unsigned textPreviewLength = 500;

static RefPtr<Node> findNodeByPathIndex(const Node& parent, unsigned pathIndex, const String& nodeName)
{
    if (!is<ContainerNode>(parent))
        return nullptr;

    for (auto* child = downcast<ContainerNode>(parent).firstChild(); child; child = child->nextSibling()) {
        if (nodeName != child->nodeName())
            continue;

        if (!pathIndex--)
            return child;
    }
    return nullptr;
}

static std::pair<RefPtr<Node>, size_t> findNodeStartingAtPathComponentIndex(const AppHighlightRangeData::NodePath& path, Node& initialNode, size_t initialIndexToFollow)
{
    if (initialIndexToFollow >= path.size())
        return { nullptr, initialIndexToFollow };

    RefPtr currentNode = &initialNode;
    size_t currentPathIndex = initialIndexToFollow;
    for (; currentPathIndex < path.size(); ++currentPathIndex) {
        auto& component = path[currentPathIndex];
        auto nextNode = findNodeByPathIndex(*currentNode, component.pathIndex, component.nodeName);
        if (!nextNode)
            return { nullptr, currentPathIndex };

        if (is<CharacterData>(*nextNode) && downcast<CharacterData>(*nextNode).data() != component.textData)
            return { nullptr, currentPathIndex };

        currentNode = WTFMove(nextNode);
    }
    return { currentNode, currentPathIndex };
}

static RefPtr<Node> findNode(const AppHighlightRangeData::NodePath& path, Document& document)
{
    if (path.isEmpty() || !document.body())
        return nullptr;

    auto [foundNode, nextIndex] = findNodeStartingAtPathComponentIndex(path, *document.body(), 0);
    if (foundNode)
        return foundNode;

    while (nextIndex < path.size()) {
        auto& component = path[nextIndex++];
        if (component.identifier.isEmpty())
            continue;

        RefPtr elementWithIdentifier = document.getElementById(component.identifier);
        if (!elementWithIdentifier || elementWithIdentifier->nodeName() != component.nodeName)
            continue;

        std::tie(foundNode, nextIndex) = findNodeStartingAtPathComponentIndex(path, *elementWithIdentifier, nextIndex);
        if (foundNode)
            return foundNode;
    }

    return nullptr;
}

static std::optional<SimpleRange> findRangeByIdentifyingStartAndEndPositions(const AppHighlightRangeData& range, Document& document)
{
    auto startContainer = findNode(range.startContainer(), document);
    if (!startContainer)
        return std::nullopt;

    auto endContainer = findNode(range.endContainer(), document);
    if (!endContainer)
        return std::nullopt;

    auto start = makeContainerOffsetPosition(startContainer.get(), range.startOffset());
    auto end = makeContainerOffsetPosition(endContainer.get(), range.endOffset());
    if (start.isOrphan() || end.isOrphan())
        return std::nullopt;

    return makeSimpleRange(start, end);
}

static std::optional<SimpleRange> findRangeBySearchingText(const AppHighlightRangeData& range, Document& document)
{
    HashSet<String> identifiersInStartPath;
    for (auto& component : range.startContainer()) {
        if (!component.identifier.isEmpty())
            identifiersInStartPath.add(component.identifier);
    }

    RefPtr<Element> foundElement = document.body();
    for (auto iterator = range.endContainer().rbegin(), end = range.endContainer().rend(); iterator != end; ++iterator) {
        auto elementIdentifier = iterator->identifier;
        if (elementIdentifier.isEmpty() || !identifiersInStartPath.contains(elementIdentifier))
            continue;

        foundElement = document.getElementById(elementIdentifier);
        if (foundElement)
            break;
    }

    if (!foundElement)
        return std::nullopt;

    auto foundElementRange = makeRangeSelectingNodeContents(*foundElement);
    auto foundText = plainText(foundElementRange);
    if (auto index = foundText.find(range.text()); index != notFound && index == foundText.reverseFind(range.text()))
        return resolveCharacterRange(foundElementRange, { index, range.text().length() });

    return std::nullopt;
}

static std::optional<SimpleRange> findRange(const AppHighlightRangeData& range, Document& document)
{
    if (auto foundRange = findRangeByIdentifyingStartAndEndPositions(range, document))
        return foundRange;

    return findRangeBySearchingText(range, document);
}

static unsigned computePathIndex(const Node& node)
{
    String nodeName = node.nodeName();
    unsigned index = 0;
    for (auto* previousSibling = node.previousSibling(); previousSibling; previousSibling = previousSibling->previousSibling()) {
        if (previousSibling->nodeName() == nodeName)
            index++;
    }
    return index;
}

static AppHighlightRangeData::NodePathComponent createNodePathComponent(const Node& node)
{
    return {
        is<Element>(node) ? downcast<Element>(node).getIdAttribute().string() : nullString(),
        node.nodeName(),
        is<CharacterData>(node) ? downcast<CharacterData>(node).data() : nullString(),
        computePathIndex(node)
    };
}

static AppHighlightRangeData::NodePath makeNodePath(RefPtr<Node>&& node)
{
    AppHighlightRangeData::NodePath components;
    auto body = node->document().body();
    for (auto ancestor = node; ancestor && ancestor != body; ancestor = ancestor->parentNode())
        components.append(createNodePathComponent(*ancestor));
    components.reverse();
    return { components };
}

static AppHighlightRangeData createAppHighlightRangeData(const StaticRange& range)
{
    auto text = plainText(range);
    text.truncate(textPreviewLength);
    auto identifier = createVersion4UUIDString();

    return {
        identifier,
        text,
        makeNodePath(&range.startContainer()),
        range.startOffset(),
        makeNodePath(&range.endContainer()),
        range.endOffset()
    };
}

AppHighlightStorage::AppHighlightStorage(Document& document)
    : m_document(document)
{
}

AppHighlightStorage::~AppHighlightStorage() = default;

void AppHighlightStorage::storeAppHighlight(Ref<StaticRange>&& range)
{
    auto data = createAppHighlightRangeData(range);
    std::optional<String> text;

    if (!data.text().isEmpty())
        text = data.text();

    AppHighlight highlight = {data.toSharedBuffer(), text, CreateNewGroupForHighlight::No, HighlightRequestOriginatedInApp::No};

    m_document->page()->chrome().storeAppHighlight(WTFMove(highlight));
}

void AppHighlightStorage::restoreAndScrollToAppHighlight(Ref<FragmentedSharedBuffer>&& buffer, ScrollToHighlight scroll)
{
    auto appHighlightRangeData = AppHighlightRangeData::create(buffer);
    if (!appHighlightRangeData)
        return;
    
    if (!attemptToRestoreHighlightAndScroll(appHighlightRangeData.value(), scroll)) {
        if (scroll == ScrollToHighlight::Yes)
            m_unrestoredScrollHighlight = appHighlightRangeData;
        else
            m_unrestoredHighlights.append(appHighlightRangeData.value());
    }
    
    m_timeAtLastRangeSearch = MonotonicTime::now();
}

bool AppHighlightStorage::attemptToRestoreHighlightAndScroll(AppHighlightRangeData& highlight, ScrollToHighlight scroll)
{
    if (!m_document)
        return false;
    
    RefPtr strongDocument = m_document.get();
    
    auto range = findRange(highlight, *strongDocument);
    
    if (!range)
        return false;
    
    strongDocument->appHighlightRegister().addAnnotationHighlightWithRange(StaticRange::create(*range));
    
    if (scroll == ScrollToHighlight::Yes) {
        auto textIndicator = TextIndicator::createWithRange(range.value(), { TextIndicatorOption::DoNotClipToVisibleRect }, WebCore::TextIndicatorPresentationTransition::Bounce);
        if (textIndicator)
            m_document->page()->chrome().client().setTextIndicator(textIndicator->data());

        TemporarySelectionChange selectionChange(*strongDocument, { range.value() }, { TemporarySelectionOption::DelegateMainFrameScroll, TemporarySelectionOption::SmoothScroll, TemporarySelectionOption::RevealSelectionBounds });
    }

    return true;
}

void AppHighlightStorage::restoreUnrestoredAppHighlights()
{
    Vector<AppHighlightRangeData> remainingRanges;
    
    for (auto& highlight : m_unrestoredHighlights) {
        if (!attemptToRestoreHighlightAndScroll(highlight, ScrollToHighlight::No))
            remainingRanges.append(highlight);
    }
    if (m_unrestoredScrollHighlight) {
        if (attemptToRestoreHighlightAndScroll(m_unrestoredScrollHighlight.value(), ScrollToHighlight::Yes))
            m_unrestoredScrollHighlight.reset();
    }
        
    m_timeAtLastRangeSearch = MonotonicTime::now();
    m_unrestoredHighlights = WTFMove(remainingRanges);
}

#endif

} // namespace WebCore
