/*
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "CString.h"
#include "Noncopyable.h"
#include "NotImplemented.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "webkitdownload.h"
#include "webkitmarshal.h"
#include "webkitprivate.h"

#include <glib/gstdio.h>

using namespace WebKit;
using namespace WebCore;

class DownloadClient : Noncopyable, public ResourceHandleClient {
    public:
        DownloadClient(WebKitDownload*);

        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
        virtual void didReceiveData(ResourceHandle*, const char*, int, int);
        virtual void didFinishLoading(ResourceHandle*);
        virtual void didFail(ResourceHandle*, const ResourceError&);
        virtual void wasBlocked(ResourceHandle*);
        virtual void cannotShowURL(ResourceHandle*);

    private:
        WebKitDownload* m_download;
};

extern "C" {

#define WEBKIT_DOWNLOAD_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_DOWNLOAD, WebKitDownloadPrivate))

struct _WebKitDownloadPrivate {
    gchar* destinationURI;
    gchar* suggestedFilename;
    guint currentSize;
    GTimer* timer;
    WebKitDownloadState state;
    GFileOutputStream* outputStream;
    DownloadClient* downloadClient;
    WebKitNetworkRequest* networkRequest;
    ResourceResponse* networkResponse;
    RefPtr<ResourceHandle> resourceHandle;
};

enum {
    // Normal signals.
    ERROR,
    LAST_SIGNAL
};

static guint webkit_download_signals[LAST_SIGNAL] = { 0 };

enum {
    PROP_0,

    PROP_NETWORK_REQUEST,
    PROP_DESTINATION_URI,
    PROP_SUGGESTED_FILENAME,
    PROP_PROGRESS,
    PROP_CURRENT_SIZE,
    PROP_TOTAL_SIZE
};

G_DEFINE_TYPE(WebKitDownload, webkit_download, G_TYPE_OBJECT);

static void webkit_download_dispose(GObject* object)
{
    WebKitDownload* download = WEBKIT_DOWNLOAD(object);
    WebKitDownloadPrivate* priv = download->priv;

    if (priv->outputStream) {
        g_object_unref(priv->outputStream);
        priv->outputStream = NULL;
    }

    if (priv->networkRequest) {
        g_object_unref(priv->networkRequest);
        priv->networkRequest = NULL;
    }

    G_OBJECT_CLASS(webkit_download_parent_class)->dispose(object);
}

static void webkit_download_finalize(GObject* object)
{
    WebKitDownload* download = WEBKIT_DOWNLOAD(object);
    WebKitDownloadPrivate* priv = download->priv;

    // We don't call webkit_download_cancel() because we don't want to emit
    // signals when finalizing an object.
    if (priv->resourceHandle) {
        if (priv->state == WEBKIT_DOWNLOAD_STATE_STARTED) {
            priv->resourceHandle->setClient(0);
            priv->resourceHandle->cancel();
        }
        priv->resourceHandle.release();
    }

    delete priv->downloadClient;
    delete priv->networkResponse;

    // The download object may never have _start called on it, so we
    // need to make sure timer is non-NULL.
    if (priv->timer)
        g_timer_destroy(priv->timer);

    g_free(priv->destinationURI);
    g_free(priv->suggestedFilename);

    G_OBJECT_CLASS(webkit_download_parent_class)->finalize(object);
}

static void webkit_download_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebKitDownload* download = WEBKIT_DOWNLOAD(object);

    switch(prop_id) {
    case PROP_NETWORK_REQUEST:
        g_value_set_object(value, webkit_download_get_network_request(download));
        break;
    case PROP_DESTINATION_URI:
        g_value_set_string(value, webkit_download_get_destination_uri(download));
        break;
    case PROP_SUGGESTED_FILENAME:
        g_value_set_string(value, webkit_download_get_suggested_filename(download));
        break;
    case PROP_PROGRESS:
        g_value_set_double(value, webkit_download_get_progress(download));
        break;
    case PROP_CURRENT_SIZE:
        g_value_set_uint64(value, webkit_download_get_current_size(download));
        break;
    case PROP_TOTAL_SIZE:
        g_value_set_uint64(value, webkit_download_get_total_size(download));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void webkit_download_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec *pspec)
{
    WebKitDownload* download = WEBKIT_DOWNLOAD(object);
    WebKitDownloadPrivate* priv = download->priv;

    switch(prop_id) {
    case PROP_NETWORK_REQUEST:
        priv->networkRequest = WEBKIT_NETWORK_REQUEST(g_value_dup_object(value));
        // This is safe as network-request is a construct only property and
        // suggestedFilename is initially null.
        priv->suggestedFilename = g_path_get_basename(webkit_network_request_get_uri(priv->networkRequest));
        break;
    case PROP_DESTINATION_URI:
        webkit_download_set_destination_uri(download, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void webkit_download_class_init(WebKitDownloadClass* downloadClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(downloadClass);
    objectClass->dispose = webkit_download_dispose;
    objectClass->finalize = webkit_download_finalize;
    objectClass->get_property = webkit_download_get_property;
    objectClass->set_property = webkit_download_set_property;

    /**
     * WebKitDownload::error:
     * @download: the object on which the signal is emitted
     * @current_bytes: the current count of bytes downloaded
     * @total_bytes: the total bytes count in the downloaded file, aka file size.
     *
     * Indicates an error in the download.
     *
     * Since: 1.1.2
     */
    webkit_download_signals[ERROR] = g_signal_new("error",
            G_TYPE_FROM_CLASS(downloadClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__INT_INT_STRING,
            G_TYPE_BOOLEAN, 3,
            G_TYPE_INT,
            G_TYPE_INT,
            G_TYPE_STRING);

    // Properties.

    /**
     * WebKitDownload:network-request
     *
     * The #WebKitNetworkRequest instance associated with the download.
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass,
                                    PROP_NETWORK_REQUEST,
                                    g_param_spec_object("network-request",
                                                        "Network Request",
                                                        "The network request for the URI that should be downloaded",
                                                        WEBKIT_TYPE_NETWORK_REQUEST,
                                                        (GParamFlags)(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitDownload:destination-uri
     *
     * The URI of the save location for this download.
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass,
                                    PROP_DESTINATION_URI,
                                    g_param_spec_string("destination-uri",
                                                        "Destination URI",
                                                        "The destination URI where to save the file",
                                                        "",
                                                        WEBKIT_PARAM_READWRITE));

    /**
     * WebKitDownload:suggested-filename
     *
     * The file name suggested as default when saving
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass,
                                    PROP_SUGGESTED_FILENAME,
                                    g_param_spec_string("suggested-filename",
                                                        "Suggested Filename",
                                                        "The filename suggested as default when saving",
                                                        "",
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitDownload:progress:
     *
     * Determines the current progress of the download.
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass, PROP_PROGRESS,
                                    g_param_spec_double("progress",
                                                        "Progress",
                                                        "Determines the current progress of the download",
                                                        0.0, 1.0, 1.0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitDownload:current-size
     *
     * The length of the data already downloaded
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass,
                                    PROP_CURRENT_SIZE,
                                    g_param_spec_uint64("current-size",
                                                        "Current Size",
                                                        "The length of the data already downloaded",
                                                        0, G_MAXUINT64, 0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitDownload:total-size
     *
     * The total size of the file
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass,
                                    PROP_CURRENT_SIZE,
                                    g_param_spec_uint64("total-size",
                                                        "Total Size",
                                                        "The total size of the file",
                                                        0, G_MAXUINT64, 0,
                                                        WEBKIT_PARAM_READABLE));

    g_type_class_add_private(downloadClass, sizeof(WebKitDownloadPrivate));
}

static void webkit_download_init(WebKitDownload* download)
{
    WebKitDownloadPrivate* priv = WEBKIT_DOWNLOAD_GET_PRIVATE(download);
    download->priv = priv;

    priv->downloadClient = new DownloadClient(download);
    priv->currentSize = 0;
    priv->state = WEBKIT_DOWNLOAD_STATE_CREATED;
}

/**
 * webkit_download_new:
 * @request: a #WebKitNetworkRequest
 *
 * Creates a new #WebKitDownload object for the given
 * #WebKitNetworkRequest object.
 *
 * Returns: the new #WebKitDownload
 *
 * Since: 1.1.2
 */
WebKitDownload* webkit_download_new(WebKitNetworkRequest* request)
{
    g_return_val_if_fail(request, NULL);

    return WEBKIT_DOWNLOAD(g_object_new(WEBKIT_TYPE_DOWNLOAD, "network-request", request, NULL));
}

static gboolean webkit_download_open_stream_for_uri(WebKitDownload* download, const gchar* uri, gboolean append=FALSE)
{
    g_return_val_if_fail(uri, FALSE);

    WebKitDownloadPrivate* priv = download->priv;
    GFile* file = g_file_new_for_uri(uri);
    GError* error = NULL;

    if (append)
        priv->outputStream = g_file_append_to(file, G_FILE_CREATE_NONE, NULL, &error);
    else
        priv->outputStream = g_file_replace(file, NULL, TRUE, G_FILE_CREATE_NONE, NULL, &error);

    g_object_unref(file);

    if (error) {
        gboolean handled;
        g_signal_emit_by_name(download, "error", 0, WEBKIT_DOWNLOAD_ERROR_DESTINATION, error->message, &handled);
        g_error_free(error);
        return FALSE;
    }

    return TRUE;
}

static void webkit_download_close_stream(WebKitDownload* download)
{
    WebKitDownloadPrivate* priv = download->priv;
    if (priv->outputStream) {
        g_object_unref(priv->outputStream);
        priv->outputStream = NULL;
    }
}

/**
 * webkit_download_start:
 * @download: the #WebKitDownload
 *
 * Initiates the download. Notice that you must have set the
 * destination-uri property before calling this method.
 *
 * Since: 1.1.2
 */
void webkit_download_start(WebKitDownload* download)
{
    g_return_if_fail(WEBKIT_IS_DOWNLOAD(download));

    WebKitDownloadPrivate* priv = download->priv;
    g_return_if_fail(priv->destinationURI);
    g_return_if_fail(priv->state == WEBKIT_DOWNLOAD_STATE_CREATED);
    g_return_if_fail(priv->timer == NULL);

    if (priv->resourceHandle)
        priv->resourceHandle->setClient(priv->downloadClient);
    else {
        // FIXME: Use the actual request object when WebKitNetworkRequest is finished.
        ResourceRequest request(webkit_network_request_get_uri(priv->networkRequest));
        priv->resourceHandle = ResourceHandle::create(request, priv->downloadClient, 0, false, false, false);
    }

    priv->timer = g_timer_new();
    webkit_download_open_stream_for_uri(download, priv->destinationURI);
}

/**
 * webkit_download_cancel:
 * @download: the #WebKitDownload
 *
 * Cancels the download. Calling this will not free the
 * #WebKitDownload object, so you still need to call
 * g_object_unref() on it, if you are the owner of a reference. Notice
 * that cancelling the download provokes the emission of the
 * WebKitDownload::error signal, reporting that the download was
 * cancelled.
 *
 * Since: 1.1.2
 */
void webkit_download_cancel(WebKitDownload* download)
{
    g_return_if_fail(WEBKIT_IS_DOWNLOAD(download));

    WebKitDownloadPrivate* priv = download->priv;

    // Cancel may be called even if start was not called, so we need
    // to make sure timer is non-NULL.
    if (priv->timer)
        g_timer_stop(priv->timer);

    if (priv->resourceHandle)
        priv->resourceHandle->cancel();

    priv->state = WEBKIT_DOWNLOAD_STATE_CANCELLED;

    gboolean handled;
    g_signal_emit_by_name(download, "error", 0, WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER, "User cancelled the download", &handled);
}

/**
 * webkit_download_get_uri:
 * @download: the #WebKitDownload
 *
 * Convenience method to retrieve the URI from the
 * #WebKitNetworkRequest which is being downloaded.
 *
 * Returns: the uri
 *
 * Since: 1.1.2
 */
const gchar* webkit_download_get_uri(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), NULL);

    WebKitDownloadPrivate* priv = download->priv;
    return webkit_network_request_get_uri(priv->networkRequest);
}

