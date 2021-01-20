/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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
#include "VTTRegion.h"

#if ENABLE(VIDEO)

#include "DOMRect.h"
#include "DOMTokenList.h"
#include "ElementChildIterator.h"
#include "HTMLDivElement.h"
#include "HTMLParserIdioms.h"
#include "Logging.h"
#include "RenderElement.h"
#include "VTTCue.h"
#include "VTTScanner.h"
#include "WebVTTParser.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// The default values are defined within the WebVTT Regions Spec.
// https://dvcs.w3.org/hg/text-tracks/raw-file/default/608toVTT/region.html

// Default region line-height (vh units)
static const float lineHeight = 5.33;

// Default scrolling animation time period (s).
static const Seconds scrollTime { 433_ms };

VTTRegion::VTTRegion(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
    , m_id(emptyString())
    , m_scrollTimer(*this, &VTTRegion::scrollTimerFired)
{
}

VTTRegion::~VTTRegion() = default;

void VTTRegion::setTrack(TextTrack* track)
{
    m_track = track;
}

void VTTRegion::setId(const String& id)
{
    m_id = id;
}

ExceptionOr<void> VTTRegion::setWidth(double value)
{
    if (!(value >= 0 && value <= 100))
        return Exception { IndexSizeError };
    m_width = value;
    return { };
}

ExceptionOr<void> VTTRegion::setLines(int value)
{
    if (value < 0)
        return Exception { IndexSizeError };
    m_lines = value;
    return { };
}

ExceptionOr<void> VTTRegion::setRegionAnchorX(double value)
{
    if (!(value >= 0 && value <= 100))
        return Exception { IndexSizeError };
    m_regionAnchor.setX(value);
    return { };
}

ExceptionOr<void> VTTRegion::setRegionAnchorY(double value)
{
    if (!(value >= 0 && value <= 100))
        return Exception { IndexSizeError };
    m_regionAnchor.setY(value);
    return { };
}

ExceptionOr<void> VTTRegion::setViewportAnchorX(double value)
{
    if (!(value >= 0 && value <= 100))
        return Exception { IndexSizeError };
    m_viewportAnchor.setX(value);
    return { };
}

ExceptionOr<void> VTTRegion::setViewportAnchorY(double value)
{
    if (!(value >= 0 && value <= 100))
        return Exception { IndexSizeError };
    m_viewportAnchor.setY(value);
    return { };
}

static const AtomString& upKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> upKeyword("up", AtomString::ConstructFromLiteral);
    return upKeyword;
}

const AtomString& VTTRegion::scroll() const
{
    return m_scroll ? upKeyword() : emptyAtom();
}

ExceptionOr<void> VTTRegion::setScroll(const AtomString& value)
{
    if (value.isEmpty()) {
        m_scroll = false;
        return { };
    }
    if (value == upKeyword()) {
        m_scroll = true;
        return { };
    }
    return Exception { SyntaxError };
}

void VTTRegion::updateParametersFromRegion(const VTTRegion& other)
{
    m_lines = other.m_lines;
    m_width = other.m_width;
    m_regionAnchor = other.m_regionAnchor;
    m_viewportAnchor = other.m_viewportAnchor;
    m_scroll = other.m_scroll;
}

void VTTRegion::setRegionSettings(const String& inputString)
{
    m_settings = inputString;
    VTTScanner input(inputString);

    while (!input.isAtEnd()) {
        input.skipWhile<WebVTTParser::isValidSettingDelimiter>();
        if (input.isAtEnd())
            break;

        // Scan the name part.
        RegionSetting name = scanSettingName(input);

        // Verify that we're looking at a ':'.
        if (name == None || !input.scan(':')) {
            input.skipUntil<isHTMLSpace<UChar>>();
            continue;
        }

        // Scan the value part.
        parseSettingValue(name, input);
    }
}

VTTRegion::RegionSetting VTTRegion::scanSettingName(VTTScanner& input)
{
    if (input.scan("id"))
        return Id;
    if (input.scan("lines"))
        return Lines;
    if (input.scan("width"))
        return Width;
    if (input.scan("viewportanchor"))
        return ViewportAnchor;
    if (input.scan("regionanchor"))
        return RegionAnchor;
    if (input.scan("scroll"))
        return Scroll;

    return None;
}

