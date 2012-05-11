/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebKitPlatformSupport_h
#define WebKitPlatformSupport_h

#include "../WebIDBKeyPath.h" // FIXME: Remove with: http://webkit.org/b/84207
#include "WebCommon.h"
#include "WebGraphicsContext3D.h"
#include "WebLocalizedString.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebVector.h"
#include <time.h>
#include "../../../../Platform/chromium/public/Platform.h"

#ifdef WIN32
typedef void *HANDLE;
#endif

namespace WebKit {

class WebApplicationCacheHost; // FIXME: Does this belong in platform?
class WebApplicationCacheHostClient; // FIXME: Does this belong in platform?
class WebBlobRegistry;
class WebCookieJar;
class WebFileUtilities;
class WebIDBFactory; // FIXME: Does this belong in platform?
class WebIDBKey; // FIXME: Does this belong in platform?
class WebMessagePortChannel; // FIXME: Does this belong in platform?
class WebPluginListBuilder; // FIXME: Does this belong in platform?
class WebSandboxSupport;
class WebSharedWorkerRepository; // FIXME: Does this belong in platform?
class WebThemeEngine;
class WebWorkerRunLoop;

// FIXME: Eventually all these API will need to move to WebKit::Platform.
class WebKitPlatformSupport : public Platform {
public:
    // Must return non-null.
    virtual WebFileUtilities* fileUtilities() { return 0; }

    // May return null if sandbox support is not necessary
    virtual WebSandboxSupport* sandboxSupport() { return 0; }

    // May return null on some platforms.
    virtual WebThemeEngine* themeEngine() { return 0; }

    // May return null.
    virtual WebCookieJar* cookieJar() { return 0; }

    // Blob ----------------------------------------------------------------

    // Must return non-null.
    virtual WebBlobRegistry* blobRegistry() { return 0; }

    // HTML5 Database ------------------------------------------------------

#ifdef WIN32
    typedef HANDLE FileHandle;
#else
    typedef int FileHandle;
#endif

    // Opens a database file; dirHandle should be 0 if the caller does not need
    // a handle to the directory containing this file
    virtual FileHandle databaseOpenFile(
        const WebString& vfsFileName, int desiredFlags) { return FileHandle(); }

    // Deletes a database file and returns the error code
    virtual int databaseDeleteFile(const WebString& vfsFileName, bool syncDir) { return 0; }

    // Returns the attributes of the given database file
    virtual long databaseGetFileAttributes(const WebString& vfsFileName) { return 0; }

    // Returns the size of the given database file
    virtual long long databaseGetFileSize(const WebString& vfsFileName) { return 0; }

    // Returns the space available for the given origin
    virtual long long databaseGetSpaceAvailableForOrigin(const WebKit::WebString& originIdentifier) { return 0; }

    // Indexed Database ----------------------------------------------------

    virtual WebIDBFactory* idbFactory() { return 0; }
    // FIXME: Remove WebString keyPath overload once callers are updated.
    // http://webkit.org/b/84207
    virtual void createIDBKeysFromSerializedValuesAndKeyPath(const WebVector<WebSerializedScriptValue>& values,  const WebString& keyPath, WebVector<WebIDBKey>& keys) { createIDBKeysFromSerializedValuesAndKeyPath(values, WebIDBKeyPath(keyPath), keys); }
    virtual void createIDBKeysFromSerializedValuesAndKeyPath(const WebVector<WebSerializedScriptValue>& values,  const WebIDBKeyPath& keyPath, WebVector<WebIDBKey>& keys) { }
    // FIXME: Remove WebString keyPath overload once callers are updated.
    // http://webkit.org/b/84207
    virtual WebSerializedScriptValue injectIDBKeyIntoSerializedValue(const WebIDBKey& key, const WebSerializedScriptValue& value, const WebString& keyPath) { return injectIDBKeyIntoSerializedValue(key, value, WebIDBKeyPath(keyPath)); }
    virtual WebSerializedScriptValue injectIDBKeyIntoSerializedValue(const WebIDBKey& key, const WebSerializedScriptValue& value, const WebIDBKeyPath& keyPath) { return WebSerializedScriptValue(); }


    // Message Ports -------------------------------------------------------

    // Creates a Message Port Channel. This can be called on any thread.
    // The returned object should only be used on the thread it was created on.
    virtual WebMessagePortChannel* createMessagePortChannel() { return 0; }


    // Plugins -------------------------------------------------------------

    // If refresh is true, then cached information should not be used to
    // satisfy this call.
    virtual void getPluginList(bool refresh, WebPluginListBuilder*) { }


    // Resources -----------------------------------------------------------

    // Returns a localized string resource (with substitution parameters).
    virtual WebString queryLocalizedString(WebLocalizedString::Name) { return WebString(); }
    virtual WebString queryLocalizedString(WebLocalizedString::Name, const WebString& parameter) { return WebString(); }
    virtual WebString queryLocalizedString(WebLocalizedString::Name, const WebString& parameter1, const WebString& parameter2) { return WebString(); }


    // Shared Workers ------------------------------------------------------

    virtual WebSharedWorkerRepository* sharedWorkerRepository() { return 0; }

    // GPU ----------------------------------------------------------------
    //
    // May return null if GPU is not supported.
    // Returns newly allocated and initialized offscreen WebGraphicsContext3D instance.
    virtual WebGraphicsContext3D* createOffscreenGraphicsContext3D(const WebGraphicsContext3D::Attributes&) { return 0; }

    // Returns true if the platform is capable of producing an offscreen context suitable for accelerating 2d canvas.
    // This will return false if the platform cannot promise that contexts will be preserved across operations like
    // locking the screen or if the platform cannot provide a context with suitable performance characteristics.
    //
    // This value must be checked again after a context loss event as the platform's capabilities may have changed.
    virtual bool canAccelerate2dCanvas() { return false; }


    // WebWorker ----------------------------------------------------------

    virtual void didStartWorkerRunLoop(const WebWorkerRunLoop&) { }
    virtual void didStopWorkerRunLoop(const WebWorkerRunLoop&) { }

protected:
    ~WebKitPlatformSupport() { }
};

} // namespace WebKit

#endif
