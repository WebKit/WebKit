/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2009 Igalia S.L.
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
#include "ResourceHandle.h"

#include "Base64.h"
#include "CookieJarSoup.h"
#include "CString.h"
#include "DocLoader.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "TextEncoding.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <libsoup/soup-message.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if PLATFORM(GTK)
    #if GLIB_CHECK_VERSION(2,12,0)
        #define USE_GLIB_BASE64
    #endif
#endif

namespace WebCore {

class WebCoreSynchronousLoader : public ResourceHandleClient, Noncopyable {
public:
    WebCoreSynchronousLoader(ResourceError&, ResourceResponse &, Vector<char>&);
    ~WebCoreSynchronousLoader();

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);

    void run();

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<char>& m_data;
    bool m_finished;
    GMainLoop* m_mainLoop;
};

WebCoreSynchronousLoader::WebCoreSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<char>& data)
    : m_error(error)
    , m_response(response)
    , m_data(data)
    , m_finished(false)
{
    m_mainLoop = g_main_loop_new(NULL, false);
}

WebCoreSynchronousLoader::~WebCoreSynchronousLoader()
{
    g_main_loop_unref(m_mainLoop);
}

void WebCoreSynchronousLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
}

void WebCoreSynchronousLoader::didReceiveData(ResourceHandle*, const char* data, int length, int)
{
    m_data.append(data, length);
}

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*)
{
    g_main_loop_quit(m_mainLoop);
    m_finished = true;
}

void WebCoreSynchronousLoader::didFail(ResourceHandle* handle, const ResourceError& error)
{
    m_error = error;
    didFinishLoading(handle);
}

void WebCoreSynchronousLoader::run()
{
    if (!m_finished)
        g_main_loop_run(m_mainLoop);
}

enum
{
    ERROR_TRANSPORT,
    ERROR_UNKNOWN_PROTOCOL,
    ERROR_BAD_NON_HTTP_METHOD,
    ERROR_UNABLE_TO_OPEN_FILE,
};

struct FileMapping
{
    gpointer ptr;
    gsize length;
};

static void freeFileMapping(gpointer data)
{
    FileMapping* fileMapping = static_cast<FileMapping*>(data);
    if (fileMapping->ptr != MAP_FAILED)
        munmap(fileMapping->ptr, fileMapping->length);
    g_slice_free(FileMapping, fileMapping);
}

static void cleanupGioOperation(ResourceHandleInternal* handle);

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_msg) {
        g_object_unref(m_msg);
        m_msg = 0;
    }

    cleanupGioOperation(this);

    if (m_idleHandler) {
        g_source_remove(m_idleHandler);
        m_idleHandler = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
}

static void fillResponseFromMessage(SoupMessage* msg, ResourceResponse* response)
{
    SoupMessageHeadersIter iter;
    const char* name = NULL;
    const char* value = NULL;
    soup_message_headers_iter_init(&iter, msg->response_headers);
    while (soup_message_headers_iter_next(&iter, &name, &value))
        response->setHTTPHeaderField(name, value);

    String contentType = soup_message_headers_get(msg->response_headers, "Content-Type");
    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
    response->setUrl(KURL(uri));
    g_free(uri);
    response->setMimeType(extractMIMETypeFromMediaType(contentType));
    response->setTextEncodingName(extractCharsetFromMediaType(contentType));
    response->setExpectedContentLength(soup_message_headers_get_content_length(msg->response_headers));
    response->setHTTPStatusCode(msg->status_code);
    response->setSuggestedFilename(filenameFromHTTPContentDisposition(response->httpHeaderField("Content-Disposition")));
}

// Called each time the message is going to be sent again except the first time.
// It's used mostly to let webkit know about redirects.
static void restartedCallback(SoupMessage* msg, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;

    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
    String location = String(uri);
    g_free(uri);
    KURL newURL = KURL(handle->request().url(), location);

    ResourceRequest request = handle->request();
    ResourceResponse response;
    request.setURL(newURL);
    fillResponseFromMessage(msg, &response);
    if (d->client())
        d->client()->willSendRequest(handle, request, response);

    d->m_request.setURL(newURL);
}

