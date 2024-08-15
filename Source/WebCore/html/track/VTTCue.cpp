/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#include "VTTCue.h"

#if ENABLE(VIDEO)

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "CommonAtomStrings.h"
#include "DocumentFragment.h"
#include "ElementInlines.h"
#include "Event.h"
#include "HTMLDivElement.h"
#include "HTMLSpanElement.h"
#include "HTMLStyleElement.h"
#include "JSVTTCue.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "RenderVTTCue.h"
#include "ScriptDisallowedScope.h"
#include "SpeechSynthesis.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueGeneric.h"
#include "TextTrackCueList.h"
#include "UserAgentParts.h"
#include "VTTRegionList.h"
#include "VTTScanner.h"
#include "WebVTTElement.h"
#include "WebVTTParser.h"
#include <wtf/Language.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(VTTCue);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(VTTCueBox);

static const CSSValueID displayWritingModeMap[] = {
    CSSValueHorizontalTb, CSSValueVerticalRl, CSSValueVerticalLr
};
static_assert(std::size(displayWritingModeMap) == static_cast<size_t>(WebCore::VTTDirectionSetting::MaxValue) + 1, "displayWritingModeMap has wrong size");

static const CSSValueID displayAlignmentMap[] = {
    CSSValueStart, CSSValueCenter, CSSValueEnd, CSSValueLeft, CSSValueRight
};
static_assert(std::size(displayAlignmentMap) == static_cast<size_t>(WebCore::VTTAlignSetting::MaxValue) + 1, "displayAlignmentMap has wrong size");

static const String& startKeyword()
{
    static NeverDestroyed<const String> start(MAKE_STATIC_STRING_IMPL("start"));
    return start;
}

static const String& centerKeyword()
{
    static NeverDestroyed<const String> center(MAKE_STATIC_STRING_IMPL("center"));
    return center;
}

static const String& endKeyword()
{
    static NeverDestroyed<const String> end(MAKE_STATIC_STRING_IMPL("end"));
    return end;
}

static const String& leftKeyword()
{
    static NeverDestroyed<const String> left(MAKE_STATIC_STRING_IMPL("left"));
    return left;
}

static const String& rightKeyword()
{
    static NeverDestroyed<const String> right(MAKE_STATIC_STRING_IMPL("right"));
    return right;
}

static const String& verticalGrowingLeftKeyword()
{
    static NeverDestroyed<const String> verticalrl(MAKE_STATIC_STRING_IMPL("rl"));
    return verticalrl;
}

static const String& verticalGrowingRightKeyword()
{
    static NeverDestroyed<const String> verticallr(MAKE_STATIC_STRING_IMPL("lr"));
    return verticallr;
}

static const String& lineLeftKeyword()
{
    static NeverDestroyed<const String> lineLeft(MAKE_STATIC_STRING_IMPL("line-left"));
    return lineLeft;
}

static const String& lineRightKeyword()
{
    static NeverDestroyed<const String> lineRight(MAKE_STATIC_STRING_IMPL("line-right"));
    return lineRight;
}

// ----------------------------

Ref<VTTCueBox> VTTCueBox::create(Document& document, VTTCue& cue)
{
    auto box = adoptRef(*new VTTCueBox(document, cue));
    box->initialize();
    return box;
}

VTTCueBox::VTTCueBox(Document& document, VTTCue& cue)
    : TextTrackCueBox(document, cue)
{
}

void VTTCueBox::applyCSSPropertiesWithRegion()
{
    auto textTrackCue = getCue();
    ASSERT(!textTrackCue || is<VTTCue>(textTrackCue));
    if (!textTrackCue)
        return;

    RefPtr cue = dynamicDowncast<VTTCue>(*textTrackCue);
    if (!cue)
        return;

    // the 'left' property must be set to left
    std::visit(WTF::makeVisitor([&] (double left) {
        setInlineStyleProperty(CSSPropertyLeft, left, CSSUnitType::CSS_PERCENTAGE);
    }, [&] (auto) {
        setInlineStyleProperty(CSSPropertyLeft, CSSValueAuto);
    }), cue->left());
    setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    setInlineStyleProperty(CSSPropertyTextAlign, cue->getCSSAlignment());

    // Section 7.4 states that the text track display should be abolustely positioned,
    // unless if it is the child of a region, then it is to be relatively positioned.
    setInlineStyleProperty(CSSPropertyPosition, CSSValueRelative);
}

