/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaControlTextTrackContainerElement.h"

#if ENABLE(VIDEO)

#include "DOMTokenList.h"
#include "ElementChildIteratorInlines.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FullscreenManager.h"
#include "GraphicsContext.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "PODInterval.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderLayer.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
#include "TextTrackCueGeneric.h"
#include "TextTrackList.h"
#include "UserAgentParts.h"
#include "VTTRegionList.h"
#include <wtf/Language.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaControlTextTrackContainerElement);

using namespace HTMLNames;

Ref<MediaControlTextTrackContainerElement> MediaControlTextTrackContainerElement::create(Document& document, HTMLMediaElement& mediaElement)
{
    auto element = adoptRef(*new MediaControlTextTrackContainerElement(document, mediaElement));
    element->setUserAgentPart(UserAgentParts::webkitMediaTextTrackContainer());
    element->hide();
    return element;
}

MediaControlTextTrackContainerElement::MediaControlTextTrackContainerElement(Document& document, HTMLMediaElement& element)
    : HTMLDivElement(divTag, document)
    , m_mediaElement(element)
{
}

RenderPtr<RenderElement> MediaControlTextTrackContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, *this, WTFMove(style));
}

static bool compareCueIntervalForDisplay(const CueInterval& one, const CueInterval& two)
{
    return one.data()->isPositionedAbove(two.data());
};

void MediaControlTextTrackContainerElement::updateDisplay()
{
    if (m_mediaElement && !m_mediaElement->closedCaptionsVisible())
        removeChildren();

    // 1. If the media element is an audio element, or is another playback
    // mechanism with no rendering area, abort these steps. There is nothing to
    // render.
    if (!m_mediaElement || m_videoDisplaySize.size().isEmpty())
        return;

    // 2. Let video be the media element or other playback mechanism.
    RefPtr video = dynamicDowncast<HTMLVideoElement>(*m_mediaElement);
    if (!video)
        return;

    // 3. Let output be an empty list of absolutely positioned CSS block boxes.

    // 4. If the user agent is exposing a user interface for video, add to
    // output one or more completely transparent positioned CSS block boxes that
    // cover the same region as the user interface.

    // 5. If the last time these rules were run, the user agent was not exposing
    // a user interface for video, but now it is, optionally let reset be true. Otherwise,
    // let reset be false.

    // There is nothing to be done explicitly for 4th and 5th steps, as
    // everything is handled through CSS. The caption box is on top of the
    // controls box, in a container set with the -webkit-box display property.

    // 6. Let tracks be the subset of video's list of text tracks that have as
    // their rules for updating the text track rendering these rules for
    // updating the display of WebVTT text tracks, and whose text track mode is
    // showing or showing by default.
    // 7. Let cues be an empty list of text track cues.
    // 8. For each track track in tracks, append to cues all the cues from
    // track's list of cues that have their text track cue active flag set.
    CueList activeCues = video->currentlyActiveCues();

    // 9. Let regions be an empty list of WebVTT regions.

    // 10. For each track track in tracks, append to regions all the regions with an
    // identifier from track’s list of regions.

    // Steps 9 and 10 are unneccesary because we will initialize each region
    // individually in text track cue order.

    // There is nothing explicitly to be done here, as all the caching occurs
    // within the TextTrackCue instance itself. If parameters of the cue change,
    // the display tree is cleared.

    // 11. If reset is false, then, for each WebVTT region in regions let regionNode
    // be a WebVTT region object.

    // Steps 12 and 13 are performed in VTTRegion

    // If the number of CSS boxes in the output is less than the number of cues
    // we wish to render (e.g., we are adding another cue in a set of roll-up
    // cues), remove all the existing CSS boxes representing the cues and re-add
    // them so that the new cue is at the bottom.
    // FIXME: Calling countChildNodes() here is inefficient. We don't need to
    // traverse all children just to check if there are less children than cues.
    if (countChildNodes() < activeCues.size())
        removeChildren();

    activeCues.removeAllMatching([] (CueInterval& cueInterval) {
        RefPtr cue = cueInterval.data();
        return !cue->track()
            || !cue->track()->isRendered()
            || cue->track()->mode() == TextTrack::Mode::Disabled
            || !cue->isActive()
            || !cue->isRenderable();
    });

    // Sort the active cues for the appropriate display order. For example, for roll-up
    // or paint-on captions, we need to add the cues in reverse chronological order,
    // so that the newest captions appear at the bottom.
    std::sort(activeCues.begin(), activeCues.end(), &compareCueIntervalForDisplay);

    if (m_mediaElement->closedCaptionsVisible()) {
        // 10. For each text track cue in cues that has not yet had
        // corresponding CSS boxes added to output, in text track cue order, run the
        // following substeps:
        for (auto& interval : activeCues) {
            auto cue = interval.data();

            if (cue->track()->isSpoken())
                continue;

            cue->setFontSize(m_fontSize, m_fontSizeIsImportant);
            if (RefPtr vttCue = dynamicDowncast<VTTCue>(*cue))
                processActiveVTTCue(*vttCue);
            else {
                auto displayBox = cue->getDisplayTree();
                if (displayBox->hasChildNodes() && !contains(displayBox.get()))
                    appendChild(*displayBox);
            }
        }
    }

    // 11. Return output.
    if (hasChildNodes())
        show();
    else
        hide();

    updateTextTrackRepresentationIfNeeded();
    updateTextTrackStyle();
}

