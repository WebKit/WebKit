/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerQuirks.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerHolePunchQuirkBcmNexus.h"
#include "GStreamerHolePunchQuirkFake.h"
#include "GStreamerHolePunchQuirkRialto.h"
#include "GStreamerHolePunchQuirkWesteros.h"
#include "GStreamerQuirkAmLogic.h"
#include "GStreamerQuirkBcmNexus.h"
#include "GStreamerQuirkBroadcom.h"
#include "GStreamerQuirkOpenMAX.h"
#include "GStreamerQuirkQualcomm.h"
#include "GStreamerQuirkRealtek.h"
#include "GStreamerQuirkRialto.h"
#include "GStreamerQuirkWesteros.h"
#include <optional>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerQuirkBase);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerQuirk);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerHolePunchQuirk);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerQuirksManager);

GST_DEBUG_CATEGORY_STATIC(webkit_quirks_debug);
#define GST_CAT_DEFAULT webkit_quirks_debug

GStreamerQuirksManager& GStreamerQuirksManager::singleton()
{
    static NeverDestroyed<GStreamerQuirksManager> sharedInstance(false, true);
    return sharedInstance;
}

GStreamerQuirksManager::GStreamerQuirksManager(bool isForTesting, bool loadQuirksFromEnvironment)
    : m_isForTesting(isForTesting)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        if (isInWebProcess())
            ensureGStreamerInitialized();
        else
            // This is needed, e.g. when running in NetworkProcess to determine MIME type support
            ensureGStreamerInitializedNonWebProcess();

        GST_DEBUG_CATEGORY_INIT(webkit_quirks_debug, "webkitquirks", 0, "WebKit Quirks");
    });

    // For the time being keep this disabled on non-WPE platforms. GTK on desktop shouldn't require
    // quirks, for instance.
#if !PLATFORM(WPE)
    return;
#endif

    GST_DEBUG("Quirk manager created%s", m_isForTesting ? " for testing." : ".");
    if (!loadQuirksFromEnvironment)
        return;

    auto quirksString = CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_QUIRKS"));
    GST_DEBUG("Attempting to parse requested quirks: %s", GST_STR_NULL(quirksString.utf8()));
    if (quirksString) {
        if (WTF::equalLettersIgnoringASCIICase(quirksString.span(), "help"_s)) {
            gst_printerrln("Supported quirks for WEBKIT_GST_QUIRKS are: amlogic, broadcom, bcmnexus, openmax, qualcomm, realtek, rialto, westeros");
            return;
        }

        for (const auto& identifier : String(quirksString.span()).split(',')) {
            std::unique_ptr<GStreamerQuirk> quirk;
            if (WTF::equalLettersIgnoringASCIICase(identifier, "amlogic"_s))
                quirk = WTF::makeUnique<GStreamerQuirkAmLogic>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "broadcom"_s))
                quirk = WTF::makeUnique<GStreamerQuirkBroadcom>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "bcmnexus"_s))
                quirk = WTF::makeUnique<GStreamerQuirkBcmNexus>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "openmax"_s))
                quirk = WTF::makeUnique<GStreamerQuirkOpenMAX>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "qualcomm"_s))
                quirk = WTF::makeUnique<GStreamerQuirkQualcomm>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "realtek"_s))
                quirk = WTF::makeUnique<GStreamerQuirkRealtek>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "rialto"_s))
                quirk = WTF::makeUnique<GStreamerQuirkRialto>();
            else if (WTF::equalLettersIgnoringASCIICase(identifier, "westeros"_s))
                quirk = WTF::makeUnique<GStreamerQuirkWesteros>();
            else {
                GST_WARNING("Unknown quirk requested: %s. Skipping", identifier.ascii().data());
                continue;
            }

            if (!quirk->isPlatformSupported()) {
                GST_WARNING("Quirk %s was requested but is not supported on this platform. Skipping", quirk->identifier().characters());
                continue;
            }
            m_quirks.append(WTF::move(quirk));
        }
    } else {
        GST_DEBUG("WEBKIT_GST_QUIRKS was not set, now performing automatic quirks detection.");
#define ADD_QUIRK(name) m_quirks.append(WTF::makeUnique<GStreamerQuirk##name>())
        ADD_QUIRK(AmLogic);
        ADD_QUIRK(Broadcom);
        ADD_QUIRK(BcmNexus);
        ADD_QUIRK(OpenMAX);
        ADD_QUIRK(Qualcomm);
        ADD_QUIRK(Realtek);
        ADD_QUIRK(Rialto);
        ADD_QUIRK(Westeros);
#undef ADD_QUIRK

        m_quirks.removeAllMatching([](auto& quirk) -> bool {
            bool isPlatformSupported = quirk->isPlatformSupported();
            if (isPlatformSupported)
                GST_INFO("Enabling quirk %s", quirk->identifier().characters());
            return !isPlatformSupported;
        });
    }

    GST_DEBUG("%zu quirks enabled", m_quirks.size());

    auto identifierString = CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_HOLE_PUNCH_QUIRK"));
    GST_DEBUG("Attempting to parse requested hole-punch quirk: %s", GST_STR_NULL(identifierString.utf8()));
    if (!identifierString)
        return;

    if (WTF::equalLettersIgnoringASCIICase(identifierString.span(), "help"_s)) {
        gst_printerrln("Supported quirks for WEBKIT_GST_HOLE_PUNCH_QUIRK are: fake, bcmnexus, rialto, westeros");
        return;
    }

    // TODO: Maybe check this is coherent (somehow) with the quirk(s) selected above.
    if (WTF::equalLettersIgnoringASCIICase(identifierString.span(), "bcmnexus"_s))
        m_holePunchQuirk = WTF::makeUnique<GStreamerHolePunchQuirkBcmNexus>();
    else if (WTF::equalLettersIgnoringASCIICase(identifierString.span(), "rialto"_s))
        m_holePunchQuirk = WTF::makeUnique<GStreamerHolePunchQuirkRialto>();
    else if (WTF::equalLettersIgnoringASCIICase(identifierString.span(), "westeros"_s))
        m_holePunchQuirk = WTF::makeUnique<GStreamerHolePunchQuirkWesteros>();
    else if (WTF::equalLettersIgnoringASCIICase(identifierString.span(), "fake"_s))
        m_holePunchQuirk = WTF::makeUnique<GStreamerHolePunchQuirkFake>();
    else
        GST_WARNING("HolePunch quirk %s un-supported.", identifierString.utf8());
}

