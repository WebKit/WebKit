/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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
#include "TextTrackCue.h"

#if ENABLE(VIDEO)

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DOMRect.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLDivElement.h"
#include "HTMLStyleElement.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "VTTCue.h"
#include "VTTRegionList.h"
#include <limits.h>
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrackCue);
WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrackCueBox);

const AtomString& TextTrackCue::cueShadowPseudoId()
{
    static MainThreadNeverDestroyed<const AtomString> cue("cue", AtomString::ConstructFromLiteral);
    return cue;
}

const AtomString& TextTrackCue::cueBoxShadowPseudoId()
{
    static MainThreadNeverDestroyed<const AtomString> trackDisplayBoxShadowPseudoId("-webkit-media-text-track-display", AtomString::ConstructFromLiteral);
    return trackDisplayBoxShadowPseudoId;
}

const AtomString& TextTrackCue::cueBackdropShadowPseudoId()
{
    static MainThreadNeverDestroyed<const AtomString> cueBackdropShadowPseudoId("-webkit-media-text-track-display-backdrop", AtomString::ConstructFromLiteral);
    return cueBackdropShadowPseudoId;
}

static const QualifiedName& cueAttributName()
{
    static NeverDestroyed<QualifiedName> cueTag(nullAtom(), "cue", nullAtom());
    return cueTag;
}

static const QualifiedName& cueBackgroundAttributName()
{
    static NeverDestroyed<QualifiedName> cueBackgroundTag(nullAtom(), "cuebackground", nullAtom());
    return cueBackgroundTag;
}

Ref<TextTrackCueBox> TextTrackCueBox::create(Document& document, TextTrackCue& cue)
{
    auto box = adoptRef(*new TextTrackCueBox(document, cue));
    box->initialize();
    return box;
}

TextTrackCueBox::TextTrackCueBox(Document& document, TextTrackCue& cue)
    : HTMLElement(HTMLNames::divTag, document)
    , m_cue(makeWeakPtr(cue))
{
    setHasCustomStyleResolveCallbacks();
}

void TextTrackCueBox::initialize()
{
    setPseudo(TextTrackCue::cueBoxShadowPseudoId());
}

TextTrackCue* TextTrackCueBox::getCue() const
{
    return m_cue.get();
}

static inline bool isLegalNode(Node& node)
{
    return node.hasTagName(HTMLNames::brTag)
        || node.hasTagName(HTMLNames::divTag)
        || node.hasTagName(HTMLNames::imgTag)
        || node.hasTagName(HTMLNames::pTag)
        || node.hasTagName(HTMLNames::rbTag)
        || node.hasTagName(HTMLNames::rtTag)
        || node.hasTagName(HTMLNames::rtcTag)
        || node.hasTagName(HTMLNames::rubyTag)
        || node.hasTagName(HTMLNames::spanTag)
        || node.nodeType() == Node::TEXT_NODE;
}

static Exception invalidNodeException(Node& node)
{
    return Exception { InvalidNodeTypeError, makeString("Invalid node type: ", node.nodeName()) };
}

static ExceptionOr<void> checkForInvalidNodeTypes(Node& root)
{
    if (!isLegalNode(root))
        return invalidNodeException(root);

    for (auto* child = root.firstChild(); child; child = child->nextSibling()) {
        if (!isLegalNode(*child))
            return invalidNodeException(*child);

        if (is<ContainerNode>(*child)) {
            auto result = checkForInvalidNodeTypes(*child);
            if (result.hasException())
                return result.releaseException();
        }
    }

    return { };
}

enum RequiredNodes {
    Cue = 1 << 0,
    CueBackground = 1 << 1,
};

static OptionSet<RequiredNodes> tagPseudoObjects(Node& node)
{
    if (!is<Element>(node))
        return { };

    OptionSet<RequiredNodes> nodeTypes = { };

    auto& element = downcast<Element>(node);
    if (element.hasAttributeWithoutSynchronization(cueAttributName())) {
        element.setPseudo(TextTrackCue::cueShadowPseudoId());
        nodeTypes = { RequiredNodes::Cue };
    } else if (element.hasAttributeWithoutSynchronization(cueBackgroundAttributName())) {
        element.setPseudo(TextTrackCue::cueBackdropShadowPseudoId());
        nodeTypes = { RequiredNodes::CueBackground };
    }

    for (auto* child = element.firstChild(); child; child = child->nextSibling())
        nodeTypes.add(tagPseudoObjects(*child));

    return nodeTypes;
}

static void removePseudoAttributes(Node& node)
{
    if (!is<Element>(node))
        return;

    auto& element = downcast<Element>(node);
    if (element.hasAttributeWithoutSynchronization(cueAttributName()) || element.hasAttributeWithoutSynchronization(cueBackgroundAttributName()))
        element.removeAttribute(HTMLNames::pseudoAttr);

    for (auto* child = element.firstChild(); child; child = child->nextSibling())
        removePseudoAttributes(*child);
}