/**
 * webkit_download_get_network_request:
 * @download: the #WebKitDownload
 *
 * Retrieves the #WebKitNetworkRequest object that backs the download
 * process.
 *
 * Returns: the #WebKitNetworkRequest instance
 *
 * Since: 1.1.2
 */
WebKitNetworkRequest* webkit_download_get_network_request(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), NULL);

    WebKitDownloadPrivate* priv = download->priv;
    return priv->networkRequest;
}

static void webkit_download_set_response(WebKitDownload* download, const ResourceResponse& response)
{
    // FIXME Use WebKitNetworkResponse when it's merged.
    WebKitDownloadPrivate* priv = download->priv;
    priv->networkResponse = new ResourceResponse(response);
}

/**
 * webkit_download_get_suggested_filename:
 * @download: the #WebKitDownload
 *
 * Retrieves the filename that was suggested by the server, or the one
 * derived by WebKit from the URI.
 *
 * Returns: the suggested filename
 *
 * Since: 1.1.2
 */
const gchar* webkit_download_get_suggested_filename(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), NULL);

    WebKitDownloadPrivate* priv = download->priv;
    return priv->suggestedFilename;
}

/**
 * webkit_download_get_destination_uri:
 * @download: the #WebKitDownload
 *
 * Obtains the URI to which the downloaded file will be written. This
 * must have been set by the application before calling
 * webkit_download_start(), and may be %NULL.
 *
 * Returns: the destination URI or %NULL
 *
 * Since: 1.1.2
 */