bool GStreamerQuirksManager::isEnabled() const
{
    return !m_quirks.isEmpty();
}

GstElement* GStreamerQuirksManager::createAudioSink()
{
    for (const auto& quirk : m_quirks) {
        auto* sink = quirk->createAudioSink();
        if (sink) {
            GST_DEBUG("Using AudioSink from quirk %s : %" GST_PTR_FORMAT, quirk->identifier().characters(), sink);
            return sink;
        }
    }

    return nullptr;
}

GstElement* GStreamerQuirksManager::createWebAudioSink()
{
    for (const auto& quirk : m_quirks) {
        auto* sink = quirk->createWebAudioSink();
        if (!sink)
            continue;

        GST_DEBUG("Using WebAudioSink from quirk %s : %" GST_PTR_FORMAT, quirk->identifier().characters(), sink);
        return sink;
    }

    GST_DEBUG("Quirks didn't specify a WebAudioSink, falling back to default sink");
    return createPlatformAudioSink("webaudio"_s);
}

GstElement* GStreamerQuirksManager::createHolePunchVideoSink(bool isLegacyPlaybin, const MediaPlayer* player)
{
    if (!m_holePunchQuirk) {
        GST_DEBUG("None of the quirks requested a HolePunchSink");
        return nullptr;
    }
    auto sink = m_holePunchQuirk->createHolePunchVideoSink(isLegacyPlaybin, player);
    GST_DEBUG("Using HolePunchSink from quirk %s : %" GST_PTR_FORMAT, m_holePunchQuirk->identifier().characters(), sink);
    return sink;
}

void GStreamerQuirksManager::setHolePunchVideoRectangle(GstElement* videoSink, const IntRect& rect)
{
    if (!m_holePunchQuirk) {
        GST_DEBUG("None of the quirks requested a HolePunchSink");
        return;
    }

    if (!m_holePunchQuirk->setHolePunchVideoRectangle(videoSink, rect))
        GST_WARNING("Hole punch video rectangle configuration failed.");
}

bool GStreamerQuirksManager::sinksRequireClockSynchronization() const
{
    if (!m_holePunchQuirk)
        return true;

    return m_holePunchQuirk->requiresClockSynchronization();
}

void GStreamerQuirksManager::configureElement(GstElement* element, OptionSet<ElementRuntimeCharacteristics>&& characteristics)
{
    if (m_quirks.isEmpty())
        return;
    GST_DEBUG("Configuring element %" GST_PTR_FORMAT, element);
    for (const auto& quirk : m_quirks)
        quirk->configureElement(element, characteristics);
}

std::optional<bool> GStreamerQuirksManager::isHardwareAccelerated(GstElementFactory* factory) const
{
    for (const auto& quirk : m_quirks) {
        auto result = quirk->isHardwareAccelerated(factory);
        if (!result)
            continue;

        GST_DEBUG("Setting %" GST_PTR_FORMAT " as %s accelerated from quirk %s", factory, quirk->identifier().characters(), *result ? "hardware" : "software");
        return *result;
    }

    return std::nullopt;
}

bool GStreamerQuirksManager::supportsVideoHolePunchRendering() const
{
    return m_holePunchQuirk.get();
}

GstElementFactoryListType GStreamerQuirksManager::audioVideoDecoderFactoryListType() const
{
    for (const auto& quirk : m_quirks) {
        auto result = quirk->audioVideoDecoderFactoryListType();
        if (!result)
            continue;

        GST_DEBUG("Quirk %s requests audio/video decoder factory list override to %" G_GUINT32_FORMAT, quirk->identifier().characters(), static_cast<uint32_t>(*result));
        return *result;
    }

    return GST_ELEMENT_FACTORY_TYPE_DECODER;
}

Vector<String> GStreamerQuirksManager::disallowedWebAudioDecoders() const
{
    Vector<String> result;
    for (const auto& quirk : m_quirks)
        result.appendVector(quirk->disallowedWebAudioDecoders());

    return result;
}

void GStreamerQuirksManager::setHolePunchEnabledForTesting(bool enabled)
{
    if (enabled)
        m_holePunchQuirk = WTF::makeUnique<GStreamerHolePunchQuirkFake>();
    else
        m_holePunchQuirk = nullptr;
}

unsigned GStreamerQuirksManager::getAdditionalPlaybinFlags() const
{
    unsigned flags = 0;
    for (const auto& quirk : m_quirks) {
        auto quirkFlags = quirk->getAdditionalPlaybinFlags();
        GST_DEBUG("Quirk %s requests these playbin flags: %u", quirk->identifier().characters(), quirkFlags);
        flags |= quirkFlags;
    }

    if (flags)
        GST_DEBUG("Final quirk flags: %u", flags);
    else {
        GST_DEBUG("Quirks didn't request any specific playbin flags, returning default text+soft-colorbalance.");
        flags = getGstPlayFlag("text") | getGstPlayFlag("soft-colorbalance");
    }
    return flags;
}

bool GStreamerQuirksManager::shouldParseIncomingLibWebRTCBitStream() const
{
    for (auto& quirk : m_quirks) {
        if (!quirk->shouldParseIncomingLibWebRTCBitStream())
            return false;
    }
    return true;
}

GRefPtr<GstCaps> GStreamerQuirksManager::videoSinkGLCapsFormat() const
{
    for (auto& quirk : m_quirks) {
        if (auto caps = quirk->videoSinkGLCapsFormat())
            return caps;
    }
    return nullptr;
}

bool GStreamerQuirksManager::needsBufferingPercentageCorrection() const
{
    for (auto& quirk : m_quirks) {
        if (quirk->needsBufferingPercentageCorrection())
            return true;
    }
    return false;
}

ASCIILiteral GStreamerQuirksManager::queryBufferingPercentage(MediaPlayerPrivateGStreamer* mediaPlayerPrivate, const GRefPtr<GstQuery>& query) const
{
    // Only the first quirk that needs percentage correction must operate. We're assuming that the m_quirks Vector
    // preserves its order among calls to the percentage correction family of methods.
    for (auto& quirk : m_quirks) {
        if (quirk->needsBufferingPercentageCorrection())
            return quirk->queryBufferingPercentage(mediaPlayerPrivate, query);
    }
    return nullptr;
}

int GStreamerQuirksManager::correctBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, int originalBufferingPercentage, GstBufferingMode mode) const
{
    // Only the first quirk that needs percentage correction must operate. We're assuming that the m_quirks Vector
    // preserves its order among calls to the percentage correction family of methods.
    for (auto& quirk : m_quirks) {
        if (quirk->needsBufferingPercentageCorrection())
            return quirk->correctBufferingPercentage(playerPrivate, originalBufferingPercentage, mode);
    }
    return originalBufferingPercentage;
}

void GStreamerQuirksManager::resetBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, int bufferingPercentage) const
{
    // Only the first quirk that needs percentage correction must operate. We're assuming that the m_quirks Vector
    // preserves its order among calls to the percentage correction family of methods.
    for (auto& quirk : m_quirks) {
        if (quirk->needsBufferingPercentageCorrection()) {
            quirk->resetBufferingPercentage(playerPrivate, bufferingPercentage);
            return;
        }
    }
}

void GStreamerQuirksManager::setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer* playerPrivate, GstState currentState, GstState newState, GRefPtr<GstElement>&& element) const
{
    // Only the first quirk that needs percentage correction must operate. We're assuming that the m_quirks Vector
    // preserves its order among calls to the percentage correction family of methods.
    for (auto& quirk : m_quirks) {
        if (quirk->needsBufferingPercentageCorrection()) {
            // We're moving the element to the inner method. If this loop ever needs to call the method twice,
            // think about a solution to avoid passing a dummy element (after first move) to the method the second
            // time it's called.
            quirk->setupBufferingPercentageCorrection(playerPrivate, currentState, newState, WTF::move(element));
            return;
        }
    }
}

void GStreamerQuirksManager::processWebAudioSilentBuffer(GstBuffer* buffer) const
{
    for (const auto& quirk : m_quirks) {
        if (quirk->processWebAudioSilentBuffer(buffer))
            break;
    }
}

bool GStreamerQuirksManager::needsCustomInstantRateChange() const
{
    for (auto& quirk : m_quirks) {
        if (quirk->needsCustomInstantRateChange())
            return true;
    }
    return false;
}

// Returning processed and didInstantRateChange.
std::pair<bool, bool> GStreamerQuirksManager::applyCustomInstantRateChange(
    bool isPipelinePlaying, bool isPipelineWaitingPreroll, float playbackRate, bool mute, GstElement* pipeline) const
{
    for (auto& quirk : m_quirks) {
        if (quirk->needsCustomInstantRateChange()) {
            return quirk->applyCustomInstantRateChange(isPipelinePlaying, isPipelineWaitingPreroll, playbackRate,
                mute, pipeline);
        }
    }
    return { false, false };
}

// Returning forwardToAllPads.
bool GStreamerQuirksManager::analyzeWebKitMediaSrcCustomEvent(GRefPtr<GstEvent> event) const
{
    for (auto& quirk : m_quirks) {
        if (quirk->needsCustomInstantRateChange())
            return quirk->analyzeWebKitMediaSrcCustomEvent(event);
    }
    return false;
}

// Returning rate.
std::optional<double> GStreamerQuirksManager::processWebKitMediaSrcCustomEvent(GRefPtr<GstEvent> event, bool handledByAnyStream,
    bool handledByAllStreams) const
{
    for (auto& quirk : m_quirks) {
        if (quirk->needsCustomInstantRateChange())
            return quirk->processWebKitMediaSrcCustomEvent(event, handledByAnyStream, handledByAllStreams);
    }
    return std::nullopt;
}

// Returning processed and didInstantRateChange.
std::pair<bool, bool> GStreamerQuirk::applyCustomInstantRateChange(
    bool isPipelinePlaying, bool isPipelineWaitingPreroll, float playbackRate, bool mute, GstElement* pipeline) const
{
    // This check allows to implement a common behaviour for this instant rate change family of methods
    // in the superclass, but ensure that the code will only run on subclasses actually supporting
    // instant rate change. This is better than copypasting the code to all the subclasses, and better
    // than creating a common base class. Common base classes for features instead of for platforms
    // is a strategy that would quickly become unmaintainable.
    if (!needsCustomInstantRateChange())
        return std::pair<bool, bool>(false, false);

    bool didInstantRateChange = false;
    if (isPipelinePlaying && !isPipelineWaitingPreroll) {
        GstStructure* s = gst_structure_new("custom-instant-rate-change",
            "rate", G_TYPE_DOUBLE, playbackRate, nullptr);
        didInstantRateChange = gst_element_send_event(
            pipeline, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));
        if (didInstantRateChange)
            g_object_set(pipeline, "mute", mute, nullptr);
    }

    return { true, didInstantRateChange };
}

// Returning forwardToAllPads.
bool GStreamerQuirk::analyzeWebKitMediaSrcCustomEvent(GRefPtr<GstEvent> event) const
{
    return needsCustomInstantRateChange() && gst_event_has_name(event.get(), "custom-instant-rate-change");
}

// Returning rate.
std::optional<double> GStreamerQuirk::processWebKitMediaSrcCustomEvent(GRefPtr<GstEvent> event, bool,
    bool handledByAllStreams) const
{
    if (!needsCustomInstantRateChange())
        return std::nullopt;
    if (gst_event_has_name(event.get(), "custom-instant-rate-change") && handledByAllStreams) {
        auto eventRate = gstStructureGet<double>(gst_event_get_structure(event.get()), "rate"_s);
        if (eventRate.has_value())
            return eventRate.value();
    }
    return std::nullopt;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