void VTTCueBox::applyCSSProperties()
{
    auto textTrackCue = getCue();
    ASSERT(!textTrackCue || is<VTTCue>(textTrackCue));
    RefPtr cue = dynamicDowncast<VTTCue>(*textTrackCue);
    if (!cue)
        return;

    // https://w3c.github.io/webvtt/#applying-css-properties
    // 7.4. Applying CSS properties to WebVTT Node Objects

    // When following the rules for updating the display of WebVTT text tracks,
    // user agents must set properties of WebVTT Node Objects at the CSS user
    // agent cascade layer as defined in this section. [CSS22]

    // Initialize the (root) list of WebVTT Node Objects with the following CSS settings:
    // NOTE: The following settings are initialized by text-tracks.css:

    // the 'position' property must be set to 'absolute'
    // the 'unicode-bidi' property must be set to 'plaintext'
    // the overflow-wrap property must be set to break-word
    // the text-wrap property must be set to balance [CSS-TEXT-4]
    // The color property on the (root) list of WebVTT Node Objects must be set
    // to rgba(255,255,255,1). [CSS3-COLOR]
    // The background shorthand property on the WebVTT cue background box and
    // on WebVTT Ruby Text Objects must be set to rgba(0,0,0,0.8). [CSS3-COLOR]
    // The white-space property on the (root) list of WebVTT Node Objects must
    // be set to pre-line. [CSS22]

    // NOTE: For 'top' and 'left', see step 25. in 7.2, Processing Cue Settings:
    //   Let left be x-position vw and top be y-position vh.
    // Use 'cqh' and 'cqw' rather than 'vh' and 'vw' here, as the video viewport
    // is not a true viewport, but it is a container, so they serve the same purpose.

    // the 'writing-mode' property must be set to writing-mode
    setInlineStyleProperty(CSSPropertyWritingMode, cue->getCSSWritingMode());

    // the 'top' property must be set to top
    std::visit(WTF::makeVisitor([&] (double top) {
        setInlineStyleProperty(CSSPropertyTop, top, CSSUnitType::CSS_CQH);
    }, [&] (auto) {
        setInlineStyleProperty(CSSPropertyTop, CSSValueAuto);
    }), cue->top());

    // the 'left' property must be set to left
    std::visit(WTF::makeVisitor([&] (double left) {
        setInlineStyleProperty(CSSPropertyLeft, left, CSSUnitType::CSS_CQW);
    }, [&] (auto) {
        setInlineStyleProperty(CSSPropertyLeft, CSSValueAuto);
    }), cue->left());

    // NOTE: For 'width' and 'height', see step 8. in 7.2, Processing Cue Settings:
    //   If the WebVTT cue writing direction is horizontal, then let width be size
    //   vw and height be auto. Otherwise, let width be auto and height be size vh.
    // Use 'cqh' and 'cqw' rather than 'vh' and 'vw' here, as the video viewport
    // is not a true viewport, but it is a container, so they serve the same purpose.

    // the 'width' property must be set to width
    std::visit(WTF::makeVisitor([&] (double width) {
        setInlineStyleProperty(CSSPropertyWidth, width, CSSUnitType::CSS_CQW);
    }, [&] (auto) {
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);
    }), cue->width());

    // the 'height' property must be set to height
    std::visit(WTF::makeVisitor([&] (double height) {
        setInlineStyleProperty(CSSPropertyHeight, height, CSSUnitType::CSS_CQH);
    }, [&] (auto) {
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    }), cue->height());

    // The 'text-align' property on the (root) List of WebVTT Node Objects must
    // be set to the value in the second cell of the row of the table below
    // whose first cell is the value of the corresponding cue's WebVTT cue text
    // alignment:
    setInlineStyleProperty(CSSPropertyTextAlign, cue->getCSSAlignment());

    // Section 7.4 states that the text track display should be abolustely positioned,
    // unless if it is the child of a region, then it is to be relatively positioned.
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);

    // The font shorthand property on the (root) list of WebVTT Node Objects
    // must be set to 5vh sans-serif. [CSS-VALUES]
    // NOTE: We use 'cqh' rather than 'vh' as the video element is not a proper viewport.
    setInlineStyleProperty(CSSPropertyFontSize, cue->fontSize(), CSSUnitType::CSS_CQMIN, cue->fontSizeIsImportant() ? IsImportant::Yes : IsImportant::No);

    if (!cue->snapToLines()) {
        setInlineStyleProperty(CSSPropertyWhiteSpaceCollapse, CSSValuePreserve);
        setInlineStyleProperty(CSSPropertyTextWrapMode, CSSValueNowrap);
    }

    // Make sure shadow or stroke is not clipped.
    // NOTE: Set in text-tracks.css
}

RenderPtr<RenderElement> VTTCueBox::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderVTTCue>(*this, WTFMove(style));
}

// ----------------------------

Ref<VTTCue> VTTCue::create(Document& document, double start, double end, String&& content)
{
    auto cue = adoptRef(*new VTTCue(document, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), WTFMove(content)));
    cue->suspendIfNeeded();
    return cue;
}

Ref<VTTCue> VTTCue::create(Document& document, const WebVTTCueData& data)
{
    auto cue = adoptRef(*new VTTCue(document, data));
    cue->suspendIfNeeded();
    return cue;
}

VTTCue::VTTCue(Document& document, const MediaTime& start, const MediaTime& end, String&& content)
    : TextTrackCue(document, start, end)
    , m_content(WTFMove(content))
    , m_cueHighlightBox(HTMLSpanElement::create(spanTag, document))
    , m_cueBackdropBox(HTMLDivElement::create(document))
    , m_originalStartTime(MediaTime::zeroTime())
    , m_snapToLines(true)
    , m_displayTreeShouldChange(true)
    , m_notifyRegion(true)
#if !RELEASE_LOG_DISABLED
    , m_logger(&document.logger())
#endif
{
}

VTTCue::VTTCue(Document& document, const WebVTTCueData& cueData)
    : VTTCue(document, MediaTime::zeroTime(), MediaTime::zeroTime(), { })
{
    m_originalStartTime = cueData.originalStartTime();
    setText(cueData.content());
    setStartTime(cueData.startTime());
    setEndTime(cueData.endTime());
    setId(cueData.id());
    setCueSettings(cueData.settings());
}

VTTCue::~VTTCue()
{
}

RefPtr<VTTCueBox> VTTCue::createDisplayTree()
{
    if (auto* document = this->document())
        return VTTCueBox::create(*document, *this);
    return nullptr;
}

VTTCueBox* VTTCue::displayTreeInternal()
{
    if (!m_displayTree)
        m_displayTree = createDisplayTree();
    return m_displayTree.get();
}

void VTTCue::didChange(bool affectOrder)
{
    TextTrackCue::didChange(affectOrder);
    m_displayTreeShouldChange = true;
}

void VTTCue::setVertical(DirectionSetting direction)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-vertical
    // On setting, the text track cue writing direction must be set to the value given 
    // in the first cell of the row in the table above whose second cell is a 
    // case-sensitive match for the new value, if any. If none of the values match, then
    // the user agent must instead throw a SyntaxError exception.

    if (direction == m_writingDirection)
        return;

    willChange();
    m_writingDirection = direction;
    didChange();
}

void VTTCue::setSnapToLines(bool value)
{
    if (m_snapToLines == value)
        return;
    
    willChange();
    m_snapToLines = value;
    didChange();
}

VTTCue::LineAndPositionSetting VTTCue::line() const
{
    if (!m_linePosition)
        return Auto;

    return *m_linePosition;
}

void VTTCue::setLine(const LineAndPositionSetting& position)
{
    std::optional<double> linePosition;

    if (!std::holds_alternative<AutoKeyword>(position))
        linePosition = std::get<double>(position);

    if (m_linePosition == linePosition)
        return;

    willChange();
    m_linePosition = linePosition;
    m_computedLinePosition = calculateComputedLinePosition();
    didChange();
}

void VTTCue::setLineAlign(LineAlignSetting lineAlignment)
{
    if (lineAlignment == m_lineAlignment)
        return;

    willChange();
    m_lineAlignment = lineAlignment;
    didChange();
}


auto VTTCue::position() const -> LineAndPositionSetting
{
    if (m_textPosition)
        return *m_textPosition;
    return Auto;
}