ExceptionOr<Ref<TextTrackCue>> TextTrackCue::create(Document& document, double start, double end, DocumentFragment& cueFragment)
{
    if (!cueFragment.firstChild())
        return Exception { InvalidNodeTypeError, "Empty cue fragment" };

    for (Node* node = cueFragment.firstChild(); node; node = node->nextSibling()) {
        auto result = checkForInvalidNodeTypes(*node);
        if (result.hasException())
            return result.releaseException();
    }

    auto fragment = DocumentFragment::create(document);
    for (Node* node = cueFragment.firstChild(); node; node = node->nextSibling()) {
        auto result = fragment->ensurePreInsertionValidity(*node, nullptr);
        if (result.hasException())
            return result.releaseException();
    }
    cueFragment.cloneChildNodes(fragment);

    OptionSet<RequiredNodes> nodeTypes = { };
    for (Node* node = fragment->firstChild(); node; node = node->nextSibling())
        nodeTypes.add(tagPseudoObjects(*node));

    if (!nodeTypes.contains(RequiredNodes::Cue))
        return Exception { InvalidStateError, makeString("Missing required attribute: ", cueAttributName().toString()) };
    if (!nodeTypes.contains(RequiredNodes::CueBackground))
        return Exception { InvalidStateError, makeString("Missing required attribute: ", cueBackgroundAttributName().toString()) };

    auto textTrackCue = adoptRef(*new TextTrackCue(document, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), WTFMove(fragment)));
    textTrackCue->suspendIfNeeded();
    return textTrackCue;
}

TextTrackCue::TextTrackCue(Document& document, const MediaTime& start, const MediaTime& end, Ref<DocumentFragment>&& cueFragment)
    : ActiveDOMObject(document)
    , m_startTime(start)
    , m_endTime(end)
    , m_document(document)
    , m_cueNode(WTFMove(cueFragment))
{
}

TextTrackCue::TextTrackCue(Document& document, const MediaTime& start, const MediaTime& end)
    : ActiveDOMObject(document)
    , m_startTime(start)
    , m_endTime(end)
    , m_document(document)
{
}

ScriptExecutionContext* TextTrackCue::scriptExecutionContext() const
{
    return &m_document;
}

void TextTrackCue::willChange()
{
    if (++m_processingCueChanges > 1)
        return;

    if (m_track)
        m_track->cueWillChange(*this);
}

void TextTrackCue::didChange()
{
    ASSERT(m_processingCueChanges);
    if (--m_processingCueChanges)
        return;

    m_displayTreeNeedsUpdate = true;

    if (m_track)
        m_track->cueDidChange(*this);
}

TextTrack* TextTrackCue::track() const
{
    return m_track;
}

void TextTrackCue::setTrack(TextTrack* track)
{
    m_track = track;
}

void TextTrackCue::setId(const String& id)
{
    if (m_id == id)
        return;

    willChange();
    m_id = id;
    didChange();
}

void TextTrackCue::setStartTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_startTime.toDouble() == value || value < 0)
        return;

    setStartTime(MediaTime::createWithDouble(value));
}

void TextTrackCue::setStartTime(const MediaTime& value)
{
    willChange();
    m_startTime = value;
    didChange();
}
    
void TextTrackCue::setEndTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_endTime.toDouble() == value || value < 0)
        return;

    setEndTime(MediaTime::createWithDouble(value));
}

void TextTrackCue::setEndTime(const MediaTime& value)
{
    willChange();
    m_endTime = value;
    didChange();
}
    
void TextTrackCue::setPauseOnExit(bool value)
{
    if (m_pauseOnExit == value)
        return;
    
    m_pauseOnExit = value;
}

void TextTrackCue::dispatchEvent(Event& event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::Mode::Disabled)
        return;

    EventTarget::dispatchEvent(event);
}

bool TextTrackCue::isActive() const
{
    return m_isActive && track() && track()->mode() != TextTrack::Mode::Disabled;
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;

    if (m_isActive || !m_displayTree)
        return;

    // The display tree is never exposed to author scripts so it's safe to dispatch events here.
    ScriptDisallowedScope::EventAllowedScope allowedScope(*m_displayTree);
    m_displayTree->remove();
}

unsigned TextTrackCue::cueIndex() const
{
    ASSERT(m_track && m_track->cuesInternal());
    if (!m_track || !m_track->cuesInternal())
        return std::numeric_limits<unsigned>::max();

    return m_track->cuesInternal()->cueIndex(*this);
}

bool TextTrackCue::isOrderedBefore(const TextTrackCue* other) const
{
    // ... cues must be sorted by their start time, earliest first;
    if (startMediaTime() != other->startMediaTime())
        return startMediaTime() < other->startMediaTime();

    // then, any cues with the same start time must be sorted by their end time, latest first;
    if (endMediaTime() != other->endMediaTime())
        return endMediaTime() > other->endMediaTime();

    // and finally, any cues with identical end times must be sorted in the order they were last added to
    // their respective text track list of cues, oldest first (so e.g. for cues from a WebVTT file, that
    // would initially be the order in which the cues were listed in the file)
    return cueIndex() < other->cueIndex();
}

bool TextTrackCue::cueContentsMatch(const TextTrackCue& other) const
{
    return m_id == other.m_id;
}

