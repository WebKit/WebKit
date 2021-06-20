/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#include "DocumentFragment.h"
#include "Event.h"
#include "HTMLDivElement.h"
#include "HTMLSpanElement.h"
#include "HTMLStyleElement.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "RenderVTTCue.h"
#include "ScriptDisallowedScope.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueGeneric.h"
#include "TextTrackCueList.h"
#include "VTTRegionList.h"
#include "VTTScanner.h"
#include "WebVTTElement.h"
#include "WebVTTParser.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(VTTCue);
WTF_MAKE_ISO_ALLOCATED_IMPL(VTTCueBox);

// This constant should correspond with the percentage returned by CaptionUserPreferences::captionFontSizeScaleAndImportance.
constexpr double DEFAULTCAPTIONFONTSIZEPERCENTAGE = 5;

static const CSSValueID displayWritingModeMap[] = {
    CSSValueHorizontalTb, CSSValueVerticalRl, CSSValueVerticalLr
};
COMPILE_ASSERT(WTF_ARRAY_LENGTH(displayWritingModeMap) == VTTCue::NumberOfWritingDirections, displayWritingModeMap_has_wrong_size);

static const CSSValueID displayAlignmentMap[] = {
    CSSValueStart, CSSValueCenter, CSSValueEnd, CSSValueLeft, CSSValueRight
};
COMPILE_ASSERT(WTF_ARRAY_LENGTH(displayAlignmentMap) == VTTCue::NumberOfAlignments, displayAlignmentMap_has_wrong_size);

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

