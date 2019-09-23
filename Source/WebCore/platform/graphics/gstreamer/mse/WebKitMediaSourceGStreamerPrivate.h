/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivateGStreamer.h"
#include "GUniquePtrGStreamer.h"
#include "MainThreadNotifier.h"
#include "SourceBufferPrivateGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <wtf/Forward.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class MediaPlayerPrivateGStreamerMSE;

};

void webKitMediaSrcUriHandlerInit(gpointer, gpointer);

#define WEBKIT_MEDIA_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcPrivate))

typedef struct _Stream Stream;

struct _Stream {
    // Fields filled when the Stream is created.
    WebKitMediaSrc* parent;

    // AppSrc. Never modified after first assignment.
    GstElement* appsrc;

    // Never modified after first assignment.
    WebCore::SourceBufferPrivateGStreamer* sourceBuffer;

    // Fields filled when the track is attached.
    WebCore::MediaSourceStreamTypeGStreamer type;
    GRefPtr<GstCaps> caps;

    // Only audio, video or nothing at a given time.
    RefPtr<WebCore::AudioTrackPrivateGStreamer> audioTrack;
    RefPtr<WebCore::VideoTrackPrivateGStreamer> videoTrack;
    WebCore::FloatSize presentationSize;

    // This helps WebKitMediaSrcPrivate.appsrcNeedDataCount, ensuring that needDatas are
    // counted only once per each appsrc.
    bool appsrcNeedDataFlag;

    // Used to enforce continuity in the appended data and avoid breaking the decoder.
    // Only used from the main thread.
    MediaTime lastEnqueuedTime;
};

enum {
    PROP_0,
    PROP_LOCATION,
    PROP_N_AUDIO,
    PROP_N_VIDEO,
    PROP_N_TEXT,
    PROP_LAST
};

enum {
    SIGNAL_VIDEO_CHANGED,
    SIGNAL_AUDIO_CHANGED,
    SIGNAL_TEXT_CHANGED,
    LAST_SIGNAL
};

enum OnSeekDataAction {
    Nothing,
    MediaSourceSeekToTime
};

enum WebKitMediaSrcMainThreadNotification {
    ReadyForMoreSamples = 1 << 0,
    SeekNeedsData = 1 << 1
};

struct _WebKitMediaSrcPrivate {
    // Used to coordinate the release of Stream track info.
    Lock streamLock;
    Condition streamCondition;

    // Streams are only added/removed in the main thread.
    Vector<Stream*> streams;

    GUniquePtr<gchar> location;
    int numberOfAudioStreams;
    int numberOfVideoStreams;
    int numberOfTextStreams;
    bool asyncStart;
    bool allTracksConfigured;
    unsigned numberOfPads;

    MediaTime seekTime;

    // On seek, we wait for all the seekDatas, then for all the needDatas, and then run the nextAction.
    OnSeekDataAction appsrcSeekDataNextAction;
    int appsrcSeekDataCount;
    int appsrcNeedDataCount;

    WebCore::MediaPlayerPrivateGStreamerMSE* mediaPlayerPrivate;

    RefPtr<WebCore::MainThreadNotifier<WebKitMediaSrcMainThreadNotification>> notifier;
    GUniquePtr<GstFlowCombiner> flowCombiner;
};

extern guint webKitMediaSrcSignals[LAST_SIGNAL];
extern GstAppSrcCallbacks enabledAppsrcCallbacks;
extern GstAppSrcCallbacks disabledAppsrcCallbacks;

void webKitMediaSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);
void webKitMediaSrcFinalize(GObject*);
void webKitMediaSrcSetProperty(GObject*, guint propertyId, const GValue*, GParamSpec*);
void webKitMediaSrcGetProperty(GObject*, guint propertyId, GValue*, GParamSpec*);
void webKitMediaSrcDoAsyncStart(WebKitMediaSrc*);
void webKitMediaSrcDoAsyncDone(WebKitMediaSrc*);
GstStateChangeReturn webKitMediaSrcChangeState(GstElement*, GstStateChange);
gint64 webKitMediaSrcGetSize(WebKitMediaSrc*);
gboolean webKitMediaSrcQueryWithParent(GstPad*, GstObject*, GstQuery*);
void webKitMediaSrcUpdatePresentationSize(GstCaps*, Stream*);
void webKitMediaSrcLinkStreamToSrcPad(GstPad*, Stream*);
void webKitMediaSrcLinkSourcePad(GstPad*, GstCaps*, Stream*);
void webKitMediaSrcFreeStream(WebKitMediaSrc*, Stream*);
void webKitMediaSrcCheckAllTracksConfigured(WebKitMediaSrc*);
GstURIType webKitMediaSrcUriGetType(GType);
const gchar* const* webKitMediaSrcGetProtocols(GType);
gchar* webKitMediaSrcGetUri(GstURIHandler*);
gboolean webKitMediaSrcSetUri(GstURIHandler*, const gchar*, GError**);

#endif // USE(GSTREAMER)