bool TextTrackCue::isEqual(const TextTrackCue& other, TextTrackCue::CueMatchRules match) const
{
    if (match != IgnoreDuration && endMediaTime() != other.endMediaTime())
        return false;
    return cueType() == other.cueType() && hasEquivalentStartTime(other) && cueContentsMatch(other);
}

bool TextTrackCue::hasEquivalentStartTime(const TextTrackCue& cue) const
{
    MediaTime startTimeVariance = MediaTime::zeroTime();
    if (track())
        startTimeVariance = track()->startTimeVariance();
    else if (cue.track())
        startTimeVariance = cue.track()->startTimeVariance();

    return abs(abs(startMediaTime()) - abs(cue.startMediaTime())) <= startTimeVariance;
}

void TextTrackCue::toJSON(JSON::Object& value) const
{
    ASCIILiteral type = "Generic"_s;
    switch (cueType()) {
    case TextTrackCue::ConvertedToWebVTT:
        type = "ConvertedToWebVTT"_s;
        break;
    case TextTrackCue::WebVTT:
        type = "WebVTT"_s;
        break;
    case TextTrackCue::Data:
        type = "Data"_s;
        break;
    case TextTrackCue::Generic:
        type = "Generic"_s;
        break;
    }

    value.setString("type"_s, type);
    value.setDouble("startTime"_s, startTime());
    value.setDouble("endTime"_s, endTime());
}

String TextTrackCue::toJSONString() const
{
    auto object = JSON::Object::create();
    toJSON(object.get());
    return object->toJSONString();
}

#ifndef NDEBUG

TextStream& operator<<(TextStream& stream, const TextTrackCue& cue)
{
    String text;
    if (is<VTTCue>(cue))
        text = downcast<VTTCue>(cue).text();
    return stream << &cue << " id=" << cue.id() << " interval=" << cue.startTime() << "-->" << cue.endTime() << " cue=" << text << ')';
}

#endif

RefPtr<DocumentFragment> TextTrackCue::getCueAsHTML()
{
    if (!m_cueNode)
        return nullptr;

    auto clonedFragment = DocumentFragment::create(ownerDocument());
    m_cueNode->cloneChildNodes(clonedFragment);

    for (Node* node = clonedFragment->firstChild(); node; node = node->nextSibling())
        removePseudoAttributes(*node);

    return clonedFragment;
}

bool TextTrackCue::isRenderable() const
{
    return m_cueNode && m_cueNode->firstChild();
}

RefPtr<TextTrackCueBox> TextTrackCue::getDisplayTree(const IntSize&, int)
{
    if (m_displayTree && !m_displayTreeNeedsUpdate)
        return m_displayTree;

    rebuildDisplayTree();

    return m_displayTree;
}

void TextTrackCue::removeDisplayTree()
{
    if (!m_displayTree)
        return;

    // The display tree is never exposed to author scripts so it's safe to dispatch events here.
    ScriptDisallowedScope::EventAllowedScope allowedScope(*m_displayTree);
    m_displayTree->remove();
}

void TextTrackCue::setFontSize(int fontSize, const IntSize&, bool important)
{
    if (fontSize == m_fontSize && important == m_fontSizeIsImportant)
        return;

    m_displayTreeNeedsUpdate = true;
    m_fontSizeIsImportant = important;
    m_fontSize = fontSize;
}

void TextTrackCue::rebuildDisplayTree()
{
    if (!m_cueNode)
        return;

    ScriptDisallowedScope::EventAllowedScope allowedScopeForReferenceTree(*m_cueNode);

    if (!m_displayTree) {
        static MainThreadNeverDestroyed<const AtomString> webkitGenericCueRootName("-webkit-generic-cue-root", AtomString::ConstructFromLiteral);
        m_displayTree = TextTrackCueBox::create(ownerDocument(), *this);
        m_displayTree->setPseudo(webkitGenericCueRootName);
    }

    m_displayTree->removeChildren();
    auto clonedFragment = DocumentFragment::create(ownerDocument());
    m_cueNode->cloneChildNodes(clonedFragment);
    m_displayTree->appendChild(clonedFragment);

    if (m_fontSize) {
        if (auto page = ownerDocument().page()) {
            auto style = HTMLStyleElement::create(HTMLNames::styleTag, ownerDocument(), false);
            style->setTextContent(makeString(page->captionUserPreferencesStyleSheet(),
                " ::", TextTrackCue::cueShadowPseudoId(), "{font-size:", m_fontSize, m_fontSizeIsImportant ? "px !important}" : "px}"));
            m_displayTree->appendChild(style);
        }
    }

    if (track()) {
        if (const auto& styleSheets = track()->styleSheets()) {
            for (const auto& cssString : *styleSheets) {
                auto style = HTMLStyleElement::create(HTMLNames::styleTag, m_displayTree->document(), false);
                style->setTextContent(cssString);
                m_displayTree->appendChild(style);
            }
        }
    }

    m_displayTreeNeedsUpdate = false;
}

const char* TextTrackCue::activeDOMObjectName() const
{
    return "TextTrackCue";
}

} // namespace WebCore

#endif