ExceptionOr<void> VTTCue::setPosition(const LineAndPositionSetting& position)
{
    // http://dev.w3.org/html5/webvtt/#dfn-vttcue-position
    // On setting, if the new value is negative or greater than 100, then an
    // IndexSizeError exception must be thrown. Otherwise, the WebVTT cue
    // position must be set to the new value; if the new value is the string
    // "auto", then it must be interpreted as the special value auto.
    std::optional<double> textPosition;

    // Otherwise, set the text track cue line position to the new value.
    if (!std::holds_alternative<AutoKeyword>(position)) {
        textPosition = std::get<double>(position);
        if (!(textPosition >= 0 && textPosition <= 100))
            return Exception { ExceptionCode::IndexSizeError };
    }

    if (m_textPosition == textPosition)
        return { };

    willChange();
    m_textPosition = WTFMove(textPosition);
    didChange();

    return { };
}

void VTTCue::setPositionAlign(PositionAlignSetting positionAlignment)
{
    if (positionAlignment == m_positionAlignment)
        return;

    willChange();
    m_positionAlignment = positionAlignment;
    didChange();
}

ExceptionOr<void> VTTCue::setSize(double size)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-size
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError
    // exception. Otherwise, set the text track cue size to the new value.
    if (!(size >= 0 && size <= 100))
        return Exception { ExceptionCode::IndexSizeError };

    // Otherwise, set the text track cue line position to the new value.
    if (m_cueSize == size)
        return { };
    
    willChange();
    m_cueSize = size;
    didChange();

    return { };
}

void VTTCue::setAlign(AlignSetting alignment)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-align
    // On setting, the text track cue alignment must be set to the value given in the 
    // first cell of the row in the table above whose second cell is a case-sensitive
    // match for the new value, if any. If none of the values match, then the user
    // agent must instead throw a SyntaxError exception.

    if (alignment == m_cueAlignment)
        return;

    willChange();
    m_cueAlignment = alignment;
    didChange();
}
    
void VTTCue::setText(const String& text)
{
    if (m_content == text)
        return;
    
    willChange();
    // Clear the document fragment but don't bother to create it again just yet as we can do that
    // when it is requested.
    m_webVTTNodeTree = nullptr;
    m_content = text;
    didChange();
}

void VTTCue::createWebVTTNodeTree()
{
    if (!m_webVTTNodeTree && document())
        m_webVTTNodeTree = WebVTTParser::createDocumentFragmentFromCueText(*document(), m_content);
}

static void copyWebVTTNodeToDOMTree(ContainerNode& webVTTNode, Node& parent)
{
    for (RefPtr node = webVTTNode.firstChild(); node; node = node->nextSibling()) {
        RefPtr<Node> clonedNode;
        if (RefPtr element = dynamicDowncast<WebVTTElement>(*node))
            clonedNode = element->createEquivalentHTMLElement(parent.document());
        else
            clonedNode = node->cloneNode(false);
        parent.appendChild(*clonedNode);
        if (RefPtr containerNode = dynamicDowncast<ContainerNode>(*node))
            copyWebVTTNodeToDOMTree(*containerNode, *clonedNode);
    }
}

RefPtr<DocumentFragment> VTTCue::getCueAsHTML()
{
    createWebVTTNodeTree();
    if (!m_webVTTNodeTree)
        return nullptr;

    auto* document = this->document();
    if (!document)
        return nullptr;

    auto clonedFragment = DocumentFragment::create(*document);
    copyWebVTTNodeToDOMTree(*m_webVTTNodeTree, clonedFragment);
    return clonedFragment;
}

RefPtr<DocumentFragment> VTTCue::createCueRenderingTree()
{
    createWebVTTNodeTree();
    if (!m_webVTTNodeTree)
        return nullptr;

    auto* document = this->document();
    if (!document)
        return nullptr;

    auto clonedFragment = DocumentFragment::create(*document);

    // The cloned fragment is never exposed to author scripts so it's safe to dispatch events here.
    ScriptDisallowedScope::EventAllowedScope allowedScope(clonedFragment);

    m_webVTTNodeTree->cloneChildNodes(clonedFragment);
    return clonedFragment;
}

void VTTCue::notifyRegionWhenRemovingDisplayTree(bool notifyRegion)
{
    m_notifyRegion = notifyRegion;
}

void VTTCue::setIsActive(bool active)
{
    TextTrackCue::setIsActive(active);

    if (!active) {
        if (!hasDisplayTree())
            return;

        // Remove the display tree as soon as the cue becomes inactive.
        removeDisplayTree();
    }
}

void VTTCue::setTrack(TextTrack* track)
{
    TextTrackCue::setTrack(track);
    if (!m_parsedRegionId.isEmpty()) {
        if (track != nullptr) {
            if (auto* regions = track->regions()) {
                if (auto region = regions->getRegionById(m_parsedRegionId))
                    m_region = RefPtr<VTTRegion>(region);
            }
        }
    }
    INFO_LOG(LOGIDENTIFIER);
}

void VTTCue::setRegion(VTTRegion* region)
{
    if (m_region != region) {
        willChange();
        m_region = region;
        didChange();
    }
}

VTTRegion* VTTCue::region()
{
    if (!m_region)
        return nullptr;

    return &*m_region;
}

const String& VTTCue::regionId()
{
    if (!m_region)
        return emptyString();

    return m_region->id();
}

int VTTCue::calculateComputedLinePosition() const
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#text-track-cue-computed-line-position
    // A WebVTT cue has a computed line whose value is that returned by the following
    // algorithm, which is defined in terms of the other aspects of the cue:

    // 1. If the line is numeric, the WebVTT cue snap-to-lines flag of the WebVTT
    // cue is false, and the line is negative or greater than 100, then return 100
    // and abort these steps.
    // (Although the WebVTT parser will not set the line to a number outside the
    // range 0..100 and also set the WebVTT cue snap-to-lines flag to false, this
    // can happen when using the DOM API’s snapToLines and line attributes.)
    if (m_snapToLines && m_linePosition && (*m_linePosition < 0 && *m_linePosition > 100))
        return 100;

    // 2. If the line is numeric, return the value of the WebVTT cue line and abort
    // these steps. (Either the WebVTT cue snap-to-lines flag is true, so any value,
    // not just those in the range 0..100, is valid, or the value is in the range 0..100
    // and is thus valid regardless of the value of that flag.)
    if (m_linePosition)
        return *m_linePosition;

    // 3. If the WebVTT cue snap-to-lines flag of the WebVTT cue is false, return the value
    // 100 and abort these steps. (The line is the special value auto.)
    if (!m_snapToLines)
        return 100;

    // 4. Let cue be the WebVTT cue.
    // 5. If cue is not in a list of cues of a text track, or if that text track is not in
    // the list of text tracks of a media element, return −1 and abort these steps.
    if (!track())
        return -1;

    // 6. Let track be the text track whose list of cues the cue is in.
    // 7. Let n be the number of text tracks whose text track mode is showing and that are
    // in the media element’s list of text tracks before track.
    int n = track()->trackIndexRelativeToRenderedTracks();

    // 8. Increment n by one.
    n++;

    // 9. Negate n.
    n = -n;

    // 10. Return n.
    return n;
}