void MediaControlTextTrackContainerElement::updateTextTrackRepresentationImageIfNeeded()
{
    if (!m_needsToGenerateTextTrackRepresentation)
        return;

    m_needsToGenerateTextTrackRepresentation = false;

    // We should call m_textTrackRepresentation->update() to paint the subtree of
    // the RenderTextTrackContainerElement after the layout is clean.
    if (m_textTrackRepresentation) {
        m_textTrackRepresentation->update();
        m_textTrackRepresentation->setHidden(false);
    }
}

void MediaControlTextTrackContainerElement::processActiveVTTCue(VTTCue& cue)
{
    DEBUG_LOG(LOGIDENTIFIER, "adding and positioning cue: \"", cue.text(), "\", start=", cue.startTime(), ", end=", cue.endTime());

    if (auto region = cue.track()->regions()->getRegionById(cue.regionId())) {
        // Let region be the WebVTT region whose region identifier
        // matches the text track cue region identifier of cue.
        Ref<HTMLDivElement> regionNode = region->getDisplayTree();

        if (!contains(regionNode.ptr()))
            appendChild(region->getDisplayTree());

        region->appendTextTrackCueBox(*cue.getDisplayTree());
    } else {
        // If cue has an empty text track cue region identifier or there is no
        // WebVTT region whose region identifier is identical to cue's text
        // track cue region identifier, run the following substeps:
        Ref<TextTrackCueBox> displayBox = *cue.getDisplayTree();
        if (displayBox->hasChildNodes() && !contains(displayBox.ptr())) {
            // Note: the display tree of a cue is removed when the active flag of the cue is unset.
            appendChild(displayBox);
        }
    }
}

void MediaControlTextTrackContainerElement::updateActiveCuesFontSize()
{
    if (!document().page())
        return;

    if (!m_mediaElement)
        return;

    float fontScale = document().page()->group().ensureCaptionPreferences().captionFontSizeScaleAndImportance(m_fontSizeIsImportant);

    // Caption fonts are defined as |size vh| units, so there's no need to
    // scale by display size. Since |vh| is a decimal percentage, multiply
    // the scale factor by 100 to achive the final font size.
    m_fontSize = lroundf(100 * fontScale);

    for (auto& activeCue : m_mediaElement->currentlyActiveCues()) {
        RefPtr cue = activeCue.data();
        if (cue->isRenderable())
            cue->setFontSize(m_fontSize, m_fontSizeIsImportant);
    }
}