const gchar* webkit_download_get_destination_uri(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), NULL);

    WebKitDownloadPrivate* priv = download->priv;
    return priv->destinationURI;
}

/**
 * webkit_download_set_destination_uri:
 * @download: the #WebKitDownload
 * @destination_uri: the destination URI
 *
 * Defines the URI that should be used to save the downloaded file to.
 *
 * Since: 1.1.2
 */
void webkit_download_set_destination_uri(WebKitDownload* download, const gchar* destination_uri)
{
    g_return_if_fail(WEBKIT_IS_DOWNLOAD(download));
    g_return_if_fail(destination_uri);

    WebKitDownloadPrivate* priv = download->priv;
    if (priv->destinationURI && !strcmp(priv->destinationURI, destination_uri))
        return;

    if (priv->state != WEBKIT_DOWNLOAD_STATE_CREATED && priv->state != WEBKIT_DOWNLOAD_STATE_CANCELLED) {
        ASSERT(priv->destinationURI);

        gboolean downloading = priv->outputStream != NULL;
        if (downloading)
            webkit_download_close_stream(download);

        GFile* src = g_file_new_for_uri(priv->destinationURI);
        GFile* dest = g_file_new_for_uri(destination_uri);
        GError* error = NULL;

        g_file_move(src, dest, G_FILE_COPY_BACKUP, NULL, NULL, NULL, &error);

        g_object_unref(src);
        g_object_unref(dest);

        g_free(priv->destinationURI);
        priv->destinationURI = g_strdup(destination_uri);

        if (error) {
            gboolean handled;
            g_signal_emit_by_name(download, "error", 0, WEBKIT_DOWNLOAD_ERROR_DESTINATION, error->message, &handled);
            g_error_free(error);
            return;
        }

        if (downloading) {
            if (!webkit_download_open_stream_for_uri(download, destination_uri, TRUE)) {
                webkit_download_cancel(download);
                return;
            }
        }
    } else {
        g_free(priv->destinationURI);
        priv->destinationURI = g_strdup(destination_uri);
    }

    // Only notify change if everything went fine.
    g_object_notify(G_OBJECT(download), "destination-uri");
}

