/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCue.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "HTMLDivElement.h"
#include "HTMLMediaElement.h"
#include "HTMLSpanElement.h"
#include "NodeTraversal.h"
#include "RenderTextTrackCue.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "WebVTTParser.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const int invalidCueIndex = -1;
static const int undefinedPosition = -1;
static const int autoSize = 0;

static const String& startKeyword()
{
    DEFINE_STATIC_LOCAL(const String, start, (ASCIILiteral("start")));
    return start;
}

static const String& middleKeyword()
{
    DEFINE_STATIC_LOCAL(const String, middle, (ASCIILiteral("middle")));
    return middle;
}

static const String& endKeyword()
{
    DEFINE_STATIC_LOCAL(const String, end, (ASCIILiteral("end")));
    return end;
}

static const String& horizontalKeyword()
{
    return emptyString();
}

static const String& verticalGrowingLeftKeyword()
{
    DEFINE_STATIC_LOCAL(const String, verticalrl, (ASCIILiteral("rl")));
    return verticalrl;
}

static const String& verticalGrowingRightKeyword()
{
    DEFINE_STATIC_LOCAL(const String, verticallr, (ASCIILiteral("lr")));
    return verticallr;
}

// ----------------------------

TextTrackCueBox::TextTrackCueBox(Document* document, TextTrackCue* cue)
    : HTMLElement(divTag, document)
    , m_cue(cue)
{
    setPseudo(textTrackCueBoxShadowPseudoId());
}

TextTrackCue* TextTrackCueBox::getCue() const
{
    return m_cue;
}

void TextTrackCueBox::applyCSSProperties()
{
    // FIXME: Apply all the initial CSS positioning properties. http://wkb.ug/79916

    // 3.5.1 On the (root) List of WebVTT Node Objects:

    // the 'position' property must be set to 'absolute'
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);

    //  the 'unicode-bidi' property must be set to 'plaintext'
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueWebkitPlaintext);

    // FIXME: Determine the text direction using the BIDI algorithm. http://wkb.ug/79749
    // the 'direction' property must be set to direction
    setInlineStyleProperty(CSSPropertyDirection, CSSValueLtr);

    // the 'writing-mode' property must be set to writing-mode
    setInlineStyleProperty(CSSPropertyWebkitWritingMode, m_cue->getCSSWritingMode(), false);

    std::pair<float, float> position = m_cue->getCSSPosition();

    // the 'top' property must be set to top,
    setInlineStyleProperty(CSSPropertyTop, static_cast<double>(position.second), CSSPrimitiveValue::CSS_PERCENTAGE);

    // the 'left' property must be set to left
    setInlineStyleProperty(CSSPropertyLeft, static_cast<double>(position.first), CSSPrimitiveValue::CSS_PERCENTAGE);

    // the 'width' property must be set to width, and the 'height' property  must be set to height
    if (m_cue->vertical() == horizontalKeyword()) {
        setInlineStyleProperty(CSSPropertyWidth, static_cast<double>(m_cue->getCSSSize()), CSSPrimitiveValue::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    } else {
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);
        setInlineStyleProperty(CSSPropertyHeight, static_cast<double>(m_cue->getCSSSize()),  CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    // The 'text-align' property on the (root) List of WebVTT Node Objects must
    // be set to the value in the second cell of the row of the table below
    // whose first cell is the value of the corresponding cue's text track cue
    // alignment:
    if (m_cue->align() == startKeyword())
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueStart);
    else if (m_cue->align() == endKeyword())
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueEnd);
    else
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueCenter);

    if (!m_cue->snapToLines()) {
        // 10.13.1 Set up x and y:
        // Note: x and y are set through the CSS left and top above.

        // 10.13.2 Position the boxes in boxes such that the point x% along the
        // width of the bounding box of the boxes in boxes is x% of the way
        // across the width of the video's rendering area, and the point y%
        // along the height of the bounding box of the boxes in boxes is y%
        // of the way across the height of the video's rendering area, while
        // maintaining the relative positions of the boxes in boxes to each
        // other.
        setInlineStyleProperty(CSSPropertyWebkitTransform,
                String::format("translate(-%.2f%%, -%.2f%%)", position.first, position.second));

        setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePre);
    }
}