static inline bool parsedEntireRun(const VTTScanner& input, const VTTScanner::Run& run)
{
    return input.isAt(run.end()); 
}

void VTTRegion::parseSettingValue(RegionSetting setting, VTTScanner& input)
{
    VTTScanner::Run valueRun = input.collectUntil<isHTMLSpace<UChar>>();

    switch (setting) {
    case Id: {
        String stringValue = input.extractString(valueRun);
        if (stringValue.find("-->") == notFound)
            m_id = stringValue;
        break;
    }
    case Width: {
        float floatWidth;
        if (WebVTTParser::parseFloatPercentageValue(input, floatWidth) && parsedEntireRun(input, valueRun))
            m_width = floatWidth;
        else
            LOG(Media, "VTTRegion::parseSettingValue, invalid Width");
        break;
    }
    case Lines: {
        int number;
        if (input.scanDigits(number) && parsedEntireRun(input, valueRun))
            m_lines = number;
        else
            LOG(Media, "VTTRegion::parseSettingValue, invalid Height");
        break;
    }
    case RegionAnchor: {
        FloatPoint anchor;
        if (WebVTTParser::parseFloatPercentageValuePair(input, ',', anchor) && parsedEntireRun(input, valueRun))
            m_regionAnchor = anchor;
        else
            LOG(Media, "VTTRegion::parseSettingValue, invalid RegionAnchor");
        break;
    }
    case ViewportAnchor: {
        FloatPoint anchor;
        if (WebVTTParser::parseFloatPercentageValuePair(input, ',', anchor) && parsedEntireRun(input, valueRun))
            m_viewportAnchor = anchor;
        else
            LOG(Media, "VTTRegion::parseSettingValue, invalid ViewportAnchor");
        break;
    }
    case Scroll:
        if (input.scanRun(valueRun, upKeyword()))
            m_scroll = true;
        else
            LOG(Media, "VTTRegion::parseSettingValue, invalid Scroll");
        break;
    case None:
        break;
    }

    input.skipRun(valueRun);
}

const AtomString& VTTRegion::textTrackCueContainerScrollingClass()
{
    static MainThreadNeverDestroyed<const AtomString> trackRegionCueContainerScrollingClass("scrolling", AtomString::ConstructFromLiteral);

    return trackRegionCueContainerScrollingClass;
}

const AtomString& VTTRegion::textTrackCueContainerShadowPseudoId()
{
    static MainThreadNeverDestroyed<const AtomString> trackRegionCueContainerPseudoId("-webkit-media-text-track-region-container", AtomString::ConstructFromLiteral);

    return trackRegionCueContainerPseudoId;
}

const AtomString& VTTRegion::textTrackRegionShadowPseudoId()
{
    static MainThreadNeverDestroyed<const AtomString> trackRegionShadowPseudoId("-webkit-media-text-track-region", AtomString::ConstructFromLiteral);

    return trackRegionShadowPseudoId;
}

void VTTRegion::appendTextTrackCueBox(Ref<TextTrackCueBox>&& displayBox)
{
    ASSERT(m_cueContainer);

    if (m_cueContainer->contains(displayBox.ptr()))
        return;

    m_cueContainer->appendChild(displayBox);
    displayLastTextTrackCueBox();
}

void VTTRegion::displayLastTextTrackCueBox()
{
    ASSERT(m_cueContainer);

    // The container needs to be rendered, if it is not empty and the region is not currently scrolling.
    if (!m_cueContainer->renderer() || !m_cueContainer->hasChildNodes() || m_scrollTimer.isActive())
        return;

    // If it's a scrolling region, add the scrolling class.
    if (isScrollingRegion())
        m_cueContainer->classList().add(textTrackCueContainerScrollingClass());

    float regionBottom = m_regionDisplayTree->getBoundingClientRect()->bottom();

    // Find first cue that is not entirely displayed and scroll it upwards.
    for (auto& child : childrenOfType<Element>(*m_cueContainer)) {
        auto rect = child.getBoundingClientRect();
        float childTop = rect->top();
        float childBottom = rect->bottom();

        if (regionBottom >= childBottom)
            continue;

        float height = childBottom - childTop;

        m_currentTop -= std::min(height, childBottom - regionBottom);
        m_cueContainer->setInlineStyleProperty(CSSPropertyTop, m_currentTop, CSSUnitType::CSS_PX);

        startTimer();
        break;
    }
}

void VTTRegion::willRemoveTextTrackCueBox(VTTCueBox* box)
{
    LOG(Media, "VTTRegion::willRemoveTextTrackCueBox");
    ASSERT(m_cueContainer->contains(box));

    double boxHeight = box->getBoundingClientRect()->bottom() - box->getBoundingClientRect()->top();

    m_cueContainer->classList().remove(textTrackCueContainerScrollingClass());

    m_currentTop += boxHeight;
    m_cueContainer->setInlineStyleProperty(CSSPropertyTop, m_currentTop, CSSUnitType::CSS_PX);
}

HTMLDivElement& VTTRegion::getDisplayTree()
{
    if (!m_regionDisplayTree) {
        m_regionDisplayTree = HTMLDivElement::create(downcast<Document>(*m_scriptExecutionContext));
        m_regionDisplayTree->setPseudo(textTrackRegionShadowPseudoId());
        m_recalculateStyles = true;
    }

    if (m_recalculateStyles)
        prepareRegionDisplayTree();

    return *m_regionDisplayTree;
}

void VTTRegion::prepareRegionDisplayTree()
{
    ASSERT(m_regionDisplayTree);

    // 7.2 Prepare region CSS boxes

    // FIXME: Change the code below to use viewport units when
    // http://crbug/244618 is fixed.

    // Let regionWidth be the text track region width.
    // Let width be 'regionWidth vw' ('vw' is a CSS unit)
    m_regionDisplayTree->setInlineStyleProperty(CSSPropertyWidth, m_width, CSSUnitType::CSS_PERCENTAGE);

    // Let lineHeight be '0.0533vh' ('vh' is a CSS unit) and regionHeight be
    // the text track region height. Let height be 'lineHeight' multiplied
    // by regionHeight.
    double height = lineHeight * m_lines;
    m_regionDisplayTree->setInlineStyleProperty(CSSPropertyHeight, height, CSSUnitType::CSS_VH);

    // Let viewportAnchorX be the x dimension of the text track region viewport
    // anchor and regionAnchorX be the x dimension of the text track region
    // anchor. Let leftOffset be regionAnchorX multiplied by width divided by
    // 100.0. Let left be leftOffset subtracted from 'viewportAnchorX vw'.
    double leftOffset = m_regionAnchor.x() * m_width / 100;
    m_regionDisplayTree->setInlineStyleProperty(CSSPropertyLeft, m_viewportAnchor.x() - leftOffset, CSSUnitType::CSS_PERCENTAGE);

    // Let viewportAnchorY be the y dimension of the text track region viewport
    // anchor and regionAnchorY be the y dimension of the text track region
    // anchor. Let topOffset be regionAnchorY multiplied by height divided by
    // 100.0. Let top be topOffset subtracted from 'viewportAnchorY vh'.
    double topOffset = m_regionAnchor.y() * height / 100;
    m_regionDisplayTree->setInlineStyleProperty(CSSPropertyTop, m_viewportAnchor.y() - topOffset, CSSUnitType::CSS_PERCENTAGE);

    // The cue container is used to wrap the cues and it is the object which is
    // gradually scrolled out as multiple cues are appended to the region.
    if (!m_cueContainer) {
        m_cueContainer = HTMLDivElement::create(downcast<Document>(*m_scriptExecutionContext));
        m_cueContainer->setPseudo(textTrackCueContainerShadowPseudoId());
        m_regionDisplayTree->appendChild(*m_cueContainer);
    }
    m_cueContainer->setInlineStyleProperty(CSSPropertyTop, 0.0f, CSSUnitType::CSS_PX);

    // 7.5 Every WebVTT region object is initialised with the following CSS

    m_recalculateStyles = false;
}

void VTTRegion::startTimer()
{
    LOG(Media, "VTTRegion::startTimer");

    if (m_scrollTimer.isActive())
        return;

    Seconds duration = isScrollingRegion() ? scrollTime : 0_s;
    m_scrollTimer.startOneShot(duration);
}

void VTTRegion::stopTimer()
{
    LOG(Media, "VTTRegion::stopTimer");

    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

void VTTRegion::scrollTimerFired()
{
    LOG(Media, "VTTRegion::scrollTimerFired");

    stopTimer();
    displayLastTextTrackCueBox();
}

} // namespace WebCore

#endif