static const String& horizontalKeyword()
{
    return emptyString();
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

static const String& autoKeyword()
{
    static NeverDestroyed<const String> autoX(MAKE_STATIC_STRING_IMPL("auto"));
    return autoX;
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

void VTTCueBox::applyCSSProperties(const IntSize& videoSize)
{
    auto textTrackCue = getCue();
    ASSERT(!textTrackCue || is<VTTCue>(textTrackCue));
    if (!is<VTTCue>(textTrackCue))
        return;

    auto cue = makeRef(downcast<VTTCue>(*textTrackCue));

    // FIXME: Apply all the initial CSS positioning properties. http://wkb.ug/79916
    if (!cue->regionId().isEmpty()) {
        setInlineStyleProperty(CSSPropertyPosition, CSSValueRelative);
        return;
    }

    // 3.5.1 On the (root) List of WebVTT Node Objects:

    // the 'position' property must be set to 'absolute'
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);

    //  the 'unicode-bidi' property must be set to 'plaintext'
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValuePlaintext);

    // the 'direction' property must be set to direction
    setInlineStyleProperty(CSSPropertyDirection, cue->getCSSWritingDirection());

    // the 'writing-mode' property must be set to writing-mode
    setInlineStyleProperty(CSSPropertyWritingMode, cue->getCSSWritingMode(), false);

    auto position = cue->getCSSPosition();

    // the 'top' property must be set to top,
    setInlineStyleProperty(CSSPropertyTop, position.second, CSSUnitType::CSS_PERCENTAGE);

    // the 'left' property must be set to left
    if (cue->vertical() == horizontalKeyword())
        setInlineStyleProperty(CSSPropertyLeft, position.first, CSSUnitType::CSS_PERCENTAGE);
    else if (cue->vertical() == verticalGrowingRightKeyword()) {
        // FIXME: Why use calc to do the math instead of doing the subtraction here?
        setInlineStyleProperty(CSSPropertyLeft, makeString("calc(-", videoSize.width(), "px - ", cue->getCSSSize(), "px)"));
    }

    double authorFontSize = std::min(videoSize.width(), videoSize.height()) * DEFAULTCAPTIONFONTSIZEPERCENTAGE / 100.0;
    double multiplier = 1.0;
    if (authorFontSize)
        multiplier = m_fontSizeFromCaptionUserPrefs / authorFontSize;

    double textPosition = cue->calculateComputedTextPosition();
    double maxSize = 100.0;
    CSSValueID alignment = cue->getCSSAlignment();
    if (alignment == CSSValueEnd || alignment == CSSValueRight)
        maxSize = textPosition;
    else if (alignment == CSSValueStart || alignment == CSSValueLeft)
        maxSize = 100.0 - textPosition;

    double newCueSize = std::min(cue->getCSSSize() * multiplier, 100.0);
    // the 'width' property must be set to width, and the 'height' property  must be set to height
    if (cue->vertical() == horizontalKeyword()) {
        setInlineStyleProperty(CSSPropertyWidth, newCueSize, CSSUnitType::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
        setInlineStyleProperty(CSSPropertyMinWidth, "min-content");
        setInlineStyleProperty(CSSPropertyMaxWidth, maxSize, CSSUnitType::CSS_PERCENTAGE);
        if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
            setInlineStyleProperty(CSSPropertyLeft, static_cast<double>(position.first - (newCueSize - cue->getCSSSize()) / 2), CSSUnitType::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);
        setInlineStyleProperty(CSSPropertyHeight, newCueSize, CSSUnitType::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyMinHeight, "min-content");
        setInlineStyleProperty(CSSPropertyMaxHeight, maxSize, CSSUnitType::CSS_PERCENTAGE);
        if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
            setInlineStyleProperty(CSSPropertyTop, static_cast<double>(position.second - (newCueSize - cue->getCSSSize()) / 2), CSSUnitType::CSS_PERCENTAGE);
    }

    // The 'text-align' property on the (root) List of WebVTT Node Objects must
    // be set to the value in the second cell of the row of the table below
    // whose first cell is the value of the corresponding cue's text track cue
    // alignment:
    setInlineStyleProperty(CSSPropertyTextAlign, cue->getCSSAlignment());
    
    if (!cue->snapToLines()) {
        // 10.13.1 Set up x and y:
        // Note: x and y are set through the CSS left and top above.

        // 10.13.2 Position the boxes in boxes such that the point x% along the
        // width of the bounding box of the boxes in boxes is x% of the way
        // across the width of the video's rendering area, and the point y%
        // along the height of the bounding box of the boxes in boxes is y%
        // of the way across the height of the video's rendering area, while
        // maintaining the relative positions of the boxes in boxes to each
        // other.
        setInlineStyleProperty(CSSPropertyTransform, makeString("translate(", -position.first, "%, ", -position.second, "%)"));

        setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePre);
    }

    // Make sure shadow or stroke is not clipped.
    setInlineStyleProperty(CSSPropertyOverflow, CSSValueVisible);
    cue->element().setInlineStyleProperty(CSSPropertyOverflow, CSSValueVisible);
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
    , m_originalStartTime(MediaTime::zeroTime())
{
    initialize();
}

VTTCue::VTTCue(Document& document, const WebVTTCueData& cueData)
    : TextTrackCue(document, MediaTime::zeroTime(), MediaTime::zeroTime())
    , m_originalStartTime(cueData.originalStartTime())
{
    initialize();
    setText(cueData.content());
    setStartTime(cueData.startTime());
    setEndTime(cueData.endTime());
    setId(cueData.id());
    setCueSettings(cueData.settings());
}

VTTCue::~VTTCue()
{
}

void VTTCue::initialize()
{
    m_cueBackdropBox = HTMLDivElement::create(ownerDocument());
    m_cueHighlightBox = HTMLSpanElement::create(spanTag, ownerDocument());
    m_snapToLines = true;
    m_displayTreeShouldChange = true;
    m_notifyRegion = true;
}

Ref<VTTCueBox> VTTCue::createDisplayTree()
{
    return VTTCueBox::create(ownerDocument(), *this);
}

VTTCueBox& VTTCue::displayTreeInternal()
{
    if (!m_displayTree)
        m_displayTree = createDisplayTree();
    return *m_displayTree;
}

void VTTCue::didChange()
{
    TextTrackCue::didChange();
    m_displayTreeShouldChange = true;
}

const String& VTTCue::vertical() const
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

ExceptionOr<void> VTTCue::setVertical(const String& value)
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
        return { };

    if (direction == m_writingDirection)
        return { };

    willChange();
    m_writingDirection = direction;
    didChange();

    return { };
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
    if (std::isnan(m_linePosition))
        return Auto;

    return m_linePosition;
}

ExceptionOr<void> VTTCue::setLine(const LineAndPositionSetting& position)
{
    double linePosition = 0;

    if (WTF::holds_alternative<AutoKeyword>(position)) {
        if (std::isnan(m_linePosition))
            return { };
        linePosition = std::numeric_limits<double>::quiet_NaN();
    } else {
        linePosition = WTF::get<double>(position);

        if (m_linePosition == linePosition)
            return { };
    }

    willChange();
    m_linePosition = linePosition;
    m_computedLinePosition = calculateComputedLinePosition();
    didChange();

    return { };
}

const String& VTTCue::lineAlign() const
{
    switch (m_lineAlignment) {
    case LignAlignmentStart:
        return startKeyword();
    case LignAlignmentCenter:
        return centerKeyword();
    case LignAlignmentEnd:
        return endKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

ExceptionOr<void> VTTCue::setLineAlign(const String& value)
{
    CueLignAlignment lineAlignment;
    if (value == startKeyword())
        lineAlignment = LignAlignmentStart;
    else if (value == centerKeyword())
        lineAlignment = LignAlignmentCenter;
    else if (value == endKeyword())
        lineAlignment = LignAlignmentEnd;
    else
        return { };

    if (lineAlignment == m_lineAlignment)
        return { };

    willChange();
    m_lineAlignment = lineAlignment;
    didChange();

    return { };
}


VTTCue::LineAndPositionSetting VTTCue::position() const
{
    if (textPositionIsAuto())
        return Auto;
    return m_textPosition;
}

ExceptionOr<void> VTTCue::setPosition(const LineAndPositionSetting& position)
{
    // http://dev.w3.org/html5/webvtt/#dfn-vttcue-position
    // On setting, if the new value is negative or greater than 100, then an
    // IndexSizeError exception must be thrown. Otherwise, the WebVTT cue
    // position must be set to the new value; if the new value is the string
    // "auto", then it must be interpreted as the special value auto.
    double textPosition = 0;
    if (WTF::holds_alternative<AutoKeyword>(position)) {
        if (textPositionIsAuto())
            return { };
        textPosition = std::numeric_limits<double>::quiet_NaN();
    } else {
        textPosition = WTF::get<double>(position);
        if (!(textPosition >= 0 && textPosition <= 100))
            return Exception { IndexSizeError };

        // Otherwise, set the text track cue line position to the new value.
        if (m_textPosition == textPosition)
            return { };
    }

    willChange();
    m_textPosition = textPosition;
    didChange();

    return { };
}

const String& VTTCue::positionAlign() const
{
    switch (m_positionAlignment) {
    case PositionAlignmentLignLeft:
        return lineLeftKeyword();
    case PositionAlignmentLignCenter:
        return centerKeyword();
    case PositionAlignmentLignRight:
        return lineRightKeyword();
    case PositionAlignmentLignAuto:
        return autoKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

ExceptionOr<void> VTTCue::setPositionAlign(const String& value)
{
    CuePositionAlignment positionAlignment;
    if (value == lineLeftKeyword())
        positionAlignment = PositionAlignmentLignLeft;
    else if (value == centerKeyword())
        positionAlignment = PositionAlignmentLignCenter;
    else if (value == lineRightKeyword())
        positionAlignment = PositionAlignmentLignRight;
    else if (value == autoKeyword())
        positionAlignment = PositionAlignmentLignAuto;
    else
        return { };

    if (positionAlignment == m_positionAlignment)
        return { };

    willChange();
    m_positionAlignment = positionAlignment;
    didChange();

    return { };
}

ExceptionOr<void> VTTCue::setSize(int size)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-size
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError
    // exception. Otherwise, set the text track cue size to the new value.
    if (!(size >= 0 && size <= 100))
        return Exception { IndexSizeError };

    // Otherwise, set the text track cue line position to the new value.
    if (m_cueSize == size)
        return { };
    
    willChange();
    m_cueSize = size;
    didChange();

    return { };
}

const String& VTTCue::align() const
{
    switch (m_cueAlignment) {
    case Start:
        return startKeyword();
    case Center:
        return centerKeyword();
    case End:
        return endKeyword();
    case Left:
        return leftKeyword();
    case Right:
        return rightKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

ExceptionOr<void> VTTCue::setAlign(const String& value)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-align
    // On setting, the text track cue alignment must be set to the value given in the 
    // first cell of the row in the table above whose second cell is a case-sensitive
    // match for the new value, if any. If none of the values match, then the user
    // agent must instead throw a SyntaxError exception.
    
    CueAlignment alignment;
    if (value == startKeyword())
        alignment = Start;
    else if (value == centerKeyword())
        alignment = Center;
    else if (value == endKeyword())
        alignment = End;
    else if (value == leftKeyword())
        alignment = Left;
    else if (value == rightKeyword())
        alignment = Right;
    else
        return { };

    if (alignment == m_cueAlignment)
        return { };

    willChange();
    m_cueAlignment = alignment;
    didChange();

    return { };
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
    if (!m_webVTTNodeTree)
        m_webVTTNodeTree = WebVTTParser::createDocumentFragmentFromCueText(ownerDocument(), m_content);
}

static void copyWebVTTNodeToDOMTree(ContainerNode& webVTTNode, Node& parent)
{
    for (RefPtr<Node> node = webVTTNode.firstChild(); node; node = node->nextSibling()) {
        RefPtr<Node> clonedNode;
        if (is<WebVTTElement>(*node))
            clonedNode = downcast<WebVTTElement>(*node).createEquivalentHTMLElement(parent.document());
        else
            clonedNode = node->cloneNode(false);
        parent.appendChild(*clonedNode);
        if (is<ContainerNode>(*node))
            copyWebVTTNodeToDOMTree(downcast<ContainerNode>(*node), *clonedNode);
    }
}

RefPtr<DocumentFragment> VTTCue::getCueAsHTML()
{
    createWebVTTNodeTree();
    if (!m_webVTTNodeTree)
        return nullptr;

    auto clonedFragment = DocumentFragment::create(ownerDocument());
    copyWebVTTNodeToDOMTree(*m_webVTTNodeTree, clonedFragment);
    return clonedFragment;
}

RefPtr<DocumentFragment> VTTCue::createCueRenderingTree()
{
    createWebVTTNodeTree();
    if (!m_webVTTNodeTree)
        return nullptr;

    auto clonedFragment = DocumentFragment::create(ownerDocument());

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
    LOG(Media, "VTTCue::setTrack");
    TextTrackCue::setTrack(track);
    if (!m_parsedRegionId.isEmpty()) {
        if (track != nullptr) {
            if (auto* regions = track->regions()) {
                if (auto region = regions->getRegionById(m_parsedRegionId))
                    m_region = RefPtr<VTTRegion>(region);
            }
        }
    }
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

int VTTCue::calculateComputedLinePosition()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#text-track-cue-computed-line-position

    // If the text track cue line position is numeric, then that is the text
    // track cue computed line position.
    if (!std::isnan(m_linePosition))
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

static bool isCueParagraphSeparator(UChar character)
{
    // Within a cue, paragraph boundaries are only denoted by Type B characters,
    // such as U+000A LINE FEED (LF), U+0085 NEXT LINE (NEL), and U+2029 PARAGRAPH SEPARATOR.
    return u_charType(character) == U_PARAGRAPH_SEPARATOR;
}

bool VTTCue::textPositionIsAuto() const
{
    return std::isnan(m_textPosition);
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
    if (!textPositionIsAuto())
        return m_textPosition;
    
    switch (m_cueAlignment) {
    case Start:
    case Left:
        // 2. If the cue text alignment is start or left, return 0 and abort these
        // steps.
        return 0;
    case End:
    case Right:
        // 3. If the cue text alignment is end or right, return 100 and abort these
        // steps.
        return 100;
    case Center:
        // 4. If the cue text alignment is center, return 50 and abort these steps.
        return 50;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

void VTTCue::calculateDisplayParameters()
{
    // Steps 10.2, 10.3
    determineTextDirection();

    // 10.4 If the text track cue writing direction is horizontal, then let
    // block-flow be 'tb'. Otherwise, if the text track cue writing direction is
    // vertical growing left, then let block-flow be 'lr'. Otherwise, the text
    // track cue writing direction is vertical growing right; let block-flow be
    // 'rl'.

    // The above step is done through the writing direction static map.

    // 10.5 Determine the value of maximum size for cue as per the appropriate
    // rules from the following list:
    double computedTextPosition = calculateComputedTextPosition();
    int maximumSize = computedTextPosition;
    if ((m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueLtr)
        || (m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueRtl)
        || (m_writingDirection == Horizontal && m_cueAlignment == Left)
        || (m_writingDirection == VerticalGrowingLeft && (m_cueAlignment == Start || m_cueAlignment == Left))
        || (m_writingDirection == VerticalGrowingRight && (m_cueAlignment == Start || m_cueAlignment == Left))) {
        maximumSize = 100 - computedTextPosition;
    } else if ((m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueLtr)
        || (m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueRtl)
        || (m_writingDirection == Horizontal && m_cueAlignment == Right)
        || (m_writingDirection == VerticalGrowingLeft && (m_cueAlignment == End || m_cueAlignment == Right))
        || (m_writingDirection == VerticalGrowingRight && (m_cueAlignment == End || m_cueAlignment == Right))) {
        maximumSize = computedTextPosition;
    } else if (m_cueAlignment == Center) {
        maximumSize = computedTextPosition <= 50 ? computedTextPosition : (100 - computedTextPosition);
        maximumSize = maximumSize * 2;
    } else
        ASSERT_NOT_REACHED();

    // 10.6 If the text track cue size is less than maximum size, then let size
    // be text track cue size. Otherwise, let size be maximum size.
    m_displaySize = std::min(m_cueSize, maximumSize);

    // FIXME: Understand why step 10.7 is missing (just a copy/paste error?)
    // Could be done within a spec implementation check - http://crbug.com/301580

    // 10.8 Determine the value of x-position or y-position for cue as per the
    // appropriate rules from the following list:
    if (m_writingDirection == Horizontal) {
        switch (m_cueAlignment) {
        case Start:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = computedTextPosition;
            else
                m_displayPosition.first = 100 - computedTextPosition - m_displaySize;
            break;
        case End:
            if (m_displayDirection == CSSValueRtl)
                m_displayPosition.first = 100 - computedTextPosition;
            else
                m_displayPosition.first = computedTextPosition - m_displaySize;
            break;
        case Left:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = computedTextPosition;
            else
                m_displayPosition.first = 100 - computedTextPosition;
            break;
        case Right:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = computedTextPosition - m_displaySize;
            else
                m_displayPosition.first = 100 - computedTextPosition - m_displaySize;
            break;
        case Center:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = computedTextPosition - m_displaySize / 2;
            else
                m_displayPosition.first = 100 - computedTextPosition - m_displaySize / 2;
            break;
        case NumberOfAlignments:
            ASSERT_NOT_REACHED();
        }
    }

    // A text track cue has a text track cue computed line position whose value
    // is defined in terms of the other aspects of the cue.
    m_computedLinePosition = calculateComputedLinePosition();

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
        
        if (is<WebVTTElement>(*child)) {
            downcast<WebVTTElement>(*child).setIsPastNode(isPastNode);
            // Make an elemenet id match a cue id for style matching purposes.
            if (!id().isEmpty())
                downcast<WebVTTElement>(*child).setIdAttribute(id());
        }
    }
}

void VTTCue::updateDisplayTree(const MediaTime& movieTime)
{
    // The display tree may contain WebVTT timestamp objects representing
    // timestamps (processing instructions), along with displayable nodes.

    if (!track()->isRendered())
        return;

    // Mutating the VTT contents is safe because it's never exposed to author scripts.
    ScriptDisallowedScope::EventAllowedScope allowedScopeForCueHighlightBox(*m_cueHighlightBox);

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

RefPtr<TextTrackCueBox> VTTCue::getDisplayTree(const IntSize& videoSize, int fontSize)
{
    Ref<VTTCueBox> displayTree = displayTreeInternal();
    if (!m_displayTreeShouldChange || !track()->isRendered())
        return displayTree;

    // 10.1 - 10.10
    calculateDisplayParameters();

    // 10.11. Apply the terms of the CSS specifications to nodes within the
    // following constraints, thus obtaining a set of CSS boxes positioned
    // relative to an initial containing block:
    displayTree->removeChildren();

    // The document tree is the tree of WebVTT Node Objects rooted at nodes.

    // The children of the nodes must be wrapped in an anonymous box whose
    // 'display' property has the value 'inline'. This is the WebVTT cue
    // background box.

    // Note: This is contained by default in m_cueHighlightBox.
    m_cueHighlightBox->setPseudo(cueShadowPseudoId());

    m_cueBackdropBox->setPseudo(cueBackdropShadowPseudoId());
    m_cueBackdropBox->appendChild(*m_cueHighlightBox);
    displayTree->appendChild(*m_cueBackdropBox);

    // FIXME(BUG 79916): Runs of children of WebVTT Ruby Objects that are not
    // WebVTT Ruby Text Objects must be wrapped in anonymous boxes whose
    // 'display' property has the value 'ruby-base'.

    displayTree->setFontSizeFromCaptionUserPrefs(fontSize);
    displayTree->applyCSSProperties(videoSize);

    if (displayTree->document().page()) {
        auto cssString = displayTree->document().page()->captionUserPreferencesStyleSheet();
        auto style = HTMLStyleElement::create(HTMLNames::styleTag, displayTree->document(), false);
        style->setTextContent(cssString);
        displayTree->appendChild(style);
    }

    const auto& styleSheets = track()->styleSheets();
    if (styleSheets) {
        for (const auto& cssString : *styleSheets) {
            auto style = HTMLStyleElement::create(HTMLNames::styleTag, displayTree->document(), false);
            style->setTextContent(cssString);
            displayTree->appendChild(style);
        }
    }

    if (m_fontSize)
        displayTree->setInlineStyleProperty(CSSPropertyFontSize, m_fontSize, CSSUnitType::CSS_PX, m_fontSizeIsImportant);

    m_displayTreeShouldChange = false;

    if (track()) {
        if (m_region)
            m_region->cueStyleChanged();
    }

    // 10.15. Let cue's text track cue display state have the CSS boxes in
    // boxes.
    return displayTree;
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

    // The display tree is never exposed to author scripts so it's safe to dispatch events here.
    ScriptDisallowedScope::EventAllowedScope allowedScope(displayTreeInternal());
    displayTreeInternal().remove();
}

std::pair<double, double> VTTCue::getPositionCoordinates() const
{
    // This method is used for setting x and y when snap to lines is not set.
    std::pair<double, double> coordinates;

    auto textPosition = calculateComputedTextPosition();
    
    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueLtr) {
        coordinates.first = textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueRtl) {
        coordinates.first = 100 - textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingLeft) {
        coordinates.first = 100 - m_computedLinePosition;
        coordinates.second = textPosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingRight) {
        coordinates.first = m_computedLinePosition;
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
        input.skipWhile<WebVTTParser::isValidSettingDelimiter>();
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
        VTTScanner::Run valueRun = input.collectUntil<WebVTTParser::isValidSettingDelimiter>();

        // 4. Run the appropriate substeps that apply for the value of name, as follows:
        switch (name) {
        case Vertical: {
            // If name is a case-sensitive match for "vertical"
            // 1. If value is a case-sensitive match for the string "rl", then let cue's text track cue writing direction 
            //    be vertical growing left.
            if (input.scanRun(valueRun, verticalGrowingLeftKeyword()))
                m_writingDirection = VerticalGrowingLeft;
            
            // 2. Otherwise, if value is a case-sensitive match for the string "lr", then let cue's text track cue writing 
            //    direction be vertical growing right.
            else if (input.scanRun(valueRun, verticalGrowingRightKeyword()))
                m_writingDirection = VerticalGrowingRight;

            else
                LOG(Media, "VTTCue::setCueSettings, invalid Vertical");
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

                bool isPercentage = input.scan('%');
                if (!input.isAt(valueRun.end())) {
                    if (!input.scan(','))
                        break;
                    // FIXME: implement handling of line setting alignment.
                    if (!input.scan(startKeyword().characters8(), startKeyword().length())
                        && !input.scan(centerKeyword().characters8(), centerKeyword().length())
                        && !input.scan(endKeyword().characters8(), endKeyword().length())) {
                        LOG(Media, "VTTCue::setCueSettings, invalid line setting alignment");
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
                isValid = true;
            } while (0);

            if (!isValid)
                LOG(Media, "VTTCue::setCueSettings, invalid Line");

            break;
        }
        case Position: {
            float position;
            if (WebVTTParser::parseFloatPercentageValue(input, position) && input.isAt(valueRun.end()))
                m_textPosition = position;
            else
                LOG(Media, "VTTCue::setCueSettings, invalid Position");
            break;
        }
        case Size: {
            float cueSize;
            if (WebVTTParser::parseFloatPercentageValue(input, cueSize) && input.isAt(valueRun.end()))
                m_cueSize = cueSize;
            else
                LOG(Media, "VTTCue::setCueSettings, invalid Size");
            break;
        }
        case Align: {
            // 1. If value is a case-sensitive match for the string "start", then let cue's text track cue alignment be start alignment.
            if (input.scanRun(valueRun, startKeyword()))
                m_cueAlignment = Start;

            // 2. If value is a case-sensitive match for the string "center", then let cue's text track cue alignment be center alignment.
            else if (input.scanRun(valueRun, centerKeyword()))
                m_cueAlignment = Center;

            // 3. If value is a case-sensitive match for the string "end", then let cue's text track cue alignment be end alignment.
            else if (input.scanRun(valueRun, endKeyword()))
                m_cueAlignment = End;

            // 4. If value is a case-sensitive match for the string "left", then let cue's text track cue alignment be left alignment.
            else if (input.scanRun(valueRun, leftKeyword()))
                m_cueAlignment = Left;

            // 5. If value is a case-sensitive match for the string "right", then let cue's text track cue alignment be right alignment.
            else if (input.scanRun(valueRun, rightKeyword()))
                m_cueAlignment = Right;

            else
                LOG(Media, "VTTCue::setCueSettings, invalid Align");

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
    return displayAlignmentMap[m_cueAlignment];
}

CSSValueID VTTCue::getCSSWritingDirection() const
{
    return m_displayDirection;
}

CSSValueID VTTCue::getCSSWritingMode() const
{
    return displayWritingModeMap[m_writingDirection];
}

int VTTCue::getCSSSize() const
{
    return m_displaySize;
}

std::pair<double, double> VTTCue::getCSSPosition() const
{
    if (!m_snapToLines)
        return getPositionCoordinates();

    return m_displayPosition;
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

void VTTCue::setFontSize(int fontSize, const IntSize&, bool important)
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

    // FIXME: Seems dangerous to include this based on LOG_DISABLED. Can we just include it unconditionally?
#if !LOG_DISABLED
    object.setString("text"_s, text());
#endif

    object.setString("vertical"_s, vertical());
    object.setBoolean("snapToLines"_s, snapToLines());
    object.setDouble("line"_s, m_linePosition);
    if (textPositionIsAuto())
        object.setString("position"_s, "auto"_s);
    else
        object.setDouble("position"_s, m_textPosition);
    object.setInteger("size"_s, m_cueSize);
    object.setString("align"_s, align());
}

} // namespace WebCore

#endif
