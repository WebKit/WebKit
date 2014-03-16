/*
 * Copyright (C) 2012 Igalia S.L.
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

#ifndef WebSoupRequestManager_h
#define WebSoupRequestManager_h

#include "DataReference.h"
#include "MessageReceiver.h"
#include "WebProcessSupplement.h"
#include <WebCore/ResourceError.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GInputStream GInputStream;
typedef struct _GTask GTask;

namespace WebKit {

class WebProcess;
struct WebSoupRequestAsyncData;

class WebSoupRequestManager : public WebProcessSupplement, private IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebSoupRequestManager);
public:
    explicit WebSoupRequestManager(WebProcess*);
    ~WebSoupRequestManager();

    static const char* supplementName();

    void send(GTask*);
    GInputStream* finish(GTask*, GError**);

    void registerURIScheme(const String& scheme);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    void didHandleURIRequest(const IPC::DataReference&, uint64_t contentLength, const String& mimeType, uint64_t requestID);
    void didReceiveURIRequestData(const IPC::DataReference&, uint64_t requestID);
    void didFailURIRequest(const WebCore::ResourceError&, uint64_t requestID);

    WebProcess* m_process;
    GRefPtr<GPtrArray> m_schemes;
    HashMap<uint64_t, std::unique_ptr<WebSoupRequestAsyncData> > m_requestMap;
};

} // namespace WebKit

#endif