static void gotHeadersCallback(SoupMessage* msg, gpointer data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        return;

    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    fillResponseFromMessage(msg, &d->m_response);
    client->didReceiveResponse(handle, d->m_response);
    soup_message_set_flags(msg, SOUP_MESSAGE_OVERWRITE_CHUNKS);
}

static void gotChunkCallback(SoupMessage* msg, SoupBuffer* chunk, gpointer data)
{
    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        return;

    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    if (!handle)
        return;
    ResourceHandleInternal* d = handle->getInternal();
    if (d->m_cancelled)
        return;
    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    client->didReceiveData(handle, chunk->data, chunk->length, false);
}

// Called at the end of the message, with all the necessary about the last informations.
// Doesn't get called for redirects.
static void finishedCallback(SoupSession *session, SoupMessage* msg, gpointer data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(data);
    // TODO: maybe we should run this code even if there's no client?
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();

    ResourceHandleClient* client = handle->client();
    if (!client)
        return;

    if (d->m_cancelled)
        return;

    if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code)) {
        char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
        ResourceError error("webkit-network-error", ERROR_TRANSPORT, uri, String::fromUTF8(msg->reason_phrase));
        g_free(uri);
        client->didFail(handle, error);
        return;
    } else if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
        fillResponseFromMessage(msg, &d->m_response);
        client->didReceiveResponse(handle, d->m_response);

        // WebCore might have cancelled the job in the while
        if (d->m_cancelled)
            return;

        if (msg->response_body->data)
            client->didReceiveData(handle, msg->response_body->data, msg->response_body->length, true);
    }

    client->didFinishLoading(handle);
}

// parseDataUrl() is taken from the CURL http backend.
static gboolean parseDataUrl(gpointer callback_data)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(callback_data);
    ResourceHandleClient* client = handle->client();

    handle->getInternal()->m_idleHandler = 0;

    ASSERT(client);
    if (!client)
        return FALSE;

    String url = handle->request().url().string();
    ASSERT(url.startsWith("data:", false));

    int index = url.find(',');
    if (index == -1) {
        client->cannotShowURL(handle);
        return FALSE;
    }

    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);

    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);

    ResourceResponse response;
    response.setMimeType(mimeType);

    if (base64) {
        data = decodeURLEscapeSequences(data);
        response.setTextEncodingName(charset);
        client->didReceiveResponse(handle, response);

        // Use the GLib Base64 if available, since WebCore's decoder isn't
        // general-purpose and fails on Acid3 test 97 (whitespace).
#ifdef USE_GLIB_BASE64
        size_t outLength = 0;
        char* outData = 0;
        outData = reinterpret_cast<char*>(g_base64_decode(data.utf8().data(), &outLength));
        if (outData && outLength > 0)
            client->didReceiveData(handle, outData, outLength, 0);
        g_free(outData);
#else
        Vector<char> out;
        if (base64Decode(data.latin1().data(), data.latin1().length(), out) && out.size() > 0)
            client->didReceiveData(handle, out.data(), out.size(), 0);
#endif
    } else {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        client->didReceiveResponse(handle, response);
        if (data.length() > 0)
            client->didReceiveData(handle, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
    }

    client->didFinishLoading(handle);

    return FALSE;
}

bool ResourceHandle::startData(String urlString)
{
    ResourceHandleInternal* d = this->getInternal();

    // If parseDataUrl is called synchronously the job is not yet effectively started
    // and webkit won't never know that the data has been parsed even didFinishLoading is called.
    d->m_idleHandler = g_idle_add(parseDataUrl, this);
    return true;
}

static SoupSession* createSoupSession()
{
    return soup_session_async_new();
}

static void ensureSessionIsInitialized(SoupSession* session)
{
    if (g_object_get_data(G_OBJECT(session), "webkit-init"))
        return;
 
    SoupCookieJar* jar = reinterpret_cast<SoupCookieJar*>(soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR));
    if (!jar)
        soup_session_add_feature(session, SOUP_SESSION_FEATURE(defaultCookieJar()));
    else
        setDefaultCookieJar(jar);

    const char* webkit_debug = g_getenv("WEBKIT_DEBUG"); 
    if (!soup_session_get_feature(session, SOUP_TYPE_LOGGER)
        && webkit_debug && !strcmp(webkit_debug, "network")) {
        SoupLogger* logger = soup_logger_new(static_cast<SoupLoggerLogLevel>(SOUP_LOGGER_LOG_BODY), -1);
        soup_logger_attach(logger, session);
        g_object_unref(logger);
    }
 
    g_object_set_data(G_OBJECT(session), "webkit-init", reinterpret_cast<void*>(0xdeadbeef));
}

bool ResourceHandle::startHttp(String urlString)
{
    SoupSession* session = defaultSession();
    ensureSessionIsInitialized(session);

    SoupMessage* msg;
    msg = soup_message_new(request().httpMethod().utf8().data(), urlString.utf8().data());
    g_signal_connect(msg, "restarted", G_CALLBACK(restartedCallback), this);

    g_signal_connect(msg, "got-headers", G_CALLBACK(gotHeadersCallback), this);
    g_signal_connect(msg, "got-chunk", G_CALLBACK(gotChunkCallback), this);

    HTTPHeaderMap customHeaders = d->m_request.httpHeaderFields();
    if (!customHeaders.isEmpty()) {
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it)
            soup_message_headers_append(msg->request_headers, it->first.string().utf8().data(), it->second.utf8().data());
    }

    FormData* httpBody = d->m_request.httpBody();
    if (httpBody && !httpBody->isEmpty()) {
        size_t numElements = httpBody->elements().size();

        // handle the most common case (i.e. no file upload)
        if (numElements < 2) {
            Vector<char> body;
            httpBody->flatten(body);
            soup_message_set_request(msg, d->m_request.httpContentType().utf8().data(),
                                     SOUP_MEMORY_COPY, body.data(), body.size());
        } else {
            /*
             * we have more than one element to upload, and some may
             * be (big) files, which we will want to mmap instead of
             * copying into memory; TODO: support upload of non-local
             * (think sftp://) files by using GIO?
             *
             * TODO: we can avoid appending all the buffers to the
             * request_body variable with the following call, but we
             * need to depend on libsoup > 2.25.4
             *
             * soup_message_body_set_accumulate(msg->request_body, FALSE);
             */
            for (size_t i = 0; i < numElements; i++) {
                const FormDataElement& element = httpBody->elements()[i];

                if (element.m_type == FormDataElement::data)
                    soup_message_body_append(msg->request_body, SOUP_MEMORY_TEMPORARY, element.m_data.data(), element.m_data.size());
                else {
                    /*
                     * mapping for uploaded files code inspired by technique used in
                     * libsoup's simple-httpd test
                     */
                    /* FIXME: Since Linux 2.6.23 we should also use O_CLOEXEC */
                    int fd = open(element.m_filename.utf8().data(), O_RDONLY);

                    if (fd == -1) {
                        ResourceError error("webkit-network-error", ERROR_UNABLE_TO_OPEN_FILE, urlString, strerror(errno));
                        d->client()->didFail(this, error);
                        g_object_unref(msg);
                        return false;
                    }

                    struct stat statBuf;
                    fstat(fd, &statBuf);

                    FileMapping* fileMapping = g_slice_new(FileMapping);

                    fileMapping->ptr = mmap(NULL, statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                    if (fileMapping->ptr == MAP_FAILED) {
                        ResourceError error("webkit-network-error", ERROR_UNABLE_TO_OPEN_FILE, urlString, strerror(errno));
                        d->client()->didFail(this, error);
                        freeFileMapping(fileMapping);
                        g_object_unref(msg);
                        close(fd);
                        return false;
                    }
                    fileMapping->length = statBuf.st_size;

                    close(fd);

                    SoupBuffer* soupBuffer = soup_buffer_new_with_owner(fileMapping->ptr, fileMapping->length, fileMapping, freeFileMapping);
                    soup_message_body_append_buffer(msg->request_body, soupBuffer);
                    soup_buffer_free(soupBuffer);
                }
            }
        }
    }

    d->m_msg = static_cast<SoupMessage*>(g_object_ref(msg));
    soup_session_queue_message(session, d->m_msg, finishedCallback, this);

    return true;
}

bool ResourceHandle::start(Frame* frame)
{
    ASSERT(!d->m_msg);

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;

    KURL url = request().url();
    String urlString = url.string();
    String protocol = url.protocol();

    if (equalIgnoringCase(protocol, "data"))
        return startData(urlString);
    else if ((equalIgnoringCase(protocol, "http") || equalIgnoringCase(protocol, "https")) && SOUP_URI_VALID_FOR_HTTP(soup_uri_new(urlString.utf8().data())))
        return startHttp(urlString);
    else if (equalIgnoringCase(protocol, "file") || equalIgnoringCase(protocol, "ftp") || equalIgnoringCase(protocol, "ftps"))
        // FIXME: should we be doing any other protocols here?
        return startGio(url);
    else {
        // If we don't call didFail the job is not complete for webkit even false is returned.
        if (d->client()) {
            ResourceError error("webkit-network-error", ERROR_UNKNOWN_PROTOCOL, urlString, protocol);
            d->client()->didFail(this, error);
        }
        return false;
    }
}

void ResourceHandle::cancel()
{
    d->m_cancelled = true;
    if (d->m_msg) {
        soup_session_cancel_message(defaultSession(), d->m_msg, SOUP_STATUS_CANCELLED);
        // For re-entrancy troubles we call didFinishLoading when the message hasn't been handled yet.
        if (client())
            client()->didFinishLoading(this);
    } else if (d->m_cancellable) {
        g_cancellable_cancel(d->m_cancellable);
        if (client())
            client()->didFinishLoading(this);
    }
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
    notImplemented();
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest&)
{
    // Not having this function means that we'll ask the user about re-posting a form
    // even when we go back to a page that's still in the cache.
    notImplemented();
    return false;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame* frame)
{
    WebCoreSynchronousLoader syncLoader(error, response, data);
    ResourceHandle handle(request, &syncLoader, true, false, true);

    handle.start(frame);
    syncLoader.run();
}

// GIO-based loader

static inline ResourceError networkErrorForFile(GFile* file, GError* error)
{
    // FIXME: Map gio errors to a more detailed error code when we have it in WebKit.
    gchar* uri = g_file_get_uri(file);
    ResourceError resourceError("webkit-network-error", ERROR_TRANSPORT, uri, String::fromUTF8(error->message));
    g_free(uri);
    return resourceError;
}

static void cleanupGioOperation(ResourceHandleInternal* d)
{
    if (d->m_gfile) {
        g_object_set_data(G_OBJECT(d->m_gfile), "webkit-resource", 0);
        g_object_unref(d->m_gfile);
        d->m_gfile = NULL;
    }
    if (d->m_cancellable) {
        g_object_unref(d->m_cancellable);
        d->m_cancellable = NULL;
    }
    if (d->m_input_stream) {
        g_object_set_data(G_OBJECT(d->m_input_stream), "webkit-resource", 0);
        g_object_unref(d->m_input_stream);
        d->m_input_stream = NULL;
    }
    if (d->m_buffer) {
        g_free(d->m_buffer);
        d->m_buffer = NULL;
    }
}

static void closeCallback(GObject* source, GAsyncResult* res, gpointer)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    g_input_stream_close_finish(d->m_input_stream, res, NULL);
    cleanupGioOperation(d);
    client->didFinishLoading(handle);
}

static void readCallback(GObject* source, GAsyncResult* res, gpointer)
{
    // didReceiveData may cancel the load, which may release the last reference.
    RefPtr<ResourceHandle> handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(d);
        return;
    }

    gssize nread;
    GError *error = 0;

    nread = g_input_stream_read_finish(d->m_input_stream, res, &error);
    if (error) {
        ResourceError resourceError = networkErrorForFile(d->m_gfile, error);
        cleanupGioOperation(d);
        client->didFail(handle.get(), resourceError);
        return;
    } else if (!nread) {
        g_input_stream_close_async(d->m_input_stream, G_PRIORITY_DEFAULT,
                                   NULL, closeCallback, NULL);
        return;
    }

    d->m_total += nread;
    client->didReceiveData(handle.get(), d->m_buffer, nread, d->m_total);

    if (d->m_cancelled) {
        cleanupGioOperation(d);
        return;
    }

    g_input_stream_read_async(d->m_input_stream, d->m_buffer, d->m_bufsize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, NULL);
}

static void openCallback(GObject* source, GAsyncResult* res, gpointer)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled || !client) {
        cleanupGioOperation(d);
        return;
    }

    GFileInputStream* in;
    GError *error = NULL;
    in = g_file_read_finish(G_FILE(source), res, &error);
    if (error) {
        ResourceError resourceError = networkErrorForFile(d->m_gfile, error);
        cleanupGioOperation(d);
        client->didFail(handle, resourceError);
        return;
    }

    d->m_input_stream = G_INPUT_STREAM(in);
    d->m_bufsize = 8192;
    d->m_buffer = static_cast<char*>(g_malloc(d->m_bufsize));
    d->m_total = 0;
    g_object_set_data(G_OBJECT(d->m_input_stream), "webkit-resource", handle);
    g_input_stream_read_async(d->m_input_stream, d->m_buffer, d->m_bufsize,
                              G_PRIORITY_DEFAULT, d->m_cancellable,
                              readCallback, NULL);
}

static void queryInfoCallback(GObject* source, GAsyncResult* res, gpointer)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(g_object_get_data(source, "webkit-resource"));
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    ResourceHandleClient* client = handle->client();

    if (d->m_cancelled) {
        cleanupGioOperation(d);
        return;
    }

    ResourceResponse response;

    char* uri = g_file_get_uri(d->m_gfile);
    response.setUrl(KURL(uri));
    g_free(uri);

    GError *error = NULL;
    GFileInfo* info = g_file_query_info_finish(d->m_gfile, res, &error);

    if (error) {
        // FIXME: to be able to handle ftp URIs properly, we must
        // check if the error is G_IO_ERROR_NOT_MOUNTED, and if so,
        // call g_file_mount_enclosing_volume() to mount the ftp
        // server (and then keep track of the fact that we mounted it,
        // and set a timeout to unmount it later after it's been idle
        // for a while).

        ResourceError resourceError = networkErrorForFile(d->m_gfile, error);
        cleanupGioOperation(d);
        client->didFail(handle, resourceError);
        return;
    }

    if (g_file_info_get_file_type(info) != G_FILE_TYPE_REGULAR) {
        // FIXME: what if the URI points to a directory? Should we
        // generate a listing? How? What do other backends do here?

        ResourceError resourceError = networkErrorForFile(d->m_gfile, error);
        cleanupGioOperation(d);
        client->didFail(handle, resourceError);
        return;
    }

    response.setMimeType(g_file_info_get_content_type(info));
    response.setExpectedContentLength(g_file_info_get_size(info));

    GTimeVal tv;
    g_file_info_get_modification_time(info, &tv);
    response.setLastModifiedDate(tv.tv_sec);

    client->didReceiveResponse(handle, response);

    g_file_read_async(d->m_gfile, G_PRIORITY_DEFAULT, d->m_cancellable,
                      openCallback, NULL);
}

bool ResourceHandle::startGio(KURL url)
{
    if (request().httpMethod() != "GET" && request().httpMethod() != "POST") {
        ResourceError error("webkit-network-error", ERROR_BAD_NON_HTTP_METHOD, url.string(), request().httpMethod());
        d->client()->didFail(this, error);
        return false;
    }

    // GIO doesn't know how to handle refs and queries, so remove them
    // TODO: use KURL.fileSystemPath after KURLGtk and FileSystemGtk are
    // using GIO internally, and providing URIs instead of file paths
    url.removeRef();
    url.setQuery(String());
    url.setPort(0);

    // we avoid the escaping for local files, because
    // g_filename_from_uri (used internally by GFile) has problems
    // decoding strings with arbitrary percent signs
    if (url.isLocalFile())
        d->m_gfile = g_file_new_for_path(url.prettyURL().utf8().data() + sizeof("file://") - 1);
    else
        d->m_gfile = g_file_new_for_uri(url.string().utf8().data());
    g_object_set_data(G_OBJECT(d->m_gfile), "webkit-resource", this);
    d->m_cancellable = g_cancellable_new();
    g_file_query_info_async(d->m_gfile,
                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT, d->m_cancellable,
                            queryInfoCallback, NULL);
    return true;
}

SoupSession* ResourceHandle::defaultSession()
{
    static SoupSession* session = createSoupSession();;

    return session;
}

}

