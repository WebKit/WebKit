/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef CustomProtocolManagerImpl_h
#define CustomProtocolManagerImpl_h

#if ENABLE(CUSTOM_PROTOCOLS)

#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GTask GTask;
typedef struct _GInputStream GInputStream;

namespace IPC {
class DataReference;
} // namespace IPC

namespace WebCore {
class ResourceError;
class ResourceResponse;
} // namespace WebCore

namespace WebKit {

class ChildProcess;
struct WebSoupRequestAsyncData;

class CustomProtocolManagerImpl {
    WTF_MAKE_NONCOPYABLE(CustomProtocolManagerImpl);
public:
    explicit CustomProtocolManagerImpl(ChildProcess*);
    ~CustomProtocolManagerImpl();

    void registerScheme(const String&);
    bool supportsScheme(const String&);

    void didFailWithError(uint64_t customProtocolID, const WebCore::ResourceError&);
    void didLoadData(uint64_t customProtocolID, const IPC::DataReference&);
    void didReceiveResponse(uint64_t customProtocolID, const WebCore::ResourceResponse&);
    void didFinishLoading(uint64_t customProtocolID);

    // SoupRequest implementation.
    void send(GTask*);
    GInputStream* finish(GTask*, GError**);

private:
    ChildProcess* m_childProcess;
    GRefPtr<GPtrArray> m_schemes;
    HashMap<uint64_t, std::unique_ptr<WebSoupRequestAsyncData>> m_customProtocolMap;
};

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)

#endif // CustomProtocolManagerImpl_h