static bool isCueParagraphSeparator(UChar character)
{
    // Within a cue, paragraph boundaries are only denoted by Type B characters,
    // such as U+000A LINE FEED (LF), U+0085 NEXT LINE (NEL), and U+2029 PARAGRAPH SEPARATOR.
    return u_charType(character) == U_PARAGRAPH_SEPARATOR;
}

void VTTCue::determineTextDirection()
{
    static NeverDestroyed<const String> rtTag(MAKE_STATIC_STRING_IMPL("rt"));
    createWebVTTNodeTree();
    if (!m_webVTTNodeTree)
        return;

    // Apply the Unicode Bidirectional Algorithm's Paragraph Level steps to the
    // concatenation of the values of each WebVTT Text Object in nodes, in a
    // pre-order, depth-first traversal, excluding WebVTT Ruby Text Objects and
    // their descendants.
    StringBuilder paragraphBuilder;
    for (RefPtr<Node> node = m_webVTTNodeTree->firstChild(); node; node = NodeTraversal::next(*node, m_webVTTNodeTree.get())) {
        // FIXME: The code does not match the comment above. This does not actually exclude Ruby Text Object descendant.
        if (!node->isTextNode() || node->localName() == rtTag)
            continue;

        paragraphBuilder.append(node->nodeValue());
    }

    String paragraph = paragraphBuilder.toString();
    if (!paragraph.length())
        return;

    for (size_t i = 0; i < paragraph.length(); ++i) {
        UChar current = paragraph[i];
        if (!current || isCueParagraphSeparator(current))
            return;

        if (UChar current = paragraph[i]) {
            UCharDirection charDirection = u_charDirection(current);
            if (charDirection == U_LEFT_TO_RIGHT) {
                m_displayDirection = CSSValueLtr;
                return;
            }
            if (charDirection == U_RIGHT_TO_LEFT || charDirection == U_RIGHT_TO_LEFT_ARABIC) {
                m_displayDirection = CSSValueRtl;
                return;
            }
        }
    }
}

double VTTCue::calculateComputedTextPosition() const
{
    // http://dev.w3.org/html5/webvtt/#dfn-cue-computed-position
    
    // 1. If the position is numeric, then return the value of the position and
    // abort these steps. (Otherwise, the position is the special value auto.)
    if (m_textPosition)
        return *m_textPosition;
    
    switch (m_cueAlignment) {
    case AlignSetting::Start:
    case AlignSetting::Left:
        // 2. If the cue text alignment is start or left, return 0 and abort these
        // steps.
        return 0;
    case AlignSetting::End:
    case AlignSetting::Right:
        // 3. If the cue text alignment is end or right, return 100 and abort these
        // steps.
        return 100;
    case AlignSetting::Center:
        // 4. If the cue text alignment is center, return 50 and abort these steps.
        return 50;
    }

    // NOTE: The following is required for GCC, which will otherwise warn about
    // 'control reaches end of non-void function'.
    ASSERT_NOT_REACHED();
    return 0;
}

auto VTTCue::calculateComputedPositionAlignment() const -> PositionAlignSetting
{
    // https://w3c.github.io/webvtt/#cue-computed-position-alignment
    // A WebVTT cue has a computed position alignment whose value is that returned by
    // the following algorithm, which is defined in terms of other aspects of the cue:

    // 1. If the WebVTT cue position alignment is not auto, then return the
    //    value of the WebVTT cue position alignment and abort these steps.
    if (m_positionAlignment != PositionAlignSetting::Auto)
        return m_positionAlignment;

    switch (m_cueAlignment) {
    case AlignSetting::Left:
        // 2. If the WebVTT cue text alignment is left, return line-left and abort these steps.
        return PositionAlignSetting::LineLeft;
    case AlignSetting::Right:
        // 3. If the WebVTT cue text alignment is right, return line-right and abort these steps.
        return PositionAlignSetting::LineRight;
    case AlignSetting::Start:
        // 4. If the WebVTT cue text alignment is start, return line-left if the base direction
        //    of the cue text is left-to-right, line-right otherwise.
        return m_displayDirection == CSSValueLtr ? PositionAlignSetting::LineLeft : PositionAlignSetting::LineRight;
    case AlignSetting::End:
        // 5. If the WebVTT cue text alignment is end, return line-right if the base direction
        //    of the cue text is left-to-right, line-left otherwise.
        return m_displayDirection == CSSValueLtr ? PositionAlignSetting::LineRight : PositionAlignSetting::LineLeft;
    case AlignSetting::Center:
        // 6. Otherwise, return center.
        return PositionAlignSetting::Center;
    }

    // NOTE: The following is required for GCC, which will otherwise warn about
    // 'control reaches end of non-void function'.
    ASSERT_NOT_REACHED();
    return PositionAlignSetting::Center;
}

double VTTCue::calculateMaximumSize() const
{
    // 7.2. Processing cue settings
    // 7.2.2 Determine the value of maximum size for cue as per the appropriate rules from
    // the following list:

    double maxSize = 0;
    auto computedPosition = calculateComputedTextPosition();
    auto positionAlignment = calculateComputedPositionAlignment();

    if (positionAlignment == PositionAlignSetting::LineLeft) {
        // If the computed position alignment is line-left
        // Let maximum size be the computed position subtracted from 100.
        maxSize = 100.0 - computedPosition;
    } else if (positionAlignment == PositionAlignSetting::LineRight) {
        // If the computed position alignment is line-right
        // Let maximum size be the computed position.
        maxSize = computedPosition;
    } else if (positionAlignment == PositionAlignSetting::Center && computedPosition <= 50) {
        // If the computed position alignment is center, and the computed position is less than or equal to 50
        // Let maximum size be the computed position multiplied by two.
        maxSize = 2 * computedPosition;
        // If the computed position alignment is center, and the computed position is greater than 50
    } else if (positionAlignment == PositionAlignSetting::Center && computedPosition > 50) {
        // Let maximum size be the result of subtracting computed position from 100 and then multiplying the result by two.
        maxSize = 2 * (100.0 - computedPosition);
    } else
        ASSERT_NOT_REACHED();

    return maxSize;
}

void VTTCue::calculateDisplayParametersWithRegion()
{
    // 1. Let region be cue’s WebVTT cue region.
    ASSERT(region());

    // 2. If region’s WebVTT region scroll setting is up and region already has one child,
    // set region’s transition-property to top and transition-duration to 0.433s.

    // 3. Let offset be cue’s computed position multiplied by region’s WebVTT region width
    // and divided by 100 (i.e. interpret it as a percentage of the region width).
    double regionWidth = region()->width();
    double offset = calculateComputedTextPosition() * regionWidth / 100;

    // 4. Adjust offset using cue’s computed position alignment as follows:

    // 5. If the computed position alignment is center alignment
    // Subtract half of region’s WebVTT region width from offset.
    auto computedPositionAlignment = calculateComputedPositionAlignment();
    if (computedPositionAlignment == PositionAlignSetting::Center)
        offset = offset - regionWidth / 2;

    // 6. If the computed position alignment is line-right alignment
    // Subtract region’s WebVTT region width from offset.
    else if (computedPositionAlignment == PositionAlignSetting::LineRight)
        offset = offset - regionWidth;

    // 7. Let left be offset %. [CSS-VALUES]
    m_left = offset;
}
void VTTCue::calculateDisplayParameters()
{
    // https://w3c.github.io/webvtt/#processing-cue-settings
    // Steps 7.2.1-25
    determineTextDirection();

    // 1. If the WebVTT cue writing direction is horizontal, then let writing-mode be
    // "horizontal-tb". Otherwise, if the WebVTT cue writing direction is vertical growing
    // left, then let writing-mode be "vertical-rl". Otherwise, the WebVTT cue writing direction
    // is vertical growing right; let writing-mode be "vertical-lr".

    // The above step is done through the writing direction static map.

    // 2. Determine the value of maximum size for cue as per the appropriate
    // rules from the following list:
    // NOTE: Defined in calculateMaximumSize()
    double maximumSize = calculateMaximumSize();

    // 7. If the WebVTT cue size is less than maximum size, then let size be WebVTT cue size.
    // Otherwise, let size be maximum size.
    m_displaySize = std::min(m_cueSize, maximumSize);

    // 8. If the WebVTT cue writing direction is horizontal, then let width be size vw and height
    // be auto. Otherwise, let width be auto and height be size vh.
    if (m_writingDirection == DirectionSetting::Horizontal) {
        m_width = m_displaySize;
        m_height = Auto;
    } else {
        m_width = Auto;
        m_height = m_displaySize;
    }

    double computedTextPosition = calculateComputedTextPosition();
    auto computedPositionAlignment = calculateComputedPositionAlignment();

    // 9. Determine the value of x-position or y-position for cue as per the
    // appropriate rules from the following list:
    if (m_writingDirection == DirectionSetting::Horizontal) {
        switch (computedPositionAlignment) {
        case PositionAlignSetting::LineLeft:
            // Let x-position be the computed position.
            m_displayPosition.first = computedTextPosition;
            break;
        case PositionAlignSetting::Center:
            // Let x-position be the computed position minus half of size.
            m_displayPosition.first = computedTextPosition - m_displaySize / 2;
            break;
        case PositionAlignSetting::LineRight:
            // Let x-position be the computed position minus size.
            m_displayPosition.first = computedTextPosition - m_displaySize;
            break;
        case PositionAlignSetting::Auto:
            ASSERT_NOT_REACHED();
            break;
        }
    } else if (m_writingDirection == DirectionSetting::VerticalGrowingLeft || m_writingDirection == DirectionSetting::VerticalGrowingRight) {
        switch (computedPositionAlignment) {
        case PositionAlignSetting::LineLeft:
            // Let y-position be the computed position.
            m_displayPosition.second = computedTextPosition;
            break;
        case PositionAlignSetting::Center:
            // Let y-position be the computed position minus half of size.
            m_displayPosition.second = computedTextPosition - m_displaySize / 2;
            break;
        case PositionAlignSetting::LineRight:
            // Let y-position be the computed position minus size.
            m_displayPosition.second = computedTextPosition - m_displaySize;
            break;
        case PositionAlignSetting::Auto:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    // A text track cue has a text track cue computed line position whose value
    // is defined in terms of the other aspects of the cue.
    m_computedLinePosition = calculateComputedLinePosition();

    // 10. Determine the value of whichever of x-position or y-position is not
    // yet calculated for cue as per the appropriate rules from the following
    // list:
    if (!m_snapToLines) {
        if (m_writingDirection == DirectionSetting::Horizontal) {
            // Let y-position be the computed line.
            m_displayPosition.second = *m_computedLinePosition;
        } else if (m_writingDirection == DirectionSetting::VerticalGrowingLeft || m_writingDirection == DirectionSetting::VerticalGrowingRight) {
            // Let x-position be the computed line.
            m_displayPosition.first = *m_computedLinePosition;
        }
    } else {
        if (m_writingDirection == DirectionSetting::Horizontal) {
            // Let y-position 0.
            m_displayPosition.second = 0;
        } else if (m_writingDirection == DirectionSetting::VerticalGrowingLeft || m_writingDirection == DirectionSetting::VerticalGrowingRight) {
            // Let x-position 0.
            m_displayPosition.first = 0;
        }
    }

    // 25. Let left be x-position vw and top be y-position vh.
    ASSERT(m_displayPosition.first);
    ASSERT(m_displayPosition.second);
    m_left = m_displayPosition.first.value_or(0);
    m_top = m_displayPosition.second.value_or(0);
}

void VTTCue::obtainCSSBoxes()
{
    // https://w3c.github.io/webvtt/#obtaining-css-boxes
    // 7.3 Obtaining CSS Boxes
    // When the processing algorithm above requires that the user agent obtain a set of
    // CSS boxes 'boxes', then apply the terms of the CSS specifications to nodes within
    // the following constraints:
    RefPtr displayTree = displayTreeInternal();
    if (!displayTree)
        return;

    displayTree->removeChildren();

    // The document tree is the tree of WebVTT Node Objects rooted at nodes.

    // The children of the nodes must be wrapped in an anonymous box whose
    // 'display' property has the value 'inline'. This is the WebVTT cue
    // background box.

    // Note: This is contained by default in m_cueHighlightBox.
    displayTree->setUserAgentPart(UserAgentParts::cue());
    m_cueHighlightBox->setUserAgentPart(UserAgentParts::internalCueBackground());

    m_cueBackdropBox->setUserAgentPart(UserAgentParts::webkitMediaTextTrackDisplayBackdrop());
    m_cueBackdropBox->appendChild(m_cueHighlightBox);
    displayTree->appendChild(m_cueBackdropBox);

    // FIXME(BUG 79916): Runs of children of WebVTT Ruby Objects that are not
    // WebVTT Ruby Text Objects must be wrapped in anonymous boxes whose
    // 'display' property has the value 'ruby-base'.
    if (region())
        displayTree->applyCSSPropertiesWithRegion();
    else
        displayTree->applyCSSProperties();

    if (displayTree->document().page()) {
        auto cssString = displayTree->document().page()->captionUserPreferencesStyleSheet();
        auto style = HTMLStyleElement::create(HTMLNames::styleTag, displayTree->document(), false);
        style->setTextContent(WTFMove(cssString));
        displayTree->appendChild(WTFMove(style));
    }

    if (const auto& styleSheets = track()->styleSheets()) {
        for (const auto& cssString : *styleSheets) {
            auto style = HTMLStyleElement::create(HTMLNames::styleTag, displayTree->document(), false);
            style->setTextContent(String { cssString });
            displayTree->appendChild(WTFMove(style));
        }
    }
}

void VTTCue::markFutureAndPastNodes(ContainerNode* root, const MediaTime& previousTimestamp, const MediaTime& movieTime)
{
    static NeverDestroyed<const String> timestampTag(MAKE_STATIC_STRING_IMPL("timestamp"));
    
    bool isPastNode = true;
    MediaTime currentTimestamp = previousTimestamp;
    if (currentTimestamp > movieTime)
        isPastNode = false;
    
    for (RefPtr<Node> child = root->firstChild(); child; child = NodeTraversal::next(*child, root)) {
        if (child->nodeName() == timestampTag) {
            MediaTime currentTimestamp;
            bool check = WebVTTParser::collectTimeStamp(child->nodeValue(), currentTimestamp);
            ASSERT_UNUSED(check, check);
            
            currentTimestamp += m_originalStartTime;
            if (currentTimestamp > movieTime)
                isPastNode = false;
        }
        
        if (auto* childElement = dynamicDowncast<WebVTTElement>(*child))
            childElement->setIsPastNode(isPastNode);

        // Make an element id match a cue id for style matching purposes.
        if (auto* childElement = dynamicDowncast<Element>(*child); !id().isEmpty() && childElement)
            childElement->setIdAttribute(id());
    }
}

void VTTCue::updateDisplayTree(const MediaTime& movieTime)
{
    // The display tree may contain WebVTT timestamp objects representing
    // timestamps (processing instructions), along with displayable nodes.

    if (!track() || !track()->isRendered())
        return;

    // Mutating the VTT contents is safe because it's never exposed to author scripts.
    ScriptDisallowedScope::EventAllowedScope allowedScopeForCueHighlightBox(m_cueHighlightBox);

    // Clear the contents of the set.
    m_cueHighlightBox->removeChildren();

    // Update the two sets containing past and future WebVTT objects.
    RefPtr<DocumentFragment> referenceTree = createCueRenderingTree();
    if (!referenceTree)
        return;

    ScriptDisallowedScope::EventAllowedScope allowedScopeForReferenceTree(*referenceTree);

    markFutureAndPastNodes(referenceTree.get(), startMediaTime(), movieTime);
    m_cueHighlightBox->appendChild(*referenceTree);
}

RefPtr<TextTrackCueBox> VTTCue::getDisplayTree()
{
    ASSERT(track());

    RefPtr displayTree = displayTreeInternal();
    if (!displayTree || !m_displayTreeShouldChange || !track() || !track()->isRendered())
        return displayTree;

    if (region())
        calculateDisplayParametersWithRegion();
    else {
        // https://w3c.github.io/webvtt/#processing-cue-settings
        // 7.2. Processing cue settings
        // Steps 1-25:
        calculateDisplayParameters();
    }

    // 26. Obtain a set of CSS boxes boxes positioned relative to an initial containing block.
    obtainCSSBoxes();

    // Steps 27-31 performed by RenderVTTCue.
    return displayTreeInternal();
}

void VTTCue::removeDisplayTree()
{
    if (!hasDisplayTree())
        return;

    // The region needs to be informed about the cue removal.
    if (m_notifyRegion && track()) {
        if (m_region && m_displayTree)
            m_region->willRemoveTextTrackCueBox(m_displayTree.get());
    }

    RefPtr displayTree = displayTreeInternal();
    if (!displayTree)
        return;

    // The display tree is never exposed to author scripts so it's safe to dispatch events here.
    ScriptDisallowedScope::EventAllowedScope allowedScope(*displayTree);
    displayTree->remove();
}

std::pair<double, double> VTTCue::getPositionCoordinates() const
{
    // This method is used for setting x and y when snap to lines is not set.
    std::pair<double, double> coordinates;

    auto textPosition = calculateComputedTextPosition();
    auto computedLinePosition = m_computedLinePosition ? *m_computedLinePosition : calculateComputedLinePosition();
    
    if (m_writingDirection == DirectionSetting::Horizontal && m_displayDirection == CSSValueLtr) {
        coordinates.first = textPosition;
        coordinates.second = computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == DirectionSetting::Horizontal && m_displayDirection == CSSValueRtl) {
        coordinates.first = 100 - textPosition;
        coordinates.second = computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == DirectionSetting::VerticalGrowingLeft) {
        coordinates.first = 100 - *m_computedLinePosition;
        coordinates.second = textPosition;

        return coordinates;
    }

    if (m_writingDirection == DirectionSetting::VerticalGrowingRight) {
        coordinates.first = computedLinePosition;
        coordinates.second = textPosition;

        return coordinates;
    }

    ASSERT_NOT_REACHED();

    return coordinates;
}

VTTCue::CueSetting VTTCue::settingName(VTTScanner& input)
{
    CueSetting parsedSetting = None;
    if (input.scan("vertical"))
        parsedSetting = Vertical;
    else if (input.scan("line"))
        parsedSetting = Line;
    else if (input.scan("position"))
        parsedSetting = Position;
    else if (input.scan("size"))
        parsedSetting = Size;
    else if (input.scan("align"))
        parsedSetting = Align;
    else if (input.scan("region"))
        parsedSetting = Region;

    // Verify that a ':' follows.
    if (parsedSetting != None && input.scan(':'))
        return parsedSetting;

    return None;
}

void VTTCue::setCueSettings(const String& inputString)
{
    if (inputString.isEmpty())
        return;

    VTTScanner input(inputString);

    while (!input.isAtEnd()) {

        // The WebVTT cue settings part of a WebVTT cue consists of zero or more of the following components, in any order, 
        // separated from each other by one or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION (tab) characters.
        input.skipWhile<isTabOrSpace>();
        if (input.isAtEnd())
            break;

        // When the user agent is to parse the WebVTT settings given by a string input for a text track cue cue, 
        // the user agent must run the following steps:
        // 1. Let settings be the result of splitting input on spaces.
        // 2. For each token setting in the list settings, run the following substeps:
        //    1. If setting does not contain a U+003A COLON character (:), or if the first U+003A COLON character (:) 
        //       in setting is either the first or last character of setting, then jump to the step labeled next setting.
        //    2. Let name be the leading substring of setting up to and excluding the first U+003A COLON character (:) in that string.
        CueSetting name = settingName(input);

        // 3. Let value be the trailing substring of setting starting from the character immediately after the first U+003A COLON character (:) in that string.
        VTTScanner::Run valueRun = input.collectUntil<isTabOrSpace>();

        // 4. Run the appropriate substeps that apply for the value of name, as follows:
        switch (name) {
        case Vertical: {
            // If name is a case-sensitive match for "vertical"
            // 1. If value is a case-sensitive match for the string "rl", then let cue's text track cue writing direction
            //    be vertical growing left.
            if (input.scanRun(valueRun, verticalGrowingLeftKeyword()))
                m_writingDirection = DirectionSetting::VerticalGrowingLeft;

            // 2. Otherwise, if value is a case-sensitive match for the string "lr", then let cue's text track cue writing
            //    direction be vertical growing right.
            else if (input.scanRun(valueRun, verticalGrowingRightKeyword()))
                m_writingDirection = DirectionSetting::VerticalGrowingRight;

            else
                ERROR_LOG(LOGIDENTIFIER, "Invalid vertical");
            break;
        }
        case Line: {
            bool isValid = false;
            do {
                // 1-2 - Collect chars that are either '-', '%', or a digit.
                // 1. If value contains any characters other than U+002D HYPHEN-MINUS characters (-), U+0025 PERCENT SIGN
                //    characters (%), and characters in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump
                //    to the step labeled next setting.
                float linePosition;
                bool isNegative;
                if (!input.scanFloat(linePosition, &isNegative))
                    break;

                LineAlignSetting alignment { LineAlignSetting::Start };
                bool isPercentage = input.scan('%');
                if (!input.isAt(valueRun.end())) {
                    if (!input.scan(','))
                        break;

                    if (input.scan(startKeyword().span8()))
                        alignment = LineAlignSetting::Start;
                    else if (input.scan(centerKeyword().span8()))
                        alignment = LineAlignSetting::Center;
                    else if (input.scan(endKeyword().span8()))
                        alignment = LineAlignSetting::End;
                    else {
                        ERROR_LOG(LOGIDENTIFIER, "Invalid line setting alignment");
                        break;
                    }
                }

                // 2. If value does not contain at least one character in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT
                //    NINE (9), then jump to the step labeled next setting.
                // 3. If any character in value other than the first character is a U+002D HYPHEN-MINUS character (-), then
                //    jump to the step labeled next setting.
                // 4. If any character in value other than the last character is a U+0025 PERCENT SIGN character (%), then
                //    jump to the step labeled next setting.
                // 5. If the first character in value is a U+002D HYPHEN-MINUS character (-) and the last character in value is a
                //    U+0025 PERCENT SIGN character (%), then jump to the step labeled next setting.
                if (isPercentage && isNegative)
                    break;

                // 6. Ignoring the trailing percent sign, if any, interpret value as a (potentially signed) integer, and
                //    let number be that number.
                // 7. If the last character in value is a U+0025 PERCENT SIGN character (%), but number is not in the range
                //    0 ≤ number ≤ 100, then jump to the step labeled next setting.
                // 8. Let cue's text track cue line position be number.
                // 9. If the last character in value is a U+0025 PERCENT SIGN character (%), then let cue's text track cue
                //    snap-to-lines flag be false. Otherwise, let it be true.
                if (isPercentage) {
                    if (linePosition < 0 || linePosition > 100)
                        break;

                    // 10 - If '%' then set snap-to-lines flag to false.
                    m_snapToLines = false;
                } else {
                    if (linePosition - static_cast<int>(linePosition))
                        break;

                    m_snapToLines = true;
                }
                
                m_linePosition = linePosition;
                m_lineAlignment = alignment;
                isValid = true;
            } while (0);

            if (!isValid)
                ERROR_LOG(LOGIDENTIFIER, "Invalid line");

            break;
        }
        case Position: {
            float position;
            PositionAlignSetting alignment { PositionAlignSetting::Auto };

            auto parsePosition = [&] (VTTScanner& input, auto end, float& position, auto& alignment) -> bool {
                // 1. a position value consisting of: a WebVTT percentage.
                if (!WebVTTParser::parseFloatPercentageValue(input, position)) {
                    ERROR_LOG(LOGIDENTIFIER, "Invalid position percentage");
                    return false;
                }

                // 2. an optional alignment value consisting of:
                if (input.isAt(end))
                    return true;

                // 2.1 A U+002C COMMA character (,).
                if (!input.scan(','))
                    return false;

                // 2.2 One of the following strings: "line-left", "center", "line-right"
                if (input.scan(lineLeftKeyword().span8()))
                    alignment = PositionAlignSetting::LineLeft;
                else if (input.scan(centerKeyword().span8()))
                    alignment = PositionAlignSetting::Center;
                else if (input.scan(lineRightKeyword().span8()))
                    alignment = PositionAlignSetting::LineRight;
                else {
                    ERROR_LOG(LOGIDENTIFIER, "Invalid position setting alignment");
                    return false;
                }

                return true;
            };

            if (!parsePosition(input, valueRun.end(), position, alignment))
                break;

            m_textPosition = position;
            m_positionAlignment = alignment;
            break;
        }
        case Size: {
            float cueSize;
            if (WebVTTParser::parseFloatPercentageValue(input, cueSize) && input.isAt(valueRun.end()))
                m_cueSize = cueSize;
            else
                ERROR_LOG(LOGIDENTIFIER, "Invalid size");
            break;
        }
        case Align: {
            // 1. If value is a case-sensitive match for the string "start", then let cue's text track cue alignment be start alignment.
            if (input.scanRun(valueRun, startKeyword()))
                m_cueAlignment = AlignSetting::Start;

            // 2. If value is a case-sensitive match for the string "center", then let cue's text track cue alignment be center alignment.
            else if (input.scanRun(valueRun, centerKeyword()))
                m_cueAlignment = AlignSetting::Center;

            // 3. If value is a case-sensitive match for the string "end", then let cue's text track cue alignment be end alignment.
            else if (input.scanRun(valueRun, endKeyword()))
                m_cueAlignment = AlignSetting::End;

            // 4. If value is a case-sensitive match for the string "left", then let cue's text track cue alignment be left alignment.
            else if (input.scanRun(valueRun, leftKeyword()))
                m_cueAlignment = AlignSetting::Left;

            // 5. If value is a case-sensitive match for the string "right", then let cue's text track cue alignment be right alignment.
            else if (input.scanRun(valueRun, rightKeyword()))
                m_cueAlignment = AlignSetting::Right;

            else
                ERROR_LOG(LOGIDENTIFIER, "Invalid align");

            break;
        }
        case Region: {
            m_parsedRegionId = input.extractString(valueRun);
            break;
        }
        case None:
            break;
        }

        // Make sure the entire run is consumed.
        input.skipRun(valueRun);
    }

}

CSSValueID VTTCue::getCSSAlignment() const
{
    return displayAlignmentMap[static_cast<size_t>(m_cueAlignment)];
}

CSSValueID VTTCue::getCSSWritingDirection() const
{
    return m_displayDirection;
}

CSSValueID VTTCue::getCSSWritingMode() const
{
    return displayWritingModeMap[static_cast<size_t>(m_writingDirection)];
}

int VTTCue::getCSSSize() const
{
    return m_displaySize;
}

bool VTTCue::cueContentsMatch(const TextTrackCue& otherTextTrackCue) const
{
    auto& other = downcast<VTTCue>(otherTextTrackCue);
    return TextTrackCue::cueContentsMatch(other)
        && text() == other.text()
        && cueSettings() == other.cueSettings()
        && position() == other.position()
        && line() == other.line()
        && size() == other.size()
        && align() == other.align();
}

void VTTCue::setFontSize(int fontSize, bool important)
{
    if (fontSize == m_fontSize && important == m_fontSizeIsImportant)
        return;

    m_displayTreeShouldChange = true;
    m_fontSizeIsImportant = important;
    m_fontSize = fontSize;
}

void VTTCue::toJSON(JSON::Object& object) const
{
    TextTrackCue::toJSON(object);

    object.setString("text"_s, text());
    object.setString("vertical"_s, convertEnumerationToString(vertical()));
    object.setBoolean("snapToLines"_s, snapToLines());
    if (m_linePosition)
        object.setDouble("line"_s, *m_linePosition);
    else
        object.setString("line"_s, autoAtom());
    if (m_textPosition)
        object.setDouble("position"_s, *m_textPosition);
    else
        object.setString("position"_s, autoAtom());
    object.setInteger("size"_s, m_cueSize);
    object.setString("align"_s, convertEnumerationToString(align()));
}

#if ENABLE(SPEECH_SYNTHESIS)
static float mapVideoRateToSpeechRate(float rate)
{
    // WebSpeech says to go from .1 -> 10 (default 1)
    // Video rate is 0 -> 2 (default 1). [The spec has no maximum rate, but the default controls only go to 2x, so use that]
    // 1 -> (1 + (0 * 9)) => 1
    // 2 -> (1 + (1 * 9)) => 10
    if (rate > 1)
        rate = 1.0 + ((rate - 1.0) * (10 - 1));

    return rate;
}
#endif

void VTTCue::prepareToSpeak(SpeechSynthesis& speechSynthesis, double rate, double volume, SpeakCueCompletionHandler&& completion)
{
#if ENABLE(SPEECH_SYNTHESIS)
    ASSERT(!m_speechSynthesis);
    ASSERT(!m_speechUtterance);
    ASSERT(rate);
    ASSERT(track());
    if (m_content.isEmpty() || !track()) {
        completion(*this);
        return;
    }

    Ref track = *this->track();
    m_speechSynthesis = &speechSynthesis;
    m_speechUtterance = SpeechSynthesisUtterance::create(Ref { *track->scriptExecutionContext() }, m_content, [protectedThis = Ref { *this }, completion = WTFMove(completion)](const SpeechSynthesisUtterance&) {
        protectedThis->m_speechUtterance = nullptr;
        protectedThis->m_speechSynthesis = nullptr;
        completion(protectedThis.get());
    });

    auto trackLanguage = track->validBCP47Language();
    if (trackLanguage.isEmpty())
        trackLanguage = track->language();

    m_speechUtterance->setLang(trackLanguage);
    m_speechUtterance->setVolume(volume);
    m_speechUtterance->setRate(mapVideoRateToSpeechRate(rate));
#else
    UNUSED_PARAM(speechSynthesis);
    UNUSED_PARAM(rate);
    UNUSED_PARAM(volume);
    UNUSED_PARAM(completion);
#endif
}

#if !RELEASE_LOG_DISABLED
const void* VTTCue::logIdentifier() const
{
    if (!m_logIdentifier && track())
        m_logIdentifier = childLogIdentifier(track()->logIdentifier(), cryptographicallyRandomNumber<uint64_t>());
    return m_logIdentifier;
}

WTFLogChannel& VTTCue::logChannel() const
{
    return LogMedia;
}
#endif // !RELEASE_LOG_DISABLED

void VTTCue::beginSpeaking()
{
#if ENABLE(SPEECH_SYNTHESIS)
    ASSERT(m_speechSynthesis);
    ASSERT(m_speechUtterance);

    if (m_speechSynthesis->paused())
        m_speechSynthesis->resume();
    else
        m_speechSynthesis->speak(*m_speechUtterance);
#endif
}

void VTTCue::pauseSpeaking()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (!m_speechSynthesis)
        return;

    m_speechSynthesis->pause();
#endif
}

void VTTCue::cancelSpeaking()
{
#if ENABLE(SPEECH_SYNTHESIS)
    if (!m_speechSynthesis)
        return;

    m_speechSynthesis->cancel();
    m_speechSynthesis = nullptr;
    m_speechUtterance = nullptr;
#endif
}

} // namespace WebCore

#endif