void MediaControlTextTrackContainerElement::updateTextStrokeStyle()
{
    if (!document().page())
        return;

    if (!m_mediaElement)
        return;
    
    String language;

    // FIXME: Since it is possible to have more than one text track enabled, the following code may not find the correct language.
    // The default UI only allows a user to enable one track at a time, so it should be OK for now, but we should consider doing
    // this differently, see <https://bugs.webkit.org/show_bug.cgi?id=169875>.
    if (RefPtr tracks = m_mediaElement->textTracks()) {
        for (unsigned i = 0; i < tracks->length(); ++i) {
            RefPtr track = tracks->item(i);
            if (track && track->mode() == TextTrack::Mode::Showing) {
                language = track->validBCP47Language();
                break;
            }
        }
    }

    float strokeWidth;
    bool important;

    // FIXME: find a way to set this property in the stylesheet like the other user style preferences, see <https://bugs.webkit.org/show_bug.cgi?id=169874>.
    if (document().page()->group().ensureCaptionPreferences().captionStrokeWidthForFont(m_fontSize, language, strokeWidth, important))
        setInlineStyleProperty(CSSPropertyStrokeWidth, strokeWidth, CSSUnitType::CSS_PX, important ? IsImportant::Yes : IsImportant::No);
}

void MediaControlTextTrackContainerElement::updateTextTrackRepresentationIfNeeded()
{
    if (!m_mediaElement)
        return;

    auto requiresTextTrackRepresentation = m_mediaElement->requiresTextTrackRepresentation();
    if (!hasChildNodes() || !requiresTextTrackRepresentation) {
        if (m_textTrackRepresentation) {
            if (!requiresTextTrackRepresentation)
                clearTextTrackRepresentation();
            else
                m_textTrackRepresentation->setHidden(true);
        }
        return;
    }

    if (!m_textTrackRepresentation) {
        ALWAYS_LOG(LOGIDENTIFIER);

        m_textTrackRepresentation = TextTrackRepresentation::create(*this, *m_mediaElement);
        if (document().page())
            m_textTrackRepresentation->setContentScale(document().page()->deviceScaleFactor());
        m_mediaElement->setTextTrackRepresentation(m_textTrackRepresentation.get());
    }

    m_needsToGenerateTextTrackRepresentation = true;
}

void MediaControlTextTrackContainerElement::clearTextTrackRepresentation()
{
    if (!m_textTrackRepresentation)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_textTrackRepresentation = nullptr;
    if (m_mediaElement)
        m_mediaElement->setTextTrackRepresentation(nullptr);
}

void MediaControlTextTrackContainerElement::updateTextTrackStyle()
{
    if (m_textTrackRepresentation) {
        setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
        setInlineStyleProperty(CSSPropertyWidth, m_videoDisplaySize.size().width(), CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyHeight, m_videoDisplaySize.size().height(), CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyLeft, 0, CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyTop, 0, CSSUnitType::CSS_PX);
        return;
    }

    removeInlineStyleProperty(CSSPropertyPosition);
    removeInlineStyleProperty(CSSPropertyWidth);
    removeInlineStyleProperty(CSSPropertyHeight);
    removeInlineStyleProperty(CSSPropertyLeft);
    removeInlineStyleProperty(CSSPropertyTop);
}

void MediaControlTextTrackContainerElement::requiresTextTrackRepresentationChanged()
{
    updateTextTrackRepresentationIfNeeded();
    updateSizes(ForceUpdate::Yes);
}

void MediaControlTextTrackContainerElement::enteredFullscreen()
{
    updateTextTrackRepresentationIfNeeded();
    updateSizes(ForceUpdate::Yes);
}

void MediaControlTextTrackContainerElement::exitedFullscreen()
{
    clearTextTrackRepresentation();
    updateSizes(ForceUpdate::Yes);
}