/**
 * webkit_download_get_state:
 * @download: the #WebKitDownload
 *
 * Obtains the current state of the download, as a
 * #WebKitDownloadState.
 *
 * Returns: the current #WebKitDownloadState
 *
 * Since: 1.1.2
 */
WebKitDownloadState webkit_download_get_state(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), WEBKIT_DOWNLOAD_STATE_ERROR);

    WebKitDownloadPrivate* priv = download->priv;
    return priv->state;
}

/**
 * webkit_download_get_total_size:
 * @download: the #WebKitDownload
 *
 * Returns the expected total size of the download. This is expected
 * because the server may provide incorrect or missing
 * Content-Length. Notice that this may grow over time, as it will be
 * always the same as current_size in the cases where current size
 * surpasses it.
 *
 * Returns: the expected total size of the downloaded file
 *
 * Since: 1.1.2
 */
guint64 webkit_download_get_total_size(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), 0);

    WebKitDownloadPrivate* priv = download->priv;
    if (!priv->networkResponse)
        return 0;

    return MAX(priv->currentSize, priv->networkResponse->expectedContentLength());
}

/**
 * webkit_download_get_current_size:
 * @download: the #WebKitDownload
 *
 * Current already downloaded size.
 *
 * Returns: the already downloaded size
 *
 * Since: 1.1.2
 */