const AtomicString& TextTrackCueBox::textTrackCueBoxShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(const AtomicString, trackDisplayBoxShadowPseudoId, ("-webkit-media-text-track-display", AtomicString::ConstructFromLiteral));
    return trackDisplayBoxShadowPseudoId;
}

RenderObject* TextTrackCueBox::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextTrackCue(this);
}

// ----------------------------

const AtomicString& TextTrackCue::allNodesShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(const AtomicString, subtitles, ("-webkit-media-text-track-all-nodes", AtomicString::ConstructFromLiteral));
    return subtitles;
}

TextTrackCue::TextTrackCue(ScriptExecutionContext* context, double start, double end, const String& content)
    : m_startTime(start)
    , m_endTime(end)
    , m_content(content)
    , m_linePosition(undefinedPosition)
    , m_computedLinePosition(undefinedPosition)
    , m_textPosition(50)
    , m_cueSize(100)
    , m_cueIndex(invalidCueIndex)
    , m_writingDirection(Horizontal)
    , m_cueAlignment(Middle)
    , m_webVTTNodeTree(0)
    , m_track(0)
    , m_scriptExecutionContext(context)
    , m_isActive(false)
    , m_pauseOnExit(false)
    , m_snapToLines(true)
    , m_allDocumentNodes(HTMLDivElement::create(static_cast<Document*>(context)))
    , m_displayTreeShouldChange(true)
    , m_displayTree(TextTrackCueBox::create(static_cast<Document*>(m_scriptExecutionContext), this))
{
    ASSERT(m_scriptExecutionContext->isDocument());

    // 4. If the text track cue writing direction is horizontal, then let
    // writing-mode be 'horizontal-tb'. Otherwise, if the text track cue writing
    // direction is vertical growing left, then let writing-mode be
    // 'vertical-rl'. Otherwise, the text track cue writing direction is
    // vertical growing right; let writing-mode be 'vertical-lr'.
    m_displayWritingModeMap[Horizontal] = CSSValueHorizontalTb;
    m_displayWritingModeMap[VerticalGrowingLeft] = CSSValueVerticalRl;
    m_displayWritingModeMap[VerticalGrowingRight] = CSSValueVerticalLr;
}

TextTrackCue::~TextTrackCue()
{
}

void TextTrackCue::cueWillChange()
{
    if (m_track)
        m_track->cueWillChange(this);
}

void TextTrackCue::cueDidChange()
{
    if (m_track)
        m_track->cueDidChange(this);

    m_displayTreeShouldChange = true;
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

    cueWillChange();
    m_id = id;
    cueDidChange();
}

void TextTrackCue::setStartTime(double value, ExceptionCode& ec)
{
    // NaN, Infinity and -Infinity values should trigger a TypeError.
    if (isinf(value) || isnan(value)) {
        ec = TypeError;
        return;
    }
    
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_startTime == value || value < 0)
        return;
    
    cueWillChange();
    m_startTime = value;
    cueDidChange();
}
    
void TextTrackCue::setEndTime(double value, ExceptionCode& ec)
{
    // NaN, Infinity and -Infinity values should trigger a TypeError.
    if (isinf(value) || isnan(value)) {
        ec = TypeError;
        return;
    }

    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_endTime == value || value < 0)
        return;
    
    cueWillChange();
    m_endTime = value;
    cueDidChange();
}
    
void TextTrackCue::setPauseOnExit(bool value)
{
    if (m_pauseOnExit == value)
        return;
    
    cueWillChange();
    m_pauseOnExit = value;
    cueDidChange();
}