bool MediaControlTextTrackContainerElement::updateVideoDisplaySize()
{
    if (!document().page())
        return false;

    if (!m_mediaElement)
        return false;

    IntRect videoBox;
    if (m_textTrackRepresentation) {
        videoBox = m_textTrackRepresentation->bounds();
        float deviceScaleFactor = document().page()->deviceScaleFactor();
        videoBox.setWidth(videoBox.width() * deviceScaleFactor);
        videoBox.setHeight(videoBox.height() * deviceScaleFactor);
    } else {
        if (auto* renderVideo = dynamicDowncast<RenderVideo>(m_mediaElement->renderer()))
            videoBox = renderVideo->videoBox();
        else
            return false;
    }

    if (m_videoDisplaySize == videoBox)
        return false;

    m_videoDisplaySize = videoBox;
    return true;
}

void MediaControlTextTrackContainerElement::updateSizes(ForceUpdate force)
{
    if (!updateVideoDisplaySize() && force != ForceUpdate::Yes)
        return;

    if (!document().page() || !m_mediaElement)
        return;

    m_mediaElement->syncTextTrackBounds();

    updateActiveCuesFontSize();
    updateTextStrokeStyle();
    for (auto& activeCue : m_mediaElement->currentlyActiveCues())
        activeCue.data()->recalculateStyles();

    document().eventLoop().queueTask(TaskSource::MediaElement, [weakThis = WeakPtr { *this }] () {
        if (weakThis)
            weakThis->updateDisplay();
    });
}

RefPtr<NativeImage> MediaControlTextTrackContainerElement::createTextTrackRepresentationImage()
{
    if (!hasChildNodes())
        return nullptr;

    RefPtr frame = document().frame();
    if (!frame)
        return nullptr;

    protectedDocument()->updateLayout();

    auto* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    if (!renderer->hasLayer())
        return nullptr;

    RenderLayer* layer = downcast<RenderLayerModelObject>(*renderer).layer();

    float deviceScaleFactor = 1;
    if (Page* page = document().page())
        deviceScaleFactor = page->deviceScaleFactor();

    IntRect paintingRect = IntRect(IntPoint(), layer->size());

    // FIXME (149422): This buffer should not be unconditionally unaccelerated.
    auto buffer = ImageBuffer::create(paintingRect.size(), RenderingPurpose::Unspecified, deviceScaleFactor, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    auto paintFlags = RenderLayer::paintLayerPaintingCompositingAllPhasesFlags();
    paintFlags.add(RenderLayer::PaintLayerFlag::TemporaryClipRects);
    layer->paint(buffer->context(), paintingRect, LayoutSize(), { PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting }, nullptr, paintFlags);

    return ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
}

void MediaControlTextTrackContainerElement::textTrackRepresentationBoundsChanged(const IntRect&)
{
    updateTextTrackRepresentationIfNeeded();
    updateSizes();
}

void MediaControlTextTrackContainerElement::hide()
{
    setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

void MediaControlTextTrackContainerElement::show()
{
    removeInlineStyleProperty(CSSPropertyDisplay);
}

bool MediaControlTextTrackContainerElement::isShowing() const
{
    const StyleProperties* propertySet = inlineStyle();

    // Following the code from show() and hide() above, we only have
    // to check for the presense of inline display.
    return (!propertySet || !propertySet->getPropertyCSSValue(CSSPropertyDisplay));
}


#if !RELEASE_LOG_DISABLED
const Logger& MediaControlTextTrackContainerElement::logger() const
{
    if (!m_logger)
        m_logger = &document().logger();

    return *m_logger;
}

const void* MediaControlTextTrackContainerElement::logIdentifier() const
{
    if (!m_logIdentifier && m_mediaElement)
        m_logIdentifier = m_mediaElement->logIdentifier();

    return m_logIdentifier;
}

WTFLogChannel& MediaControlTextTrackContainerElement::logChannel() const
{
    return LogMedia;
}
#endif // !RELEASE_LOG_DISABLED

// ----------------------------

} // namespace WebCore

#endif // ENABLE(VIDEO)