guint64 webkit_download_get_current_size(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), 0);

    WebKitDownloadPrivate* priv = download->priv;
    return priv->currentSize;
}

/**
 * webkit_download_get_progress:
 * @download: a #WebKitDownload
 *
 * Determines the current progress of the download.
 *
 * Returns: a #gdouble ranging from 0.0 to 1.0.
 *
 * Since: 1.1.2
 */
gdouble webkit_download_get_progress(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), 1.0);

    WebKitDownloadPrivate* priv = download->priv;
    gdouble total_size = (gdouble)priv->networkResponse->expectedContentLength();

    if (total_size == 0)
        return 1.0;

    return ((gdouble)priv->currentSize) / total_size;
}

/**
 * webkit_download_get_elapsed_time:
 * @download: a #WebKitDownload
 *
 * Elapsed time for the download in seconds, including any fractional
 * part. If the download is finished, had an error or was cancelled
 * this is the time between its start and the event.
 *
 * Returns: seconds since the download was started, as a #gdouble
 *
 * Since: 1.1.2
 */
gdouble webkit_download_get_elapsed_time(WebKitDownload* download)
{
    g_return_val_if_fail(WEBKIT_IS_DOWNLOAD(download), 0.0);

    WebKitDownloadPrivate* priv = download->priv;
    return g_timer_elapsed(priv->timer, NULL);
}

static void webkit_download_received_data(WebKitDownload* download, const gchar* data, int length)
{
    WebKitDownloadPrivate* priv = download->priv;

    if (priv->currentSize == 0)
        priv->state = WEBKIT_DOWNLOAD_STATE_STARTED;

    ASSERT(priv->outputStream);

    gsize bytes_written;
    GError* error = NULL;

    g_output_stream_write_all(G_OUTPUT_STREAM(priv->outputStream),
                              data, length, &bytes_written, NULL, &error);

    if (error) {
        gboolean handled;
        g_signal_emit_by_name(download, "error", 0, WEBKIT_DOWNLOAD_ERROR_DESTINATION, error->message, &handled);
        g_error_free(error);
        return;
    }

    priv->currentSize += length;
    g_object_notify(G_OBJECT(download), "current-size");

    ASSERT(priv->networkResponse);
    if (priv->currentSize > priv->networkResponse->expectedContentLength())
        g_object_notify(G_OBJECT(download), "total-size");

    // FIXME: Throttle the number of updates? Should we remove the
    // previous g_object_notify()s if we are going to throttle the
    // progress updates?
    g_object_notify(G_OBJECT(download), "progress");
}

static void webkit_download_finished_loading(WebKitDownload* download)
{
    webkit_download_close_stream(download);

    WebKitDownloadPrivate* priv = download->priv;

    g_timer_stop(priv->timer);
    priv->state = WEBKIT_DOWNLOAD_STATE_FINISHED;

    g_object_notify(G_OBJECT(download), "progress");
}

static void webkit_download_error(WebKitDownload* download, const ResourceError& error)
{
    webkit_download_close_stream(download);

    WebKitDownloadPrivate* priv = download->priv;

    g_timer_stop(priv->timer);
    priv->state = WEBKIT_DOWNLOAD_STATE_ERROR;

    gboolean handled;
    g_signal_emit_by_name(download, "error", 0, WEBKIT_DOWNLOAD_ERROR_NETWORK, error.localizedDescription().utf8().data(), &handled);
}

DownloadClient::DownloadClient(WebKitDownload* download)
    : m_download(download)
{
}

void DownloadClient::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    webkit_download_set_response(m_download, response);
}

void DownloadClient::didReceiveData(ResourceHandle*, const char* data, int length, int lengthReceived)
{
    webkit_download_received_data(m_download, data, length);
}

void DownloadClient::didFinishLoading(ResourceHandle*)
{
    webkit_download_finished_loading(m_download);
}

void DownloadClient::didFail(ResourceHandle*, const ResourceError& error)
{
    webkit_download_error(m_download, error);
}

void DownloadClient::wasBlocked(ResourceHandle*)
{
    // FIXME: Implement this when we have the new frame loader signals
    // and error handling.
    notImplemented();
}

void DownloadClient::cannotShowURL(ResourceHandle*)
{
    // FIXME: Implement this when we have the new frame loader signals
    // and error handling.
    notImplemented();
}

}