const String& TextTrackCue::vertical() const
{
    switch (m_writingDirection) {
    case Horizontal: 
        return horizontalKeyword();
    case VerticalGrowingLeft:
        return verticalGrowingLeftKeyword();
    case VerticalGrowingRight:
        return verticalGrowingRightKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void TextTrackCue::setVertical(const String& value, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-vertical
    // On setting, the text track cue writing direction must be set to the value given 
    // in the first cell of the row in the table above whose second cell is a 
    // case-sensitive match for the new value, if any. If none of the values match, then
    // the user agent must instead throw a SyntaxError exception.
    
    WritingDirection direction = m_writingDirection;
    if (value == horizontalKeyword())
        direction = Horizontal;
    else if (value == verticalGrowingLeftKeyword())
        direction = VerticalGrowingLeft;
    else if (value == verticalGrowingRightKeyword())
        direction = VerticalGrowingRight;
    else
        ec = SYNTAX_ERR;
    
    if (direction == m_writingDirection)
        return;

    cueWillChange();
    m_writingDirection = direction;
    cueDidChange();
}

void TextTrackCue::setSnapToLines(bool value)
{
    if (m_snapToLines == value)
        return;
    
    cueWillChange();
    m_snapToLines = value;
    cueDidChange();
}

void TextTrackCue::setLine(int position, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-line
    // On setting, if the text track cue snap-to-lines flag is not set, and the new
    // value is negative or greater than 100, then throw an IndexSizeError exception.
    if (!m_snapToLines && (position < 0 || position > 100)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // Otherwise, set the text track cue line position to the new value.
    if (m_linePosition == position)
        return;

    cueWillChange();
    m_linePosition = position;
    m_computedLinePosition = calculateComputedLinePosition();
    cueDidChange();
}

void TextTrackCue::setPosition(int position, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-position
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError exception.
    // Otherwise, set the text track cue text position to the new value.
    if (position < 0 || position > 100) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // Otherwise, set the text track cue line position to the new value.
    if (m_textPosition == position)
        return;
    
    cueWillChange();
    m_textPosition = position;
    cueDidChange();
}

void TextTrackCue::setSize(int size, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-size
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError
    // exception. Otherwise, set the text track cue size to the new value.
    if (size < 0 || size > 100) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // Otherwise, set the text track cue line position to the new value.
    if (m_cueSize == size)
        return;
    
    cueWillChange();
    m_cueSize = size;
    cueDidChange();
}

const String& TextTrackCue::align() const
{
    switch (m_cueAlignment) {
    case Start:
        return startKeyword();
    case Middle:
        return middleKeyword();
    case End:
        return endKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void TextTrackCue::setAlign(const String& value, ExceptionCode& ec)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-align
    // On setting, the text track cue alignment must be set to the value given in the 
    // first cell of the row in the table above whose second cell is a case-sensitive
    // match for the new value, if any. If none of the values match, then the user
    // agent must instead throw a SyntaxError exception.
    
    Alignment alignment = m_cueAlignment;
    if (value == startKeyword())
        alignment = Start;
    else if (value == middleKeyword())
        alignment = Middle;
    else if (value == endKeyword())
        alignment = End;
    else
        ec = SYNTAX_ERR;
    
    if (alignment == m_cueAlignment)
        return;

    cueWillChange();
    m_cueAlignment = alignment;
    cueDidChange();
}
    
void TextTrackCue::setText(const String& text)
{
    if (m_content == text)
        return;
    
    cueWillChange();
    // Clear the document fragment but don't bother to create it again just yet as we can do that
    // when it is requested.
    m_webVTTNodeTree = 0;
    m_content = text;
    cueDidChange();
}

int TextTrackCue::cueIndex()
{
    if (m_cueIndex == invalidCueIndex)
        m_cueIndex = track()->cues()->getCueIndex(this);

    return m_cueIndex;
}

void TextTrackCue::invalidateCueIndex()
{
    m_cueIndex = invalidCueIndex;
}

void TextTrackCue::createWebVTTNodeTree()
{
    if (!m_webVTTNodeTree)
        m_webVTTNodeTree = WebVTTParser::create(0, m_scriptExecutionContext)->createDocumentFragmentFromCueText(m_content);
}

void TextTrackCue::copyWebVTTNodeToDOMTree(ContainerNode* webVTTNode, ContainerNode* parent)
{
    for (Node* node = webVTTNode->firstChild(); node; node = node->nextSibling()) {
        RefPtr<Node> clonedNode;
        // Specs require voice and class WebVTT elements to be spans for DOM trees.
        if (node->hasTagName(voiceElementTagName()) || node->hasTagName(classElementTagName())) {
            clonedNode = HTMLSpanElement::create(spanTag, static_cast<Document*>(m_scriptExecutionContext));
            toElement(clonedNode.get())->setAttribute(classAttr, toElement(node)->getAttribute(classAttr));
            toElement(clonedNode.get())->setAttribute(titleAttr, toElement(node)->getAttribute(voiceAttributeName()));
        } else
            clonedNode = node->cloneNode(false);

        parent->appendChild(clonedNode, ASSERT_NO_EXCEPTION);
        if (node->isContainerNode())
            copyWebVTTNodeToDOMTree(toContainerNode(node), toContainerNode(clonedNode.get()));
    }
}

PassRefPtr<DocumentFragment> TextTrackCue::getCueAsHTML()
{
    createWebVTTNodeTree();
    Document* document = static_cast<Document*>(m_scriptExecutionContext);
    RefPtr<DocumentFragment> clonedFragment = DocumentFragment::create(document);
    copyWebVTTNodeToDOMTree(m_webVTTNodeTree.get(), clonedFragment.get());
    return clonedFragment.release();
}

PassRefPtr<DocumentFragment> TextTrackCue::createCueRenderingTree()
{
    RefPtr<DocumentFragment> clonedFragment;
    createWebVTTNodeTree();
    Document* document = static_cast<Document*>(m_scriptExecutionContext);
    clonedFragment = DocumentFragment::create(document);
    m_webVTTNodeTree->cloneChildNodes(clonedFragment.get());
    return clonedFragment.release();
}

bool TextTrackCue::dispatchEvent(PassRefPtr<Event> event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::disabledKeyword())
        return false;

    return EventTarget::dispatchEvent(event);
}

bool TextTrackCue::isActive()
{
    return m_isActive && track() && track()->mode() != TextTrack::disabledKeyword();
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;

    if (!active) {
        // Remove the display tree as soon as the cue becomes inactive.
        ExceptionCode ec;
        m_displayTree->remove(ec);
    }
}

int TextTrackCue::calculateComputedLinePosition()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#text-track-cue-computed-line-position

    // If the text track cue line position is numeric, then that is the text
    // track cue computed line position.
    if (m_linePosition != undefinedPosition)
        return m_linePosition;

    // If the text track cue snap-to-lines flag of the text track cue is not
    // set, the text track cue computed line position is the value 100;
    if (!m_snapToLines)
        return 100;

    // Otherwise, it is the value returned by the following algorithm:

    // If cue is not associated with a text track, return -1 and abort these
    // steps.
    if (!track())
        return -1;

    // Let n be the number of text tracks whose text track mode is showing or
    // showing by default and that are in the media element's list of text
    // tracks before track.
    int n = track()->trackIndexRelativeToRenderedTracks();

    // Increment n by one.
    n++;

    // Negate n.
    n = -n;

    return n;
}

void TextTrackCue::calculateDisplayParameters()
{
    // FIXME(BUG 79749): Determine the text direction using the BIDI algorithm.
    // Steps 10.2, 10.3
    m_displayDirection = CSSValueLtr;

    // 10.4 If the text track cue writing direction is horizontal, then let
    // block-flow be 'tb'. Otherwise, if the text track cue writing direction is
    // vertical growing left, then let block-flow be 'lr'. Otherwise, the text
    // track cue writing direction is vertical growing right; let block-flow be
    // 'rl'.
    m_displayWritingMode = m_displayWritingModeMap[m_writingDirection];

    // 10.5 Determine the value of maximum size for cue as per the appropriate
    // rules from the following list:
    int maximumSize = m_textPosition;
    if ((m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueLtr)
            || (m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueRtl)
            || (m_writingDirection == VerticalGrowingLeft && m_cueAlignment == Start)
            || (m_writingDirection == VerticalGrowingRight && m_cueAlignment == Start)) {
        maximumSize = 100 - m_textPosition;
    } else if ((m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueLtr)
            || (m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueRtl)
            || (m_writingDirection == VerticalGrowingLeft && m_cueAlignment == End)
            || (m_writingDirection == VerticalGrowingRight && m_cueAlignment == End)) {
        maximumSize = m_textPosition;
    } else if (m_cueAlignment == Middle) {
        maximumSize = m_textPosition <= 50 ? m_textPosition : (100 - m_textPosition);
        maximumSize = maximumSize * 2;
    }

    // 10.6 If the text track cue size is less than maximum size, then let size
    // be text track cue size. Otherwise, let size be maximum size.
    m_displaySize = std::min(m_cueSize, maximumSize);

    // 10.8 Determine the value of x-position or y-position for cue as per the
    // appropriate rules from the following list:
    if (m_writingDirection == Horizontal) {
        if (m_cueAlignment == Start) {
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = m_textPosition;
            else
                m_displayPosition.first = 100 - m_textPosition - m_displaySize;
        } else if (m_cueAlignment == End) {
            if (m_displayDirection == CSSValueRtl)
                m_displayPosition.first = 100 - m_textPosition;
            else
                m_displayPosition.first = m_textPosition - m_displaySize;
        }
    }

    if ((m_writingDirection == VerticalGrowingLeft && m_cueAlignment == Start)
            || (m_writingDirection == VerticalGrowingRight && m_cueAlignment == Start)) {
        m_displayPosition.second = m_textPosition;
    } else if ((m_writingDirection == VerticalGrowingLeft && m_cueAlignment == End)
            || (m_writingDirection == VerticalGrowingRight && m_cueAlignment == End)) {
        m_displayPosition.second = 100 - m_textPosition;
    }

    if (m_writingDirection == Horizontal && m_cueAlignment == Middle) {
        if (m_displayDirection == CSSValueLtr)
            m_displayPosition.first = m_textPosition - m_displaySize / 2;
        else
           m_displayPosition.first = 100 - m_textPosition - m_displaySize / 2;
    }

    if ((m_writingDirection == VerticalGrowingLeft && m_cueAlignment == Middle)
        || (m_writingDirection == VerticalGrowingRight && m_cueAlignment == Middle))
        m_displayPosition.second = m_textPosition - m_displaySize / 2;

    // 10.9 Determine the value of whichever of x-position or y-position is not
    // yet calculated for cue as per the appropriate rules from the following
    // list:
    if (m_snapToLines && m_displayPosition.second == undefinedPosition && m_writingDirection == Horizontal)
        m_displayPosition.second = 0;

    if (!m_snapToLines && m_displayPosition.second == undefinedPosition && m_writingDirection == Horizontal)
        m_displayPosition.second = m_computedLinePosition;

    if (m_snapToLines && m_displayPosition.first == undefinedPosition
            && (m_writingDirection == VerticalGrowingLeft || m_writingDirection == VerticalGrowingRight))
        m_displayPosition.first = 0;

    if (!m_snapToLines && (m_writingDirection == VerticalGrowingLeft || m_writingDirection == VerticalGrowingRight))
        m_displayPosition.first = m_computedLinePosition;

    // A text track cue has a text track cue computed line position whose value
    // is defined in terms of the other aspects of the cue.
    m_computedLinePosition = calculateComputedLinePosition();
}
    
void TextTrackCue::markFutureAndPastNodes(ContainerNode* root, double previousTimestamp, double movieTime)
{
    DEFINE_STATIC_LOCAL(const String, timestampTag, (ASCIILiteral("timestamp")));
    
    bool isPastNode = true;
    double currentTimestamp = previousTimestamp;
    if (currentTimestamp > movieTime)
        isPastNode = false;
    
    for (Node* child = root->firstChild(); child; child = NodeTraversal::next(child, root)) {
        if (child->nodeName() == timestampTag) {
            unsigned position = 0;
            String timestamp = child->nodeValue();
            double currentTimestamp = WebVTTParser::create(0, m_scriptExecutionContext)->collectTimeStamp(timestamp, &position);
            ASSERT(currentTimestamp != -1);
            
            if (currentTimestamp > movieTime)
                isPastNode = false;
        }
        
        if (child->isElementNode()) {
            toElement(child)->setWebVTTNodeType(isPastNode ? WebVTTNodeTypePast : WebVTTNodeTypeFuture);
            // Make an elemenet id match a cue id for style matching purposes.
            if (!m_id.isEmpty())
                toElement(child)->setIdAttribute(AtomicString(m_id.characters(), m_id.length()));
        }
    }
}

void TextTrackCue::updateDisplayTree(float movieTime)
{
    // The display tree may contain WebVTT timestamp objects representing
    // timestamps (processing instructions), along with displayable nodes.

    if (!track()->isRendered())
      return;

    // Clear the contents of the set.
    m_allDocumentNodes->removeChildren();

    // Update the two sets containing past and future WebVTT objects.
    RefPtr<DocumentFragment> referenceTree = createCueRenderingTree();
    markFutureAndPastNodes(referenceTree.get(), startTime(), movieTime);
    m_allDocumentNodes->appendChild(referenceTree);
}

PassRefPtr<TextTrackCueBox> TextTrackCue::getDisplayTree()
{
    if (!m_displayTreeShouldChange || !track()->isRendered())
        return m_displayTree;

    // 10.1 - 10.10
    calculateDisplayParameters();

    // 10.11. Apply the terms of the CSS specifications to nodes within the
    // following constraints, thus obtaining a set of CSS boxes positioned
    // relative to an initial containing block:
    m_displayTree->removeChildren();

    // The document tree is the tree of WebVTT Node Objects rooted at nodes.

    // The children of the nodes must be wrapped in an anonymous box whose
    // 'display' property has the value 'inline'. This is the WebVTT cue
    // background box.

    // Note: This is contained by default in m_allDocumentNodes.
    m_allDocumentNodes->setPseudo(allNodesShadowPseudoId());
    m_displayTree->appendChild(m_allDocumentNodes, ASSERT_NO_EXCEPTION, true);

    // FIXME(BUG 79916): Runs of children of WebVTT Ruby Objects that are not
    // WebVTT Ruby Text Objects must be wrapped in anonymous boxes whose
    // 'display' property has the value 'ruby-base'.

    // FIXME(BUG 79916): Text runs must be wrapped according to the CSS
    // line-wrapping rules, except that additionally, regardless of the value of
    // the 'white-space' property, lines must be wrapped at the edge of their
    // containing blocks, even if doing so requires splitting a word where there
    // is no line breaking opportunity. (Thus, normally text wraps as needed,
    // but if there is a particularly long word, it does not overflow as it
    // normally would in CSS, it is instead forcibly wrapped at the box's edge.)
    m_displayTree->applyCSSProperties();

    m_displayTreeShouldChange = false;

    // 10.15. Let cue's text track cue display state have the CSS boxes in
    // boxes.
    return m_displayTree;
}

void TextTrackCue::removeDisplayTree()
{
    m_displayTree->remove(ASSERT_NO_EXCEPTION);
}

std::pair<double, double> TextTrackCue::getPositionCoordinates() const
{
    // This method is used for setting x and y when snap to lines is not set.
    std::pair<double, double> coordinates;

    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueLtr) {
        coordinates.first = m_textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueRtl) {
        coordinates.first = 100 - m_textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingLeft) {
        coordinates.first = 100 - m_computedLinePosition;
        coordinates.second = m_textPosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingRight) {
        coordinates.first = m_computedLinePosition;
        coordinates.second = m_textPosition;

        return coordinates;
    }

    ASSERT_NOT_REACHED();

    return coordinates;
}

TextTrackCue::CueSetting TextTrackCue::settingName(const String& name)
{
    DEFINE_STATIC_LOCAL(const String, verticalKeyword, (ASCIILiteral("vertical")));
    DEFINE_STATIC_LOCAL(const String, lineKeyword, (ASCIILiteral("line")));
    DEFINE_STATIC_LOCAL(const String, positionKeyword, (ASCIILiteral("position")));
    DEFINE_STATIC_LOCAL(const String, sizeKeyword, (ASCIILiteral("size")));
    DEFINE_STATIC_LOCAL(const String, alignKeyword, (ASCIILiteral("align")));

    if (name == verticalKeyword)
        return Vertical;
    else if (name == lineKeyword)
        return Line;
    else if (name == positionKeyword)
        return Position;
    else if (name == sizeKeyword)
        return Size;
    else if (name == alignKeyword)
        return Align;

    return None;
}

void TextTrackCue::setCueSettings(const String& input)
{
    m_settings = input;
    unsigned position = 0;

    while (position < input.length()) {

        // The WebVTT cue settings part of a WebVTT cue consists of zero or more of the following components, in any order, 
        // separated from each other by one or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION (tab) characters. 
        while (position < input.length() && WebVTTParser::isValidSettingDelimiter(input[position]))
            position++;
        if (position >= input.length())
            break;

        // When the user agent is to parse the WebVTT settings given by a string input for a text track cue cue, 
        // the user agent must run the following steps:
        // 1. Let settings be the result of splitting input on spaces.
        // 2. For each token setting in the list settings, run the following substeps:
        //    1. If setting does not contain a U+003A COLON character (:), or if the first U+003A COLON character (:) 
        //       in setting is either the first or last character of setting, then jump to the step labeled next setting.
        unsigned endOfSetting = position;
        String setting = WebVTTParser::collectWord(input, &endOfSetting);
        CueSetting name;
        size_t colonOffset = setting.find(':', 1);
        if (colonOffset == notFound || colonOffset == 0 || colonOffset == setting.length() - 1)
            goto NextSetting;

        // 2. Let name be the leading substring of setting up to and excluding the first U+003A COLON character (:) in that string.
        name = settingName(setting.substring(0, colonOffset));

        // 3. Let value be the trailing substring of setting starting from the character immediately after the first U+003A COLON character (:) in that string.
        position += colonOffset + 1;
        if (position >= input.length())
            break;

        // 4. Run the appropriate substeps that apply for the value of name, as follows:
        switch (name) {
        case Vertical:
            {
            // If name is a case-sensitive match for "vertical"
            // 1. If value is a case-sensitive match for the string "rl", then let cue's text track cue writing direction 
            //    be vertical growing left.
            String writingDirection = WebVTTParser::collectWord(input, &position);
            if (writingDirection == verticalGrowingLeftKeyword())
                m_writingDirection = VerticalGrowingLeft;
            
            // 2. Otherwise, if value is a case-sensitive match for the string "lr", then let cue's text track cue writing 
            //    direction be vertical growing right.
            else if (writingDirection == verticalGrowingRightKeyword())
                m_writingDirection = VerticalGrowingRight;
            }
            break;
        case Line:
            {
            // 1-2 - Collect chars that are either '-', '%', or a digit.
            // 1. If value contains any characters other than U+002D HYPHEN-MINUS characters (-), U+0025 PERCENT SIGN 
            //    characters (%), and characters in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump
            //    to the step labeled next setting.
            StringBuilder linePositionBuilder;
            while (position < input.length() && (input[position] == '-' || input[position] == '%' || isASCIIDigit(input[position])))
                linePositionBuilder.append(input[position++]);
            if (position < input.length() && !WebVTTParser::isValidSettingDelimiter(input[position]))
                break;

            // 2. If value does not contain at least one character in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT 
            //    NINE (9), then jump to the step labeled next setting.
            // 3. If any character in value other than the first character is a U+002D HYPHEN-MINUS character (-), then 
            //    jump to the step labeled next setting.
            // 4. If any character in value other than the last character is a U+0025 PERCENT SIGN character (%), then
            //    jump to the step labeled next setting.
            String linePosition = linePositionBuilder.toString();
            if (linePosition.find('-', 1) != notFound || linePosition.reverseFind("%", linePosition.length() - 2) != notFound)
                break;

            // 5. If the first character in value is a U+002D HYPHEN-MINUS character (-) and the last character in value is a 
            //    U+0025 PERCENT SIGN character (%), then jump to the step labeled next setting.
            if (linePosition[0] == '-' && linePosition[linePosition.length() - 1] == '%')
                break;

            // 6. Ignoring the trailing percent sign, if any, interpret value as a (potentially signed) integer, and 
            //    let number be that number. 
            // NOTE: toInt ignores trailing non-digit characters, such as '%'.
            bool validNumber;
            int number = linePosition.toInt(&validNumber);
            if (!validNumber)
                break;

            // 7. If the last character in value is a U+0025 PERCENT SIGN character (%), but number is not in the range 
            //    0 ≤ number ≤ 100, then jump to the step labeled next setting.
            // 8. Let cue's text track cue line position be number.
            // 9. If the last character in value is a U+0025 PERCENT SIGN character (%), then let cue's text track cue 
            //    snap-to-lines flag be false. Otherwise, let it be true.
            if (linePosition[linePosition.length() - 1] == '%') {
                if (number < 0 || number > 100)
                    break;

                // 10 - If '%' then set snap-to-lines flag to false.
                m_snapToLines = false;
            }

            m_linePosition = number;
            }
            break;
        case Position:
            {
            // 1. If value contains any characters other than U+0025 PERCENT SIGN characters (%) and characters in the range 
            //    U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump to the step labeled next setting.
            // 2. If value does not contain at least one character in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9),
            //    then jump to the step labeled next setting.
            String textPosition = WebVTTParser::collectDigits(input, &position);
            if (textPosition.isEmpty())
                break;
            if (position >= input.length())
                break;

            // 3. If any character in value other than the last character is a U+0025 PERCENT SIGN character (%), then jump
            //    to the step labeled next setting.
            // 4. If the last character in value is not a U+0025 PERCENT SIGN character (%), then jump to the step labeled
            //    next setting.
            if (input[position++] != '%')
                break;
            if (position < input.length() && !WebVTTParser::isValidSettingDelimiter(input[position]))
                break;

            // 5. Ignoring the trailing percent sign, interpret value as an integer, and let number be that number.
            // 6. If number is not in the range 0 ≤ number ≤ 100, then jump to the step labeled next setting.
            // NOTE: toInt ignores trailing non-digit characters, such as '%'.
            bool validNumber;
            int number = textPosition.toInt(&validNumber);
            if (!validNumber)
                break;
            if (number < 0 || number > 100)
              break;

            // 7. Let cue's text track cue text position be number.
            m_textPosition = number;
            }
            break;
        case Size:
            {
            // 1. If value contains any characters other than U+0025 PERCENT SIGN characters (%) and characters in the
            //    range U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump to the step labeled next setting.
            // 2. If value does not contain at least one character in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT 
            //    NINE (9), then jump to the step labeled next setting.
            String cueSize = WebVTTParser::collectDigits(input, &position);
            if (cueSize.isEmpty())
                break;
            if (position >= input.length())
                break;

            // 3. If any character in value other than the last character is a U+0025 PERCENT SIGN character (%),
            //    then jump to the step labeled next setting.
            // 4. If the last character in value is not a U+0025 PERCENT SIGN character (%), then jump to the step
            //    labeled next setting.
            if (input[position++] != '%')
                break;
            if (position < input.length() && !WebVTTParser::isValidSettingDelimiter(input[position]))
                break;

            // 5. Ignoring the trailing percent sign, interpret value as an integer, and let number be that number.
            // 6. If number is not in the range 0 ≤ number ≤ 100, then jump to the step labeled next setting.
            bool validNumber;
            int number = cueSize.toInt(&validNumber);
            if (!validNumber)
                break;
            if (number < 0 || number > 100)
                break;

            // 7. Let cue's text track cue size be number.
            m_cueSize = number;
            }
            break;
        case Align:
            {
            String cueAlignment = WebVTTParser::collectWord(input, &position);

            // 1. If value is a case-sensitive match for the string "start", then let cue's text track cue alignment be start alignment.
            if (cueAlignment == startKeyword())
                m_cueAlignment = Start;

            // 2. If value is a case-sensitive match for the string "middle", then let cue's text track cue alignment be middle alignment.
            else if (cueAlignment == middleKeyword())
                m_cueAlignment = Middle;

            // 3. If value is a case-sensitive match for the string "end", then let cue's text track cue alignment be end alignment.
            else if (cueAlignment == endKeyword())
                m_cueAlignment = End;
            }
            break;
        case None:
            break;
        }

NextSetting:
        position = endOfSetting;
    }
}

int TextTrackCue::getCSSWritingMode() const
{
    return m_displayWritingMode;
}

int TextTrackCue::getCSSSize() const
{
    return m_displaySize;
}

std::pair<double, double> TextTrackCue::getCSSPosition() const
{
    if (!m_snapToLines)
        return getPositionCoordinates();

    return m_displayPosition;
}

const AtomicString& TextTrackCue::interfaceName() const
{
    return eventNames().interfaceForTextTrackCue;
}

ScriptExecutionContext* TextTrackCue::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

EventTargetData* TextTrackCue::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* TextTrackCue::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif
